// add_trangle.cpp - 模型加载和纹理处理

#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <bits/stdc++.h>
#include "vector.h"
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using namespace std;

// 纹理数据结构
struct TextureData {
    string filename;
    int width, height;
    COLORREF* pixels;   // 颜色数据
    BYTE* alpha;        // 透明度通道
    bool loaded;        // 是否已加载
};

// 纹理缓存
vector<TextureData> textureCache;

// 宽字符转多字节字符串
std::string WideToMultiByte(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) return std::string();
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    if (!str.empty() && str.back() == '\0') {
        str.pop_back();
    }
    return str;
}

// 在缓存中查找纹理
TextureData* findTexture(const string& filename) {
    for (auto& tex : textureCache) {
        if (tex.filename == filename && tex.loaded) {
            return &tex;
        }
    }
    return nullptr;
}

// 加载PNG纹理（使用GDI+）
bool loadPNGTexture(const wchar_t* filename, TextureData& texData) {
    // 初始化GDI+
    static ULONG_PTR gdiplusToken = 0;
    if (gdiplusToken == 0) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    }
    
    Bitmap* bitmap = Bitmap::FromFile(filename);
    if (bitmap == NULL || bitmap->GetLastStatus() != Ok) {
        cout << "Failed to load texture: " << filename << endl;
        return false;
    }
    
    texData.width = bitmap->GetWidth();
    texData.height = bitmap->GetHeight();
    texData.pixels = new COLORREF[texData.width * texData.height];
    texData.alpha = new BYTE[texData.width * texData.height];
    
    for (int y = 0; y < texData.height; y++) {
        for (int x = 0; x < texData.width; x++) {
            Color color;
            bitmap->GetPixel(x, y, &color);
            
            // 存储颜色和透明度
            texData.pixels[y * texData.width + x] = 
                RGB(color.GetRed(), color.GetGreen(), color.GetBlue());
            texData.alpha[y * texData.width + x] = color.GetAlpha();
        }
    }
    
    delete bitmap;
    texData.loaded = true;
    cout << "Texture loaded: " << filename << " (" << texData.width << "x" << texData.height << ")" << endl;
    return true;
}

// 纹理采样
COLORREF sampleTexture(const TextureData& tex, double u, double v, BYTE& alpha) {
    // 处理纹理坐标重复
    u = u - floor(u);
    v = v - floor(v);
    
    // 确保坐标在[0,1)范围内
    if (u < 0) u = 0;
    if (u >= 1) u = 0.9999;
    if (v < 0) v = 0;
    if (v >= 1) v = 0.9999;
    
    int x = (int)(1.0 * u * tex.width);
    int y = (int)(1.0 * v * tex.height);
    
    // 边界检查
    if (x < 0) x = 0;
    if (x >= tex.width) x = tex.width - 1;
    if (y < 0) y = 0;
    if (y >= tex.height) y = tex.height - 1;
    
    int index = y * tex.width + x;
    alpha = tex.alpha[index];
    return tex.pixels[index];
}

// 解析OBJ模型文件
bool loadOBJModel(const char* filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Failed to open OBJ file: " << filename << endl;
        return false;
    }
    
    vector<Point3D> vertices;
    vector<Point3D> texCoords;
    vector<Point3D> normals;
    vector<vector<pair<int,int> > > faces;
    
    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string type;
        iss >> type;
        
        if (type == "v") {  // 顶点
            Point3D v;
            iss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        }
        else if (type == "vt") {  // 纹理坐标
            Point3D vt;
            iss >> vt.x >> vt.y;
            vt.z = 0;
            vt.y = 1.0 - vt.y;  // 翻转Y轴
            texCoords.push_back(vt);
        }
        else if (type == "vn") {  // 法线
            Point3D vn;
            iss >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if (type == "f") {  // 面
            vector<pair<int,int> > face;
            string token;
            while (iss >> token) {
                replace(token.begin(), token.end(), '/', ' ');
                istringstream tokenStream(token);
                int v, vt = -1, vn = -1;
                tokenStream >> v;
                if (tokenStream >> vt) {
                    if (tokenStream >> vn) {
                        // 三个索引都有
                    }
                }
                face.push_back(make_pair(v-1, vt-1));
            }
            faces.push_back(face);
        }
    }
    
    file.close();
    
    // 创建三角形
    for (const auto& face : faces) {
        if (face.size() == 3) {
            Point3D v1 = vertices[face[0].first];
            Point3D v2 = vertices[face[1].first];
            Point3D v3 = vertices[face[2].first];
            
            triangles[triangleCount].points[0] = v1;
            triangles[triangleCount].points[1] = v2;
            triangles[triangleCount].points[2] = v3;
            
            // 设置纹理坐标
            if (face[0].second >= 0 && face[0].second < texCoords.size())
                triangles[triangleCount].x[0] = texCoords[face[0].second].x;
            if (face[1].second >= 0 && face[1].second < texCoords.size())
                triangles[triangleCount].x[1] = texCoords[face[1].second].x;
            if (face[2].second >= 0 && face[2].second < texCoords.size())
                triangles[triangleCount].x[2] = texCoords[face[2].second].x;
                
            if (face[0].second >= 0 && face[0].second < texCoords.size())
                triangles[triangleCount].y[0] = texCoords[face[0].second].y;
            if (face[1].second >= 0 && face[1].second < texCoords.size())
                triangles[triangleCount].y[1] = texCoords[face[1].second].y;
            if (face[2].second >= 0 && face[2].second < texCoords.size())
                triangles[triangleCount].y[2] = texCoords[face[2].second].y;
            
            triangles[triangleCount].color = RGB(rand()%255, rand()%255, rand()%255);
            triangles[triangleCount].materialType = 1;
            triangles[triangleCount].is_image = false;
            triangleCount++;
            
            if (triangleCount >= 1000000) {
                cout << "Warning: Triangle limit reached" << endl;
                break;
            }
        }
    }
    
    cout << "OBJ loaded: " << filename << " (" << triangleCount << " triangles)" << endl;
    return true;
}

// 主函数：加载带纹理的模型
bool ProcessModelWithTexture(const char* objFile, const wchar_t* textureFile) {
	int to_start=triangleCount;
    cout << "Processing model with texture..." << endl;
    
    // 1. 加载纹理
    TextureData tex;
    tex.filename = WideToMultiByte(textureFile);
    if (!loadPNGTexture(textureFile, tex)) {
        cout << "Failed to load texture" << endl;
        return false;
    }
    textureCache.push_back(tex);
    
    // 2. 加载OBJ模型
    if (!loadOBJModel(objFile)) {
        cout << "Failed to load OBJ model" << endl;
        return false;
    }
    
    // 3. 为每个三角形设置纹理信息
    TextureData* loadedTex = findTexture(WideToMultiByte(textureFile));
    if (loadedTex) {
        for (int i = to_start; i < triangleCount; i++) {
            triangles[i].is_image = true;
            triangles[i].image = WideToMultiByte(textureFile);
            appear[i] = 1;
        }
        cout << "Texture assigned to " << triangleCount << " triangles" << endl;
    }
    
    return true;
}
