// blas_tlas.cpp - TLAS+BLAS实现

#include "bvh.h"
#include <algorithm>
#include <omp.h>

using namespace std;

// 全局变量定义
vector<BLASNode*> blasList;
TLASNode* tlasRoot = nullptr;
vector<Instance> instances;
vector<int> blasIndices;

// 应用变换矩阵到点（世界空间 -> 局部空间）
void applyTransform(double point[3], const double transform[16]) {
    double x = point[0], y = point[1], z = point[2];
    
    // 齐次坐标变换
    point[0] = transform[0]*x + transform[4]*y + transform[8]*z + transform[12];
    point[1] = transform[1]*x + transform[5]*y + transform[9]*z + transform[13];
    point[2] = transform[2]*x + transform[6]*y + transform[10]*z + transform[14];
    
    // 透视除法（如果有透视变换）
    double w = transform[3]*x + transform[7]*y + transform[11]*z + transform[15];
    if(w != 0.0 && w != 1.0) {
        point[0] /= w;
        point[1] /= w;
        point[2] /= w;
    }
}

// 应用逆变换（局部空间 -> 世界空间）
void applyTransformInverse(double point[3], const double transform[16]) {
    // 简化：假设只有旋转和平移（无缩放/透视）
    // 实际应该计算逆矩阵
    double translation[3] = {transform[12], transform[13], transform[14]};
    
    point[0] -= translation[0];
    point[1] -= translation[1];
    point[2] -= translation[2];
}

// 计算BLAS的包围盒（考虑变换）
AABB computeBLASAABB(BLASNode* blas) {
    AABB bbox = computeTriangleAABB(triangles[blas->startTriangle]);
    
    // 遍历BLAS中的所有三角形，合并包围盒
    for(int i = blas->startTriangle + 1; i < blas->endTriangle; i++) {
        AABB triAABB = computeTriangleAABB(triangles[i]);
        bbox.min[0] = min(bbox.min[0], triAABB.min[0]);
        bbox.min[1] = min(bbox.min[1], triAABB.min[1]);
        bbox.min[2] = min(bbox.min[2], triAABB.min[2]);
        bbox.max[0] = max(bbox.max[0], triAABB.max[0]);
        bbox.max[1] = max(bbox.max[1], triAABB.max[1]);
        bbox.max[2] = max(bbox.max[2], triAABB.max[2]);
    }
    
    // 应用变换到包围盒的8个顶点
    double corners[8][3];
    for(int i = 0; i < 8; i++) {
        corners[i][0] = (i & 1) ? bbox.max[0] : bbox.min[0];
        corners[i][1] = (i & 2) ? bbox.max[1] : bbox.min[1];
        corners[i][2] = (i & 4) ? bbox.max[2] : bbox.min[2];
        applyTransform(corners[i], blas->transform);
    }
    
    // 计算变换后的包围盒
    AABB transformedBBox;
    transformedBBox.min[0] = transformedBBox.min[1] = transformedBBox.min[2] = 1e9;
    transformedBBox.max[0] = transformedBBox.max[1] = transformedBBox.max[2] = -1e9;
    
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 3; j++) {
            if(corners[i][j] < transformedBBox.min[j]) transformedBBox.min[j] = corners[i][j];
            if(corners[i][j] > transformedBBox.max[j]) transformedBBox.max[j] = corners[i][j];
        }
    }
    
    return transformedBBox;
}

// 构建BLAS（为物体构建内部BVH）
void buildBLAS(BLASNode* blas, int startTri, int endTri) {
    blas->startTriangle = startTri;
    blas->endTriangle = endTri;
    
    // 构建物体内部的三角形索引
    vector<int> localTriangleIndices;
    for(int i = startTri; i < endTri; i++) {
        localTriangleIndices.push_back(i);
    }
    
    // 如果三角形数量少，直接作为叶子
    if(localTriangleIndices.size() <= 5) {
        // 简单处理：不构建内部BVH，直接存储索引
        // 实际可以构建小型BVH
        return;
    }
    
    // 否则构建BVH
    // 注意：这里需要在BLASNode中添加BVH树指针
    // 为简化，我们先不实现，只存储包围盒
}

// 更新BLAS（用于动态物体）
void updateBLAS(BLASNode* blas) {
    if(blas->isStatic) return;
    
    // 重新计算包围盒
    blas->bbox = computeBLASAABB(blas);
    
    // 如果有内部BVH，也需要更新
    // ...
}

// 比较BLAS的包围盒中心（用于TLAS构建）
bool compareBLASByAxis(int a, int b, int axis) {
    AABB bboxA = blasList[a]->bbox;
    AABB bboxB = blasList[b]->bbox;
    
    double centerA = (bboxA.min[axis] + bboxA.max[axis]) * 0.5;
    double centerB = (bboxB.min[axis] + bboxB.max[axis]) * 0.5;
    
    return centerA < centerB;
}

// 构建TLAS（递归）
TLASNode* buildTLASRecursive(int start, int end, int depth) {
    if(start >= end) return nullptr;
    
    TLASNode* node = new TLASNode();
    
    // 计算当前节点的包围盒
    node->bbox = blasList[blasIndices[start]]->bbox;
    for(int i = start + 1; i < end; i++) {
        node->bbox = mergeAABB(node->bbox, blasList[blasIndices[i]]->bbox);
    }
    
    // 如果BLAS数量少或者达到最大深度，创建叶子节点
    if(end - start <= 2 || depth >= 20) {
        node->isLeaf = true;
        node->startBlasIndex = start;
        node->endBlasIndex = end;
        return node;
    }
    
    // 选择最长的轴进行划分
    double extent[3] = {
        node->bbox.max[0] - node->bbox.min[0],
        node->bbox.max[1] - node->bbox.min[1],
        node->bbox.max[2] - node->bbox.min[2]
    };
    
    int axis = 0;
    if(extent[1] > extent[axis]) axis = 1;
    if(extent[2] > extent[axis]) axis = 2;
    
    // 按轴排序BLAS
    sort(blasIndices.begin() + start, blasIndices.begin() + end,
         [axis](int a, int b) {
             AABB bboxA = blasList[a]->bbox;
             AABB bboxB = blasList[b]->bbox;
             double centerA = (bboxA.min[axis] + bboxA.max[axis]) * 0.5;
             double centerB = (bboxB.min[axis] + bboxB.max[axis]) * 0.5;
             return centerA < centerB;
         });
    
    // 在中间点分割
    int mid = start + (end - start) / 2;
    
    // 递归构建左右子树
    node->isLeaf = false;
    node->left = buildTLASRecursive(start, mid, depth + 1);
    node->right = buildTLASRecursive(mid, end, depth + 1);
    
    return node;
}

// 构建完整的TLAS
void buildTLAS() {
    if(blasList.empty()) return;
    
    // 创建BLAS索引数组
    blasIndices.resize(blasList.size());
    for(size_t i = 0; i < blasList.size(); i++) {
        blasIndices[i] = i;
    }
    
    // 构建TLAS
    tlasRoot = buildTLASRecursive(0, blasList.size(), 0);
    cout << "TLAS构建完成，BLAS数量: " << blasList.size() << endl;
}

// TLAS相交测试（递归）
bool intersectTLASRecursive(TLASNode* node, const Ray& worldRay, HitRecord& hit) {
    if(node == nullptr) return false;
    
    // 测试光线与包围盒是否相交
    double tMin, tMax;
    if(!intersectAABB(worldRay, node->bbox, tMin, tMax)) {
        return false;
    }
    
    bool hitFound = false;
    
    if(node->isLeaf) {
        // 叶子节点：测试所有BLAS
        for(int i = node->startBlasIndex; i < node->endBlasIndex; i++) {
            BLASNode* blas = blasList[blasIndices[i]];
            
            // 将光线转换到物体局部空间
            Ray localRay = worldRay;
            applyTransformInverse(localRay.origin, blas->transform);
            applyTransformInverse(localRay.direction, blas->transform);
            
            // 测试包围盒
            if(!intersectAABB(localRay, blas->bbox, tMin, tMax)) {
                continue;
            }
            
            // 测试BLAS内的三角形
            HitRecord localHit;
            localHit.t = hit.t;
            
            for(int triIdx = blas->startTriangle; triIdx < blas->endTriangle; triIdx++) {
                if(appear[triIdx] == 1) {
                    intersectTriangle(localRay, triangles[triIdx], localHit);
                }
            }
            
            if(localHit.hit && localHit.t < hit.t) {
                // 将结果转换回世界空间
                hit = localHit;
                applyTransform(hit.position, blas->transform);
                
                // 变换法线（注意：法线变换需要转置逆矩阵）
                // 简化：假设只有旋转，可以直接变换
                applyTransform(hit.normal, blas->transform);
                normalize(hit.normal);
                
                hit.t = localHit.t; // 注意：t值需要重新计算距离
                hitFound = true;
            }
        }
    } else {
        // 内部节点：测试子节点
        hitFound |= intersectTLASRecursive(node->left, worldRay, hit);
        hitFound |= intersectTLASRecursive(node->right, worldRay, hit);
    }
    
    return hitFound;
}

// TLAS相交测试（入口函数）
bool intersectTLAS(const Ray& ray, HitRecord& hit) {
    if(tlasRoot == nullptr) return false;
    return intersectTLASRecursive(tlasRoot, ray, hit);
}

// 释放TLAS内存
void deleteTLAS(TLASNode* node) {
    if(node == nullptr) return;
    
    if(!node->isLeaf) {
        deleteTLAS(node->left);
        deleteTLAS(node->right);
    }
    
    delete node;
}

// 释放BLAS内存
void deleteBLAS() {
    for(auto blas : blasList) {
        delete blas;
    }
    blasList.clear();
}
