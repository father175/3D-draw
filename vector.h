// vector.h - 三维图形系统核心数据结构与函数声明

#pragma once
#include <bits/stdc++.h>
#include <graphics.h>
using namespace std;

#ifndef VECTOR_H
#define VECTOR_H

// 窗口尺寸常量
const int WIDTH = 800;
const int HEIGHT = 600;
const double PI = 3.1415926535;

// 三维点结构
struct Point3D { 
    double x, y, z; 
};

// 三角形结构（支持纹理映射）
struct Triangle {
    Point3D points[3];      // 三个顶点坐标
    COLORREF color;         // 基础颜色（无纹理时使用）
    int materialType;       // 材质类型：1-漫反射，2-半透明
    bool is_image;          // 是否有纹理贴图
    string image;           // 纹理文件路径
    double x[3], y[3];      // 纹理坐标 (u, v) 对应每个顶点
};

// 光线结构
struct Ray {
    double origin[3];
    double direction[3];
};

// 光线命中记录
struct HitRecord {
    double t;               // 光线参数
    double position[3];     // 命中点坐标
    double normal[3];       // 法线向量
    COLORREF color;         // 表面颜色
    int materialType;       // 材质类型
    double tex_u, tex_v;    // 纹理坐标
    bool hasTexture;        // 是否有纹理
    string texturePath;     // 纹理路径
    bool hit = false;       // 是否命中
};

// 相机结构
struct Camera {
    double x = 0, y = 1, z = 0;    // 位置
    double yaw = 0, pitch = 0;     // 偏航角和俯仰角
    double speed = 1.0;            // 移动速度
    double sensitivity = 0.008;    // 鼠标灵敏度
};

// 点光源结构
struct PointLight {
    double position[3];     // 光源位置
    double color[3];        // RGB颜色
    double intensity;       // 强度
    double radius;          // 光源半径（用于软阴影）
};

// BVH（包围盒层次结构）相关定义
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

// 全局变量声明
extern Triangle triangles[1000001];
extern bitset<1000001> appear;
extern int triangleCount;
extern COLORREF flash_screen[WIDTH][HEIGHT];
extern Camera camera;
extern PointLight pointLights[10];
extern int pointLightCount;
extern BVHNode* bvhRoot;
extern vector<int> triangleIndices;

// 函数声明
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
double calculateAttenuation(double distance, double intensity, 
                          double lightPos[3], double hitPos[3], double lightRadius);
double dot(double a[3], double b[3]);
void cross(double a[3], double b[3], double result[3]);
void subtract(double a[3], double b[3], double result[3]);
void normalize(double v[3]);
// vector.cpp - 三维图形系统核心函数实现


// 全局变量定义
Triangle triangles[1000001];
bitset<1000001> appear;
int triangleCount = 0;
COLORREF flash_screen[WIDTH][HEIGHT];
Camera camera;
PointLight pointLights[10];
int pointLightCount = 0;
BVHNode* bvhRoot = nullptr;
vector<int> triangleIndices;

// 添加无纹理三角形
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

// 添加带纹理的三角形
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

// 添加点光源
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

// 向量点积
double dot(double a[3], double b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// 向量叉积
void cross(double a[3], double b[3], double result[3]) {
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}

// 向量减法
void subtract(double a[3], double b[3], double result[3]) {
    result[0] = a[0] - b[0];
    result[1] = a[1] - b[1];
    result[2] = a[2] - b[2];
}

// 向量归一化
void normalize(double v[3]) {
    double length = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > 0) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
}
#endif // VECTOR_H
