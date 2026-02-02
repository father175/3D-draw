// prepare.cpp - 主渲染循环和光线追踪核心逻辑

#include <graphics.h>
#include <conio.h>
#include <Windows.h>
#include "vector.h"
#include "add_trangle.h"
#include "bvh.h"
#include <omp.h> 
using namespace std;

// 场景相交测试（使用BVH加速）
bool intersectScene(Ray ray, HitRecord& hit) {
    hit.t = 1e9;
    hit.hit = false;
    
    if (bvhRoot != nullptr) {
        intersectBVH(bvhRoot, ray, hit);
    } else {
        // 回退到逐个测试
        for (int i = 0; i < triangleCount; i++) {
            intersectTriangle(ray, triangles[i], hit);
        }
    }
    
    return hit.hit;
}
COLORREF traceRay(Ray ray, int depth);

// 修改calculateDiffuseLighting函数，添加对透明度的考虑
COLORREF calculateDiffuseLighting(HitRecord& hit, COLORREF surfaceColor) {
    // 基础环境光
    double ambient = 0.1;
    
    // 提取表面颜色分量
    double sr = GetRValue(surfaceColor) / 255.0;
    double sg = GetGValue(surfaceColor) / 255.0;
    double sb = GetBValue(surfaceColor) / 255.0;
    
    // 初始化最终颜色
    double r = sr * ambient;
    double g = sg * ambient;
    double b = sb * ambient;
    
    // 遍历所有点光源
    for (int i = 0; i < pointLightCount; i++) {
        PointLight light = pointLights[i];
        
        // 计算光线方向
        double lightDir[3];
        lightDir[0] = light.position[0] - hit.position[0];
        lightDir[1] = light.position[1] - hit.position[1];
        lightDir[2] = light.position[2] - hit.position[2];
        
        double distance = sqrt(lightDir[0]*lightDir[0] + 
                              lightDir[1]*lightDir[1] + 
                              lightDir[2]*lightDir[2]);
        
        // 归一化光线方向
        if (distance > 0) {
            lightDir[0] /= distance;
            lightDir[1] /= distance;
            lightDir[2] /= distance;
        }
        
        // 计算漫反射系数（兰伯特余弦定律）
        double diffuse = max(0.0, dot(hit.normal, lightDir));
        
        // 计算衰减（包含阴影和软阴影）
        double attenuation = calculateAttenuation(distance, light.intensity, 
                                                 light.position, hit.position, 
                                                 light.radius);
        
        // 如果光线被完全遮挡，跳过此光源
        if (attenuation <= 0.0) continue;
        
        // 累加光照贡献
        r += sr * diffuse * light.color[0] * attenuation;
        g += sg * diffuse * light.color[1] * attenuation;
        b += sb * diffuse * light.color[2] * attenuation;
    }
    
    // 限制颜色值在0-1范围内
    r = min(max(r, 0.0), 1.0);
    g = min(max(g, 0.0), 1.0);
    b = min(max(b, 0.0), 1.0);
    
    // 返回RGB颜色
    return RGB(r * 255, g * 255, b * 255);
}
// 新增函数：处理半透明像素（直接穿过并叠加颜色）
COLORREF processTransparentPixel(Ray ray, int depth, HitRecord& hit, 
                                COLORREF surfaceColor, BYTE alpha) {
    // 计算透明度比例 (0-1)
    double transparency = alpha / 255.0;
    
    // 1. 计算当前表面的颜色（考虑光照）
    COLORREF currentSurfaceColor;
    if (hit.materialType == 1) {
        // 漫反射材质，计算光照
        currentSurfaceColor = calculateDiffuseLighting(hit, surfaceColor);
    } else {
        currentSurfaceColor = surfaceColor;
    }
    
    // 2. 创建透射光线（继续沿着原方向前进）
    Ray transmissionRay = ray;
    
    // 轻微偏移起点，避免自相交
    transmissionRay.origin[0] = hit.position[0] + ray.direction[0] * 0.001;
    transmissionRay.origin[1] = hit.position[1] + ray.direction[1] * 0.001;
    transmissionRay.origin[2] = hit.position[2] + ray.direction[2] * 0.001;
    
    // 方向不变（不考虑折射）
    transmissionRay.direction[0] = ray.direction[0];
    transmissionRay.direction[1] = ray.direction[1];
    transmissionRay.direction[2] = ray.direction[2];
    
    // 3. 追踪透射光线
    COLORREF transmittedColor = traceRay(transmissionRay, depth + 1);
    
    // 4. 颜色混合：当前表面颜色 + 透射颜色
    // 使用透明度进行线性混合
    double blendFactor = transparency;
    
    int r = GetRValue(currentSurfaceColor) * blendFactor + 
            GetRValue(transmittedColor) * (1.0 - blendFactor);
    int g = GetGValue(currentSurfaceColor) * blendFactor + 
            GetGValue(transmittedColor) * (1.0 - blendFactor);
    int b = GetBValue(currentSurfaceColor) * blendFactor + 
            GetBValue(transmittedColor) * (1.0 - blendFactor);
    
    // 限制颜色范围
    r = max(0, min(255, r));
    g = max(0, min(255, g));
    b = max(0, min(255, b));
    
    return RGB(r, g, b);
}
// 新增函数：处理半透明材质（同时计算反射和透射）
COLORREF processTranslucentMaterial(Ray ray, int depth, HitRecord& hit, 
                                   COLORREF surfaceColor) {
    // 计算反射部分
    Ray reflectedRay;
    reflectedRay.origin[0] = hit.position[0] + hit.normal[0] * 0.001;
    reflectedRay.origin[1] = hit.position[1] + hit.normal[1] * 0.001;
    reflectedRay.origin[2] = hit.position[2] + hit.normal[2] * 0.001;
    
    // 计算反射方向
    double dotProduct = dot(ray.direction, hit.normal);
    reflectedRay.direction[0] = ray.direction[0] - 2.0 * dotProduct * hit.normal[0];
    reflectedRay.direction[1] = ray.direction[1] - 2.0 * dotProduct * hit.normal[1];
    reflectedRay.direction[2] = ray.direction[2] - 2.0 * dotProduct * hit.normal[2];
    normalize(reflectedRay.direction);
    
    COLORREF reflectedColor = traceRay(reflectedRay, depth + 1);
    
    // 计算透射部分（折射）
    Ray refractedRay;
    refractedRay.origin[0] = hit.position[0] - hit.normal[0] * 0.001;
    refractedRay.origin[1] = hit.position[1] - hit.normal[1] * 0.001;
    refractedRay.origin[2] = hit.position[2] - hit.normal[2] * 0.001;
    
    // 简单折射：方向稍微弯曲
    double refractionFactor = 0.7; // 折射率因子
    refractedRay.direction[0] = ray.direction[0] * refractionFactor + 
                                hit.normal[0] * (1.0 - refractionFactor);
    refractedRay.direction[1] = ray.direction[1] * refractionFactor + 
                                hit.normal[1] * (1.0 - refractionFactor);
    refractedRay.direction[2] = ray.direction[2] * refractionFactor + 
                                hit.normal[2] * (1.0 - refractionFactor);
    normalize(refractedRay.direction);
    
    COLORREF refractedColor = traceRay(refractedRay, depth + 1);
    
    // 计算漫反射颜色（如果有）
    COLORREF diffuseColor = calculateDiffuseLighting(hit, surfaceColor);
    
    // 混合：50%反射 + 30%折射 + 20%漫反射
    int r = (int)(GetRValue(reflectedColor) * 0.5 + 
                  GetRValue(refractedColor) * 0.3 + 
                  GetRValue(diffuseColor) * 0.2);
    int g = (int)(GetGValue(reflectedColor) * 0.5 + 
                  GetGValue(refractedColor) * 0.3 + 
                  GetGValue(diffuseColor) * 0.2);
    int b = (int)(GetBValue(reflectedColor) * 0.5 + 
                  GetBValue(refractedColor) * 0.3 + 
                  GetBValue(diffuseColor) * 0.2);
    
    r = max(0, min(255, r));
    g = max(0, min(255, g));
    b = max(0, min(255, b));
    
    return RGB(r, g, b);
}

// 新增函数：处理镜面反射材质
COLORREF processMirrorReflection(Ray ray, int depth, HitRecord& hit, 
                                COLORREF surfaceColor) {
    Ray reflectedRay;
    reflectedRay.origin[0] = hit.position[0] + hit.normal[0] * 0.001;
    reflectedRay.origin[1] = hit.position[1] + hit.normal[1] * 0.001;
    reflectedRay.origin[2] = hit.position[2] + hit.normal[2] * 0.001;
    
    // 计算完美的反射方向
    double dotProduct = dot(ray.direction, hit.normal);
    reflectedRay.direction[0] = ray.direction[0] - 2.0 * dotProduct * hit.normal[0];
    reflectedRay.direction[1] = ray.direction[1] - 2.0 * dotProduct * hit.normal[1];
    reflectedRay.direction[2] = ray.direction[2] - 2.0 * dotProduct * hit.normal[2];
    normalize(reflectedRay.direction);
    
    COLORREF reflectedColor = traceRay(reflectedRay, depth + 1);
    
    // 镜面材质可以混合一点点表面颜色
    int r = (GetRValue(reflectedColor) * 9 + GetRValue(surfaceColor)) / 10;
    int g = (GetGValue(reflectedColor) * 9 + GetGValue(surfaceColor)) / 10;
    int b = (GetBValue(reflectedColor) * 9 + GetBValue(surfaceColor)) / 10;
    
    return RGB(r, g, b);
}

// prepare.cpp - 修改traceRay函数中的半透明处理

COLORREF traceRay(Ray ray, int depth) {
    const int MAX_DEPTH = 5;
    
    // 递归深度限制
    if (depth > MAX_DEPTH) {
        return RGB(0, 0, 0); // 返回黑色
    }
    
    HitRecord hit;
    if (!intersectScene(ray, hit)) {
        // 没有命中任何物体，返回背景色
        return RGB(100, 100, 150); // 淡蓝色天空
    }
    
    // 获取表面颜色和透明度
    COLORREF surfaceColor = hit.color;
    BYTE alpha = 255; // 默认不透明
    
    // 如果有纹理，采样纹理并获取透明度
    if (hit.hasTexture) {
        TextureData* tex = findTexture(hit.texturePath);
        if (tex) {
            BYTE texAlpha;
            surfaceColor = sampleTexture(*tex, hit.tex_u, hit.tex_v, texAlpha);
            alpha = texAlpha; // 使用纹理的透明度
        }
    }
    
    // 根据材质类型和透明度处理
    if (alpha < 250) {
        // 半透明像素：直接穿过并叠加颜色
        return processTransparentPixel(ray, depth, hit, surfaceColor, alpha);
    }
    else if (hit.materialType == 1) {
        // 漫反射材质
        return calculateDiffuseLighting(hit, surfaceColor);
    }
    else if (hit.materialType == 2) {
        // 半透明材质 - 同时计算反射和透射
        return processTranslucentMaterial(ray, depth, hit, surfaceColor);
    }
    else if (hit.materialType == 3) {
        // 完全镜面反射材质
        return processMirrorReflection(ray, depth, hit, surfaceColor);
    }
    
    return surfaceColor;
}


// 生成相机光线（透视投影）
Ray generateRay(int x, int y) {
    Ray ray;
    ray.origin[0] = camera.x;
    ray.origin[1] = camera.y;
    ray.origin[2] = camera.z;
    
    double aspect = (double)WIDTH / HEIGHT;
    double fov = PI / 3.0;
    
    // 标准化设备坐标
    double screenX = (2.0 * (x + 0.5) / WIDTH - 1.0) * aspect * tan(fov / 2.0);
    double screenY = (1.0 - 2.0 * (HEIGHT-y + 0.5) / HEIGHT) * tan(fov / 2.0);
    
    double dir[3] = {screenX, screenY, -1.0};
    normalize(dir);
    
    // 相机坐标系到世界坐标系的变换
    double forward[3] = {
        sin(camera.yaw) * cos(camera.pitch),
        sin(camera.pitch),
        -cos(camera.yaw) * cos(camera.pitch)
    };
    
    double right[3] = {
        cos(camera.yaw),
        0,
        sin(camera.yaw)
    };
    
    double up[3];
    cross(forward, right, up);
    normalize(up);
    
    cross(up, forward, right);
    normalize(right);
    
    double worldDir[3] = {
        dir[0] * right[0] + dir[1] * up[0] + dir[2] * forward[0],
        dir[0] * right[1] + dir[1] * up[1] + dir[2] * forward[1],
        dir[0] * right[2] + dir[1] * up[2] + dir[2] * forward[2]
    };
    
    ray.direction[0] = worldDir[0];
    ray.direction[1] = worldDir[1];
    ray.direction[2] = worldDir[2];
    normalize(ray.direction);
    
    return ray;
}

// 绘制准星
void drawCrosshair() {
    setcolor(BLACK);
    setlinestyle(PS_SOLID, 2);
    settextcolor(WHITE);
    line(WIDTH / 2 - 10, HEIGHT / 2, WIDTH / 2 + 10, HEIGHT / 2);
    line(WIDTH / 2, HEIGHT / 2 - 10, WIDTH / 2, HEIGHT / 2 + 10);
}


// 新增函数：更精确的光源采样（用于直接照明）
COLORREF sampleLightSource(PointLight& light, HitRecord& hit) {
    COLORREF result = RGB(0, 0, 0);
    
    if (light.radius <= 0.0) {
        // 点光源：直接计算
        double lightDir[3];
        lightDir[0] = light.position[0] - hit.position[0];
        lightDir[1] = light.position[1] - hit.position[1];
        lightDir[2] = light.position[2] - hit.position[2];
        
        double distance = sqrt(lightDir[0]*lightDir[0] + 
                             lightDir[1]*lightDir[1] + 
                             lightDir[2]*lightDir[2]);
        
        if (distance > 0) {
            lightDir[0] /= distance;
            lightDir[1] /= distance;
            lightDir[2] /= distance;
        }
        
        double cosTheta = max(0.0, dot(hit.normal, lightDir));
        
        // 阴影测试
        Ray shadowRay;
        shadowRay.origin[0] = hit.position[0];
        shadowRay.origin[1] = hit.position[1];
        shadowRay.origin[2] = hit.position[2];
        shadowRay.direction[0] = lightDir[0];
        shadowRay.direction[1] = lightDir[1];
        shadowRay.direction[2] = lightDir[2];
        
        // 偏移起点
        shadowRay.origin[0] += lightDir[0] * 0.001;
        shadowRay.origin[1] += lightDir[1] * 0.001;
        shadowRay.origin[2] += lightDir[2] * 0.001;
        
        HitRecord shadowHit;
        if (intersectScene(shadowRay, shadowHit)) {
            if (shadowHit.t < distance - 0.001) {
                // 被遮挡
                return RGB(0, 0, 0);
            }
        }
        
        // 计算光照
        double attenuation = light.intensity / (1.0 + 0.01*distance*distance + 0.1*distance);
        double r = light.color[0] * cosTheta * attenuation;
        double g = light.color[1] * cosTheta * attenuation;
        double b = light.color[2] * cosTheta * attenuation;
        
        r = min(max(r, 0.0), 1.0);
        g = min(max(g, 0.0), 1.0);
        b = min(max(b, 0.0), 1.0);
        
        return RGB(r * 255, g * 255, b * 255);
    } else {
        // 面光源：多采样蒙特卡洛积分
        int numSamples = 8;
        double totalR = 0.0, totalG = 0.0, totalB = 0.0;
        
        // 构建本地坐标系
        double lightDir[3];
        lightDir[0] = light.position[0] - hit.position[0];
        lightDir[1] = light.position[1] - hit.position[1];
        lightDir[2] = light.position[2] - hit.position[2];
        double lightDist = sqrt(lightDir[0]*lightDir[0] + 
                              lightDir[1]*lightDir[1] + 
                              lightDir[2]*lightDir[2]);
        normalize(lightDir);
        
        double tangent[3], bitangent[3];
        double reference[3] = {0.0, 1.0, 0.0};
        if (fabs(dot(lightDir, reference)) > 0.9) {
            reference[0] = 1.0; reference[1] = 0.0; reference[2] = 0.0;
        }
        
        cross(lightDir, reference, tangent);
        normalize(tangent);
        cross(lightDir, tangent, bitangent);
        normalize(bitangent);
        
        for (int i = 0; i < numSamples; i++) {
            // 在圆盘上均匀采样
            double r = light.radius * sqrt(rand() / (double)RAND_MAX);
            double theta = 2.0 * PI * (rand() / (double)RAND_MAX);
            
            double sampleLightPos[3];
            sampleLightPos[0] = light.position[0] + r * (cos(theta) * tangent[0] + sin(theta) * bitangent[0]);
            sampleLightPos[1] = light.position[1] + r * (cos(theta) * tangent[1] + sin(theta) * bitangent[1]);
            sampleLightPos[2] = light.position[2] + r * (cos(theta) * tangent[2] + sin(theta) * bitangent[2]);
            
            double sampleDir[3];
            sampleDir[0] = sampleLightPos[0] - hit.position[0];
            sampleDir[1] = sampleLightPos[1] - hit.position[1];
            sampleDir[2] = sampleLightPos[2] - hit.position[2];
            double sampleDistance = sqrt(sampleDir[0]*sampleDir[0] + 
                                       sampleDir[1]*sampleDir[1] + 
                                       sampleDir[2]*sampleDir[2]);
            normalize(sampleDir);
            
            double cosTheta = max(0.0, dot(hit.normal, sampleDir));
            
            // 光源表面法线
            double lightNormal[3];
            lightNormal[0] = sampleLightPos[0] - light.position[0];
            lightNormal[1] = sampleLightPos[1] - light.position[1];
            lightNormal[2] = sampleLightPos[2] - light.position[2];
            normalize(lightNormal);
            
            double cosThetaPrime = max(0.0, -dot(lightNormal, sampleDir));
            
            // 阴影测试
            Ray shadowRay;
            shadowRay.origin[0] = hit.position[0];
            shadowRay.origin[1] = hit.position[1];
            shadowRay.origin[2] = hit.position[2];
            shadowRay.direction[0] = sampleDir[0];
            shadowRay.direction[1] = sampleDir[1];
            shadowRay.direction[2] = sampleDir[2];
            
            shadowRay.origin[0] += sampleDir[0] * 0.001;
            shadowRay.origin[1] += sampleDir[1] * 0.001;
            shadowRay.origin[2] += sampleDir[2] * 0.001;
            
            HitRecord shadowHit;
            if (intersectScene(shadowRay, shadowHit)) {
                if (shadowHit.t < sampleDistance - 0.001) {
                    continue; // 被遮挡，跳过
                }
            }
            
            // 计算贡献
            double geometricTerm = (cosTheta * cosThetaPrime) / (sampleDistance * sampleDistance);
            double attenuation = light.intensity / (1.0 + 0.01*sampleDistance*sampleDistance + 0.1*sampleDistance);
            
            totalR += light.color[0] * geometricTerm * attenuation;
            totalG += light.color[1] * geometricTerm * attenuation;
            totalB += light.color[2] * geometricTerm * attenuation;
        }
        
        // 平均采样结果
        double r = totalR / numSamples;
        double g = totalG / numSamples;
        double b = totalB / numSamples;
        
        r = min(max(r, 0.0), 1.0);
        g = min(max(g, 0.0), 1.0);
        b = min(max(b, 0.0), 1.0);
        
        return RGB(r * 255, g * 255, b * 255);
    }
}
// prepare.cpp - 修改calculateAttenuation函数中的软阴影采样部分

double calculateAttenuation(double distance, double intensity, 
                          double lightPos[3], double hitPos[3], double lightRadius) {
    // 平方反比衰减
    double attenuation = intensity / (1.0 + 0.01*distance * distance+0.1*distance );
    
    // 阴影检查
    Ray shadowRay;
    shadowRay.origin[0] = hitPos[0];
    shadowRay.origin[1] = hitPos[1];
    shadowRay.origin[2] = hitPos[2];
    
    shadowRay.direction[0] = lightPos[0] - hitPos[0];
    shadowRay.direction[1] = lightPos[1] - hitPos[1];
    shadowRay.direction[2] = lightPos[2] - hitPos[2];
    normalize(shadowRay.direction);
    
    // 偏移起点避免自相交
    shadowRay.origin[0] += shadowRay.direction[0] * 0.001;
    shadowRay.origin[1] += shadowRay.direction[1] * 0.001;
    shadowRay.origin[2] += shadowRay.direction[2] * 0.001;
    
    HitRecord shadowHit;
    if (intersectScene(shadowRay, shadowHit)) {
        if (shadowHit.t < distance - 0.001) {
            if (shadowHit.materialType == 3) {
                // 半透明材质，部分遮挡
                double transparency = 0.5;
                attenuation *= transparency;
            } else {
                // 不透明材质，完全遮挡
                return 0.0;
            }
        }
    }
    
    // 软阴影（如果光源有半径）- 重构后的算法
    if (lightRadius > 0) {
        int numSamples = 16; // 增加采样数量以提高质量
        
        // 计算光线到光源的方向
        double lightDir[3];
        lightDir[0] = lightPos[0] - hitPos[0];
        lightDir[1] = lightPos[1] - hitPos[1];
        lightDir[2] = lightPos[2] - hitPos[2];
        double lightDist = distance;
        normalize(lightDir);
        
        // 构建本地坐标系
        double tangent[3], bitangent[3];
        
        // 选择一个与lightDir不平行的参考向量
        double reference[3] = {0.0, 1.0, 0.0};
        if (fabs(dot(lightDir, reference)) > 0.9) {
            reference[0] = 1.0; reference[1] = 0.0; reference[2] = 0.0;
        }
        
        // 计算切线向量
        cross(lightDir, reference, tangent);
        normalize(tangent);
        
        // 计算副切线向量
        cross(lightDir, tangent, bitangent);
        normalize(bitangent);
        
        double visibleSamples = 0.0;
        double totalWeight = 0.0;
        
        // 使用分层采样(stratified sampling)减少噪声
        int sqrtSamples = (int)sqrt(numSamples);
        
        for (int i = 0; i < sqrtSamples; i++) {
            for (int j = 0; j < sqrtSamples; j++) {
                // 计算在单位圆盘上的随机点（分层）
                double xi1 = (i + (rand() / (double)RAND_MAX)) / sqrtSamples;
                double xi2 = (j + (rand() / (double)RAND_MAX)) / sqrtSamples;
                
                // 将均匀分布转换为圆盘上的均匀分布
                double r = sqrt(xi1) * lightRadius;
                double theta = 2.0 * PI * xi2;
                
                // 在光源圆盘上采样点
                double sampleLightPos[3];
                sampleLightPos[0] = lightPos[0] + r * (cos(theta) * tangent[0] + sin(theta) * bitangent[0]);
                sampleLightPos[1] = lightPos[1] + r * (cos(theta) * tangent[1] + sin(theta) * bitangent[1]);
                sampleLightPos[2] = lightPos[2] + r * (cos(theta) * tangent[2] + sin(theta) * bitangent[2]);
                
                // 计算到采样光源点的方向
                double sampleDir[3];
                sampleDir[0] = sampleLightPos[0] - hitPos[0];
                sampleDir[1] = sampleLightPos[1] - hitPos[1];
                sampleDir[2] = sampleLightPos[2] - hitPos[2];
                double sampleDistance = sqrt(sampleDir[0]*sampleDir[0] + 
                                           sampleDir[1]*sampleDir[1] + 
                                           sampleDir[2]*sampleDir[2]);
                normalize(sampleDir);
                
                // 计算光源表面法线（指向外部）
                double lightNormal[3];
                lightNormal[0] = sampleLightPos[0] - lightPos[0];
                lightNormal[1] = sampleLightPos[1] - lightPos[1];
                lightNormal[2] = sampleLightPos[2] - lightPos[2];
                normalize(lightNormal);
                
                // 计算几何项：cos(θ') * cos(θ) / r2
                double cosTheta = max(0.0, dot(shadowHit.normal, sampleDir));
                double cosThetaPrime = max(0.0, -dot(lightNormal, sampleDir));
                
                // 避免除以零
                if (sampleDistance < 1e-6 || cosTheta <= 0.0 || cosThetaPrime <= 0.0) {
                    continue;
                }
                
                // 权重因子
                double weight = (cosTheta * cosThetaPrime) / (sampleDistance * sampleDistance);
                
                Ray sampleShadowRay;
                sampleShadowRay.origin[0] = hitPos[0];
                sampleShadowRay.origin[1] = hitPos[1];
                sampleShadowRay.origin[2] = hitPos[2];
                sampleShadowRay.direction[0] = sampleDir[0];
                sampleShadowRay.direction[1] = sampleDir[1];
                sampleShadowRay.direction[2] = sampleDir[2];
                
                // 偏移起点避免自相交
                sampleShadowRay.origin[0] += sampleDir[0] * 0.001;
                sampleShadowRay.origin[1] += sampleDir[1] * 0.001;
                sampleShadowRay.origin[2] += sampleDir[2] * 0.001;
                
                HitRecord sampleHit;
                if (intersectScene(sampleShadowRay, sampleHit)) {
                    if (sampleHit.t < sampleDistance - 0.001) {
                        // 被遮挡，不增加可见性
                        continue;
                    }
                }
                
                // 这个采样点可见
                visibleSamples += weight;
                totalWeight += weight;
            }
        }
        
        // 计算可见性比例（加权平均）
        double shadowFactor = (totalWeight > 0.0) ? (visibleSamples / totalWeight) : 0.0;
        
        // 如果没有有效的采样，使用硬阴影结果
        if (totalWeight <= 0.0) {
            return 0.0;
        }
        
        attenuation *= shadowFactor;
        
        // 应用软阴影平滑函数（减少噪点）
        double softness = lightRadius / max(distance, 1e-6);
        shadowFactor = min(1.0, shadowFactor + 0.2 * softness);
    }
    
    return attenuation;
}
// 渲染场景（使用OpenMP并行加速）
void renderScene() {
    int STEP =4;
    
    // 并行计算每个像素的颜色
    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < HEIGHT; y += STEP) {
    	#pragma omp parallel for schedule(dynamic)
        for (int x = 0; x < WIDTH; x += STEP) {
            Ray ray = generateRay(x, y);
            flash_screen[x][y] = traceRay(ray,1);
        }
    }
    
    // 绘制到屏幕（每个STEPxSTEP块使用相同颜色）
    for (int y = 0; y < HEIGHT; y += STEP) {
        for (int x = 0; x < WIDTH; x += STEP) {
            setfillcolor(flash_screen[x][y]);
            solidrectangle(x, y, x + STEP, y + STEP);
        }
    }
}

// 处理键盘和鼠标输入
void processInput() {
    double moveX = 0, moveZ = 0, moveY = 0;
    
    // 键盘控制
    if (GetAsyncKeyState('W') & 0x8000) moveZ = 1;
    if (GetAsyncKeyState('S') & 0x8000) moveZ = -1;
    if (GetAsyncKeyState('A') & 0x8000) moveX = -1;
    if (GetAsyncKeyState('D') & 0x8000) moveX = 1;
    if (GetAsyncKeyState('N') & 0x8000) moveY = -1;
    if (GetAsyncKeyState('M') & 0x8000) moveY = 1;
    
    // 计算移动方向
    double forwardX = -sin(camera.yaw) * moveZ;
    double forwardZ = cos(camera.yaw) * moveZ;
    double rightX = cos(camera.yaw) * moveX;
    double rightZ = sin(camera.yaw) * moveX;
    
    camera.x += (forwardX + rightX) * camera.speed;
    camera.z += (forwardZ + rightZ) * camera.speed;
    camera.y += moveY * camera.speed;
    
    // 鼠标控制相机旋转
    MOUSEMSG m;
    while(MouseHit()) {
        m = GetMouseMsg();
        if(m.uMsg == WM_MOUSEMOVE) {
            static int lastX = WIDTH / 2, lastY = HEIGHT / 2;
            int dx = m.x - lastX;
            int dy = m.y - lastY;
            lastX = m.x;
            lastY = m.y;
            
            camera.yaw -= dx * camera.sensitivity;
            camera.pitch += dy * camera.sensitivity;
            
            // 限制俯仰角范围
            if(camera.pitch > PI/2.5) camera.pitch = PI/2.5;
            if(camera.pitch < -PI/2.5) camera.pitch = -PI/2.5;
        }
    }
}

// 主函数
int main() {
    // 初始化图形窗口
    initgraph(WIDTH, HEIGHT);
    ShowCursor(FALSE);
    SetCursorPos(WIDTH / 2, HEIGHT / 2);
    
        // 创建地面（两个三角形组成的矩形）
    addTriangleWithNoTexture({-100,-10,-100}, {-100,-10,100}, {100,-10,-100}, 
                           RGB(0,0, 0));
    addTriangleWithNoTexture({100,-10,100}, {-100,-10,100}, {100,-10,-100}, 
                           RGB(255, 255, 255));    
    // 添加点光源
    addPointLight(10, 100, 100, 1.0, 1.0, 1.0, 500, 0);
    // 加载带纹理的模型
    cout << ProcessModelWithTexture("dagon/dagon.obj", L"dagon/dagon.png") << endl;
    cout << "三角形数量: " << triangleCount << endl;
    

    
    // 构建BVH加速结构
    initBVH();
    
    // 主循环
    while(true) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) break;
        
        processInput();
        
        BeginBatchDraw();
        cleardevice();
        renderScene();
        drawCrosshair();
        EndBatchDraw();
    }
    
    // 清理资源
    deleteBVH(bvhRoot);
    closegraph();
    ShowCursor(TRUE);
    
    return 0;
}
