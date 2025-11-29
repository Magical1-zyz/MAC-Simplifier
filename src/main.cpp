#include "../include/MACSimplifier.h"
#include <iostream>
#include <filesystem>
#include <set>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: MACSimplifier <input> <output> <ratio> [w_norm] [w_uv] [w_boundary]" << std::endl;
        return 1;
    }

    std::string inputPathStr = argv[1];
    std::string outputPathStr = argv[2];
    double ratio = 0.5;
    if (argc >= 4) ratio = std::stof(argv[3]);

    MACSimplifier simplifier;
    if (argc >= 5) simplifier.w_norm = std::stof(argv[4]);
    if (argc >= 6) simplifier.w_uv_base = std::stof(argv[5]);
    if (argc >= 7) simplifier.w_boundary = std::stof(argv[6]);

    std::cout << "[App] Settings:" << std::endl;
    std::cout << "      Input:  " << inputPathStr << std::endl;
    std::cout << "      Output: " << outputPathStr << std::endl;
    std::cout << "      Ratio:  " << ratio << std::endl;

    // --- Assimp Load ---
    Assimp::Importer importer;
    // 添加 OptimizeMeshes 和 OptimizeGraph
    // 尝试将材质相同的多个 Mesh 合并，解决“构件分离”问题
    const aiScene* scene = importer.ReadFile(inputPathStr,
                                             aiProcess_Triangulate |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_PreTransformVertices |
                                             aiProcess_SortByPType |
                                             aiProcess_OptimizeMeshes |
                                             aiProcess_OptimizeGraph);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "[Error] Assimp Load Failed: " << importer.GetErrorString() << std::endl;
        return -1;
    }

    std::cout << "[App] Loaded successfully. Meshes: " << scene->mNumMeshes << std::endl;

    // --- Simplify ---
    simplifier.simplify(scene, ratio);

    // --- Texture Copying ---
    std::cout << "[App] Processing textures..." << std::endl;
    try {
        fs::path inputPath(inputPathStr);
        fs::path outputPath(outputPathStr);
        fs::path inputDir = fs::absolute(inputPath).parent_path();
        fs::path outputDir = fs::absolute(outputPath).parent_path();

        if (!fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }

        std::set<std::string> processedTextures;

        if (scene->HasMaterials()) {
            for (unsigned int m = 0; m < scene->mNumMaterials; ++m) {
                aiMaterial* mat = scene->mMaterials[m];
                for (int t = aiTextureType_NONE; t <= 20; ++t) {
                    aiTextureType type = static_cast<aiTextureType>(t);
                    unsigned int count = mat->GetTextureCount(type);
                    for (unsigned int i = 0; i < count; ++i) {
                        aiString aiPath;
                        if (mat->GetTexture(type, i, &aiPath) == AI_SUCCESS) {
                            std::string texPathStr = aiPath.C_Str();
                            if (texPathStr.empty() || texPathStr[0] == '*') continue;
                            if (processedTextures.find(texPathStr) != processedTextures.end()) continue;
                            processedTextures.insert(texPathStr);

                            fs::path texPath(texPathStr);
                            fs::path srcFile = inputDir / texPath;
                            fs::path dstFile = outputDir / texPath.filename();

                            try {
                                if (fs::exists(srcFile)) {
                                    fs::copy_file(srcFile, dstFile, fs::copy_options::overwrite_existing);
                                    std::cout << "      [Copy] " << texPath.filename().string() << std::endl;
                                } else {
                                    std::cout << "      [Warn] Texture missing: " << srcFile.string() << std::endl;
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "      [Error] Copy failed for " << texPathStr << ": " << e.what() << std::endl;
                            }
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Error] Filesystem error: " << e.what() << std::endl;
    }

    // --- Assimp Export ---
    Assimp::Exporter exporter;
    std::string formatId = "gltf2";
    if (outputPathStr.find(".obj") != std::string::npos) formatId = "obj";
    else if (outputPathStr.find(".glb") != std::string::npos) formatId = "glb2";

    std::cout << "[App] Exporting to " << outputPathStr << " (" << formatId << ")..." << std::endl;

    aiReturn ret = exporter.Export(scene, formatId, outputPathStr);
    if (ret != aiReturn_SUCCESS) {
        std::cout << "[Error] Export failed: " << exporter.GetErrorString() << std::endl;
        return -1;
    }

    std::cout << "[App] Done." << std::endl;
    return 0;
}