#include "../include/MACSimplifier.h"
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: MACSimplifier <input> <output> <ratio> [w_norm] [w_uv]" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    double ratio = 0.5;
    if (argc >= 4) ratio = std::stof(argv[3]);

    MACSimplifier simplifier;
    if (argc >= 5) simplifier.w_norm = std::stof(argv[4]);
    if (argc >= 6) simplifier.w_uv_base = std::stof(argv[5]);

    // --- Assimp Load ---
    Assimp::Importer importer;
    // 关键配置：
    // aiProcess_Triangulate: 确保全是三角形
    // aiProcess_JoinIdenticalVertices: 合并完全相同的顶点
    // aiProcess_PreTransformVertices: 【核心】将所有节点变换应用到网格，解决模型错乱/拉丝问题
    // aiProcess_SortByPType: 移除点和线，只留三角形
    const aiScene* scene = importer.ReadFile(inputPath,
                                             aiProcess_Triangulate |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_PreTransformVertices |
                                             aiProcess_SortByPType);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "[Error] Assimp Load Failed: " << importer.GetErrorString() << std::endl;
        return -1;
    }

    std::cout << "[App] Loaded " << inputPath << " successfully." << std::endl;
    std::cout << "      Meshes: " << scene->mNumMeshes << std::endl;

    // --- Simplify ---
    simplifier.simplify(scene, ratio);

    // --- Assimp Export ---
    Assimp::Exporter exporter;
    // 获取输出格式 ID (例如 "gltf2", "obj", etc.)
    // 简单起见，根据后缀判断，或者默认 gltf2
    std::string formatId = "gltf2";
    if (outputPath.find(".obj") != std::string::npos) formatId = "obj";
    else if (outputPath.find(".glb") != std::string::npos) formatId = "glb2";

    std::cout << "[App] Exporting to " << outputPath << " (" << formatId << ")..." << std::endl;

    aiReturn ret = exporter.Export(scene, formatId, outputPath);
    if (ret != aiReturn_SUCCESS) {
        std::cout << "[Error] Export failed: " << exporter.GetErrorString() << std::endl;
        return -1;
    }

    std::cout << "[App] Done." << std::endl;
    return 0;
}