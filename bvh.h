// bvh.cpp - 包围盒层次结构实现

#include "vector.h"
#include <bits/stdc++.h>
#include <graphics.h>
using namespace std;

// 计算三角形包围盒
AABB computeTriangleAABB(const Triangle& tri) {
    AABB bbox;
    
    // 初始化
    bbox.min[0] = bbox.max[0] = tri.points[0].x;
    bbox.min[1] = bbox.max[1] = tri.points[0].y;
    bbox.min[2] = bbox.max[2] = tri.points[0].z;
    
    // 扩展包围盒
    for (int i = 1; i < 3; i++) {
        bbox.min[0] = min(bbox.min[0], tri.points[i].x);
        bbox.max[0] = max(bbox.max[0], tri.points[i].x);
        bbox.min[1] = min(bbox.min[1], tri.points[i].y);
        bbox.max[1] = max(bbox.max[1], tri.points[i].y);
        bbox.min[2] = min(bbox.min[2], tri.points[i].z);
        bbox.max[2] = max(bbox.max[2], tri.points[i].z);
    }
    
    // 添加容差避免数值误差
    for (int i = 0; i < 3; i++) {
        bbox.min[i] -= 1e-6;
        bbox.max[i] += 1e-6;
    }
    
    return bbox;
}

// 合并两个包围盒
AABB mergeAABB(const AABB& a, const AABB& b) {
    AABB result;
    
    result.min[0] = min(a.min[0], b.min[0]);
    result.min[1] = min(a.min[1], b.min[1]);
    result.min[2] = min(a.min[2], b.min[2]);
    
    result.max[0] = max(a.max[0], b.max[0]);
    result.max[1] = max(a.max[1], b.max[1]);
    result.max[2] = max(a.max[2], b.max[2]);
    
    return result;
}

// 按指定轴比较三角形（用于BVH构建）
bool compareTrianglesByAxis(int a, int b, int axis) {
    AABB bboxA = computeTriangleAABB(triangles[a]);
    AABB bboxB = computeTriangleAABB(triangles[b]);
    
    double centerA = (bboxA.min[axis] + bboxA.max[axis]) * 0.5;
    double centerB = (bboxB.min[axis] + bboxB.max[axis]) * 0.5;
    
    return centerA < centerB;
}

// 光线与三角形相交测试（M?ller-Trumbore算法）
bool intersectTriangle(Ray ray, Triangle tri, HitRecord& hit) {
    const double EPSILON = 1e-6;
    
    double edge1[3], edge2[3], h[3], s[3], q[3];
    double a, f, u, v;
    
    Point3D v0 = tri.points[0], v1 = tri.points[1], v2 = tri.points[2];
    
    edge1[0] = v1.x - v0.x; edge1[1] = v1.y - v0.y; edge1[2] = v1.z - v0.z;
    edge2[0] = v2.x - v0.x; edge2[1] = v2.y - v0.y; edge2[2] = v2.z - v0.z;
    
    cross(ray.direction, edge2, h);
    a = dot(edge1, h);
    
    if (a > -EPSILON && a < EPSILON) return false;
    
    f = 1.0 / a;
    s[0] = ray.origin[0] - v0.x;
    s[1] = ray.origin[1] - v0.y;
    s[2] = ray.origin[2] - v0.z;
    
    u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return false;
    
    cross(s, edge1, q);
    v = f * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;
    
    double t = f * dot(edge2, q);
    if (t > EPSILON && t < hit.t) {
        hit.t = t;
        hit.position[0] = ray.origin[0] + ray.direction[0] * t;
        hit.position[1] = ray.origin[1] + ray.direction[1] * t;
        hit.position[2] = ray.origin[2] + ray.direction[2] * t;
        hit.hit = true;
        
        double normal[3];
        cross(edge1, edge2, normal);
        normalize(normal);
        hit.normal[0] = normal[0];
        hit.normal[1] = normal[1];
        hit.normal[2] = normal[2];
        
        hit.color = tri.color;
        hit.materialType = tri.materialType;
        
        // 计算重心坐标用于纹理插值
        double baryU = 1.0 - u - v;
        double baryV = u;
        double baryW = v;
        
        // 插值纹理坐标
        hit.tex_u = baryU * tri.x[0] + baryV * tri.x[1] + baryW * tri.x[2];
        hit.tex_v = baryU * tri.y[0] + baryV * tri.y[1] + baryW * tri.y[2];
        hit.hasTexture = tri.is_image;
        hit.texturePath = tri.image;
        
        return true;
    }
    
    return false;
}

// AABB与光线相交测试（Slab方法）
bool intersectAABB(const Ray& ray, const AABB& bbox, double& tMin, double& tMax) {
    tMin = -1e9;
    tMax = 1e9;
    
    for (int i = 0; i < 3; i++) {
        if (fabs(ray.direction[i]) < 1e-9) {
            // 光线与平面平行
            if (ray.origin[i] < bbox.min[i] || ray.origin[i] > bbox.max[i]) {
                return false;
            }
        } else {
            double t1 = (bbox.min[i] - ray.origin[i]) / ray.direction[i];
            double t2 = (bbox.max[i] - ray.origin[i]) / ray.direction[i];
            
            if (t1 > t2) swap(t1, t2);
            
            tMin = max(tMin, t1);
            tMax = min(tMax, t2);
            
            if (tMin > tMax) return false;
        }
    }
    
    return tMax > 1e-6; // 确保tMax是正数
}
