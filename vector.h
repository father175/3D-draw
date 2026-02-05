// vector.h - ?????????????????????????

#pragma once
#include <bits/stdc++.h>
#include <graphics.h>
using namespace std;

#ifndef VECTOR_H
#define VECTOR_H

// ?????糣??
const int WIDTH = 800;
const int HEIGHT = 600;
const double PI = 3.1415926535;

// ??????
struct Point3D { 
    double x, y, z; 
};

// ?????ν?????????????
struct Triangle {
    Point3D points[3];      // ????????????
    COLORREF color;         // ????????????????????
    int materialType;       // ?????????1-??????2-?????
    bool is_image;          // ????????????
    string image;           // ???????·??
    double x[3], y[3];      // ???????? (u, v) ??????????
};

// ?????
struct Ray {
    double origin[3];
    double direction[3];
};

// ???????м??
struct HitRecord {
    double t;               // ???????
    double position[3];     // ???е?????
    double normal[3];       // ????????
    COLORREF color;         // ???????
    int materialType;       // ????????
    double tex_u, tex_v;    // ????????
    bool hasTexture;        // ?????????
    string texturePath;     // ????·??
    bool hit = false;       // ???????
};

// ?????
struct Camera {
    double x = 0, y = 1, z = 0;    // λ??
    double yaw = 0, pitch = 0;     // ???????????
    double speed = 1.0;            // ??????
    double sensitivity = 0.008;    // ?????????
};

// ??????
struct PointLight {
    double position[3];     // ???λ??
    double color[3];        // RGB???
    double intensity;       // ???
    double radius;          // ??????????????????
};

// BVH????Χ?в?ν?????????
struct AABB {
    double min[3];
    double max[3];
    
    AABB() {
        min[0] = min[1] = min[2] = 1e9;
        max[0] = max[1] = max[2] = -1e9;
    }
};

struct BVHNode {
    AABB bbox;
    BVHNode* left;
    BVHNode* right;
    int startIndex;
    int endIndex;
    bool isLeaf;
    
    BVHNode() : left(nullptr), right(nullptr), isLeaf(false) {}
};

struct TextureData {
    string filename;
    int width, height;
    COLORREF* pixels;   // ???????
    BYTE* alpha;        // ????????
    bool loaded;        // ????????
};

// ??????????
extern Triangle triangles[1000001];
extern bitset<1000001> appear;
extern int triangleCount;
extern COLORREF flash_screen[WIDTH][HEIGHT];
extern Camera camera;
extern PointLight pointLights[10];
extern int pointLightCount;
extern vector<TextureData> textureCache;

// ????????
void addTriangleWithNoTexture(Point3D a, Point3D b, Point3D c, 
                             COLORREF color, int matType = 1);
void addTriangleWithTexture(Point3D a, Point3D b, Point3D c, 
                           string texturePath, 
                           double u1, double v1,
                           double u2, double v2,
                           double u3, double v3,
                           int matType = 1);
void addPointLight(double x, double y, double z, 
                   double r, double g, double b, 
                   double intensity = 1.0, double radius = 0.0);
double dot(double a[3], double b[3]);
void cross(double a[3], double b[3], double result[3]);
void subtract(double a[3], double b[3], double result[3]);
void normalize(double v[3]);
// vector.cpp - ??????????????????


// ??????????
Triangle triangles[1000001];
bitset<1000001> appear;
int triangleCount = 0;
COLORREF flash_screen[WIDTH][HEIGHT];
Camera camera;
PointLight pointLights[10];
int pointLightCount = 0;
vector<int> triangleIndices;
int STEP;

// ????????????????
void addTriangleWithNoTexture(Point3D a, Point3D b, Point3D c, 
                             COLORREF color, int matType) {
    if (triangleCount >= 1000000) {
        cout << "Error: Triangle array full!" << endl;
        return;
    }
    
    triangles[triangleCount].points[0] = a;
    triangles[triangleCount].points[1] = b;
    triangles[triangleCount].points[2] = c;
    triangles[triangleCount].is_image = false;
    triangles[triangleCount].materialType = matType;
    triangles[triangleCount].color = color;
    appear[triangleCount] = 1;
    triangleCount++;
}

// ?????????????????
void addTriangleWithTexture(Point3D a, Point3D b, Point3D c, 
                           string texturePath, 
                           double u1, double v1,
                           double u2, double v2,
                           double u3, double v3,
                           int matType) {
    if (triangleCount >= 1000000) {
        cout << "Error: Triangle array full!" << endl;
        return;
    }
    
    triangles[triangleCount].points[0] = a;
    triangles[triangleCount].points[1] = b;
    triangles[triangleCount].points[2] = c;
    triangles[triangleCount].is_image = true;
    triangles[triangleCount].image = texturePath;
    triangles[triangleCount].x[0] = u1; triangles[triangleCount].y[0] = v1;
    triangles[triangleCount].x[1] = u2; triangles[triangleCount].y[1] = v2;
    triangles[triangleCount].x[2] = u3; triangles[triangleCount].y[2] = v3;
    triangles[triangleCount].materialType = matType;
    triangles[triangleCount].color = RGB(255, 255, 255);
    appear[triangleCount] = 1;
    triangleCount++;
}

// ???????
void addPointLight(double x, double y, double z, 
                   double r, double g, double b, 
                   double intensity, double radius) {
    if (pointLightCount >= 10) {
        cout << "Error: Too many point lights!" << endl;
        return;
    }
    
    pointLights[pointLightCount].position[0] = x;
    pointLights[pointLightCount].position[1] = y;
    pointLights[pointLightCount].position[2] = z;
    pointLights[pointLightCount].color[0] = r;
    pointLights[pointLightCount].color[1] = g;
    pointLights[pointLightCount].color[2] = b;
    pointLights[pointLightCount].intensity = intensity;
    pointLights[pointLightCount].radius = radius;
    pointLightCount++;
}

// ???????
double dot(double a[3], double b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// ???????
void cross(double a[3], double b[3], double result[3]) {
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}

// ????????
void subtract(double a[3], double b[3], double result[3]) {
    result[0] = a[0] - b[0];
    result[1] = a[1] - b[1];
    result[2] = a[2] - b[2];
}

// ?????????
void normalize(double v[3]) {
    double length = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > 0) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
}
// vector.h - 新增TLAS+BLAS相关结构

// BLAS节点（每个物体一个）
struct BLASNode {
    AABB bbox;              // 物体局部空间的包围盒
    BVHNode* triangleBVH;   // 物体内部的三角形BVH
    int startTriangle;      // 物体包含的三角形起始索引
    int endTriangle;        // 物体包含的三角形结束索引
    double transform[16];   // 物体的变换矩阵（4x4，列主序）
    bool isStatic;          // 是否是静态物体
    string name;            // 物体名称（可选）
    
    BLASNode() : triangleBVH(nullptr), isStatic(true) {
        // 初始化为单位矩阵
        for(int i = 0; i < 16; i++) transform[i] = (i % 5 == 0) ? 1.0 : 0.0;
    }
};

// TLAS节点（场景管理）
struct TLASNode {
    AABB bbox;              // 世界空间的包围盒
    TLASNode* left;
    TLASNode* right;
    BLASNode* blas;         // 如果是叶子节点，指向BLAS
    int startBlasIndex;     // 起始BLAS索引（非叶子节点）
    int endBlasIndex;       // 结束BLAS索引（非叶子节点）
    bool isLeaf;
    
    TLASNode() : left(nullptr), right(nullptr), blas(nullptr), isLeaf(false) {}
};

// 物体实例（用于实例化）
struct Instance {
    BLASNode* blas;         // 指向共享的BLAS
    double transform[16];   // 实例的变换矩阵
    int instanceId;         // 实例ID
    bool visible;           // 是否可见
};

// 新增全局变量
extern vector<BLASNode*> blasList;          // 所有BLAS列表
extern TLASNode* tlasRoot;                  // TLAS根节点
extern vector<Instance> instances;          // 所有实例
extern vector<int> blasIndices;             // BLAS索引（用于TLAS构建）

// 新增函数声明
void buildBLAS(BLASNode* blas, int startTri, int endTri);
void updateBLAS(BLASNode* blas);
void buildTLAS();
bool intersectTLAS(const Ray& ray, HitRecord& hit);
void applyTransform(double point[3], const double transform[16]);
void applyTransformInverse(double point[3], const double transform[16]);
#endif // VECTOR_H
