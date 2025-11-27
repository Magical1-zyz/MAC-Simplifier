#include "../include/MACSimplifier.h"
#include <iostream>

#define TINYGLTF_NO_IMPLEMENTATION // 避免重复定义
#include "tiny_gltf.h"

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: MACSimplifier <input.gltf> <output.gltf> <ratio> [w_norm] [w_uv]" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    double ratio = std::stof(argv[3]);

    MACSimplifier simplifier;

    if (argc >= 5) simplifier.w_norm = std::stof(argv[4]);
    if (argc >= 6) simplifier.w_uv_base = std::stof(argv[5]);

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    std::cout << "[App] Loading " << inputPath << "..." << std::endl;
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, inputPath);
    if (!warn.empty()) std::cout << "Warn: " << warn << std::endl;
    if (!err.empty()) std::cout << "Err: " << err << std::endl;
    if (!ret) {
        std::cout << "Failed to parse glTF" << std::endl;
        return -1;
    }

    simplifier.loadFromTinyGLTF(model, model.meshes[0]);
    simplifier.run(ratio);
    simplifier.saveToGLTF(outputPath);

    std::cout << "[App] Saved to " << outputPath << std::endl;
    return 0;
}