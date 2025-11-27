#pragma once
#include "MathUtils.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <Eigen/Dense>

// Assimp 前向声明
struct aiScene;
struct aiMesh;

struct Vertex {
    Eigen::Vector3d p;
    Quadric q;
    int id;
    int uniqueId;
    bool removed = false;
};

struct Edge {
    int v1, v2;
    double cost;
    Eigen::Vector3d target;

    bool operator>(const Edge& other) const {
        return cost > other.cost;
    }
};

class MACSimplifier {
public:
    MACSimplifier();
    ~MACSimplifier();

    double w_geo;
    double w_norm;
    double w_uv_base;
    double w_boundary;

    // 修改：接收 Assimp 的 aiScene 指针
    // 注意：我们会直接修改 scene 中的 mesh 数据
    void simplify(const aiScene* scene, double ratio);

private:
    std::vector<Vertex> vertices;
    std::vector<int> indices;

    std::vector<Eigen::Vector3d> normals;
    std::vector<Eigen::Vector2d> uvs;

    struct UniqueVertex {
        Eigen::Vector3d p;
        Quadric q;
        std::vector<int> originalIndices;
        bool removed = false;
    };
    std::vector<UniqueVertex> uniqueVertices;
    std::vector<int> uniqueIndices;

    // 记录原始 Mesh 归属，用于回写
    struct MeshRef {
        aiMesh* mesh;      // 指向 Assimp Mesh 的指针
        int baseVertexIdx; // 全局顶点偏移
        int indexCount;    // 索引数量
    };
    std::vector<MeshRef> meshGroups;
    std::unordered_map<int, int> globalFaceToMeshID;

    // 辅助函数
    void loadData(const aiScene* scene);
    void buildUniqueTopology();
    void computeQuadrics();
    void runSimplification(double ratio);
    void writeBack(const aiScene* scene);
    void clear();
};