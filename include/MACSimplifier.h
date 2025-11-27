#pragma once
#include "MathUtils.h"
#include <vector>
#include <string>
#include <map>

// 前向声明 tinygltf 类型，避免在头文件中引入大库
namespace tinygltf {
    class Model;
    class Mesh;
}

struct Vertex {
    Vec3 p;
    Quadric q;
    int id;
    bool removed = false;
    std::vector<int> neighbors; // 邻接面索引
};

struct Edge {
    int v1, v2;
    double cost;
    Vec3 target;

    // 优先队列比较函数（最小堆）
    bool operator>(const Edge& other) const {
        return cost > other.cost;
    }
};

class MACSimplifier {
public:
    MACSimplifier();
    ~MACSimplifier();

    // 论文参数配置
    double w_geo;       // 几何权重 (默认 1.0)
    double w_norm;      // 法线权重 (默认 0.1)
    double w_uv_base;   // UV基础权重 (默认 0.1)

    // 数据加载与保存
    void loadFromTinyGLTF(const tinygltf::Model& model, const tinygltf::Mesh& mesh);
    void saveToGLTF(const std::string& filename);

    // 核心算法
    void run(double ratio);

private:
    std::vector<Vertex> vertices;
    std::vector<int> indices;
    std::vector<Vec3> normals;
    std::vector<std::pair<double, double>> uvs;

    void computeQuadrics();
};