#include "../include/MACSimplifier.h"
#include "../include/MathUtils.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <cstring> // for memcpy

// 必须在某个 .cpp 文件中定义这些宏一次
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

// --- Quadric Implementation ---

Quadric::Quadric() { std::fill(a, a + 10, 0.0); }

void Quadric::setZero() { std::fill(a, a + 10, 0.0); }

Quadric Quadric::operator+(const Quadric& b) const {
    Quadric r;
    for (int i = 0; i < 10; i++) r.a[i] = a[i] + b.a[i];
    return r;
}

Quadric Quadric::operator*(double s) const {
    Quadric r;
    for (int i = 0; i < 10; i++) r.a[i] = a[i] * s;
    return r;
}

Quadric& Quadric::operator+=(const Quadric& b) {
    for (int i = 0; i < 10; i++) a[i] += b.a[i];
    return *this;
}

Quadric Quadric::FromPlane(double a, double b, double c, double d) {
    Quadric Q;
    Q.a[0] = a * a; Q.a[1] = a * b; Q.a[2] = a * c; Q.a[3] = a * d;
    Q.a[4] = b * b; Q.a[5] = b * c; Q.a[6] = b * d;
    Q.a[7] = c * c; Q.a[8] = c * d;
    Q.a[9] = d * d;
    return Q;
}

Quadric Quadric::AttributePenalty(double w) {
    Quadric Q;
    Q.a[0] = w; Q.a[4] = w; Q.a[7] = w;
    return Q;
}

double Quadric::evaluate(const Vec3& v) const {
    return a[0] * v.x * v.x + 2 * a[1] * v.x * v.y + 2 * a[2] * v.x * v.z + 2 * a[3] * v.x +
           a[4] * v.y * v.y + 2 * a[5] * v.y * v.z + 2 * a[6] * v.y +
           a[7] * v.z * v.z + 2 * a[8] * v.z +
           a[9];
}

bool Quadric::optimize(Vec3& result) const {
    double A[3][3] = { {a[0], a[1], a[2]}, {a[1], a[4], a[5]}, {a[2], a[5], a[7]} };
    double b[3] = { a[3], a[6], a[8] };

    double det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
                 A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
                 A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);

    if (std::abs(det) < 1e-12) return false;

    double invDet = 1.0 / det;
    double A_inv[3][3];

    // 手动求逆矩阵
    A_inv[0][0] = (A[1][1] * A[2][2] - A[1][2] * A[2][1]) * invDet;
    A_inv[0][1] = (A[0][2] * A[2][1] - A[0][1] * A[2][2]) * invDet;
    A_inv[0][2] = (A[0][1] * A[1][2] - A[0][2] * A[1][1]) * invDet;

    A_inv[1][0] = (A[1][2] * A[2][0] - A[1][0] * A[2][2]) * invDet;
    A_inv[1][1] = (A[0][0] * A[2][2] - A[0][2] * A[2][0]) * invDet;
    A_inv[1][2] = (A[0][2] * A[1][0] - A[0][0] * A[1][2]) * invDet;

    A_inv[2][0] = (A[1][0] * A[2][1] - A[1][1] * A[2][0]) * invDet;
    A_inv[2][1] = (A[0][1] * A[2][0] - A[0][0] * A[2][1]) * invDet;
    A_inv[2][2] = (A[0][0] * A[1][1] - A[0][1] * A[1][0]) * invDet;

    result.x = -(A_inv[0][0] * b[0] + A_inv[0][1] * b[1] + A_inv[0][2] * b[2]);
    result.y = -(A_inv[1][0] * b[0] + A_inv[1][1] * b[1] + A_inv[1][2] * b[2]);
    result.z = -(A_inv[2][0] * b[0] + A_inv[2][1] * b[1] + A_inv[2][2] * b[2]);

    return true;
}

// --- MACSimplifier Implementation ---

MACSimplifier::MACSimplifier() : w_geo(1.0), w_norm(0.1), w_uv_base(0.1) {}
MACSimplifier::~MACSimplifier() {}

void MACSimplifier::loadFromTinyGLTF(const tinygltf::Model& model, const tinygltf::Mesh& mesh) {
    if (mesh.primitives.empty()) return;
    const auto& prim = mesh.primitives[0];

    // Load Positions
    if (prim.attributes.count("POSITION")) {
        const auto& accessor = model.accessors[prim.attributes.at("POSITION")];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];
        const float* data = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);

        for (size_t i = 0; i < accessor.count; ++i) {
            Vertex v;
            v.p = Vec3(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
            v.id = (int)i;
            vertices.push_back(v);
        }
    }

    // Load Normals
    if (prim.attributes.count("NORMAL")) {
        const auto& accessor = model.accessors[prim.attributes.at("NORMAL")];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];
        const float* data = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        for (size_t i = 0; i < accessor.count; ++i) {
            normals.push_back(Vec3(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]));
        }
    } else {
        normals.resize(vertices.size(), Vec3(0, 1, 0));
    }

    // Load UVs
    if (prim.attributes.count("TEXCOORD_0")) {
        const auto& accessor = model.accessors[prim.attributes.at("TEXCOORD_0")];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];
        const float* data = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        for (size_t i = 0; i < accessor.count; ++i) {
            uvs.push_back({ data[i * 2], data[i * 2 + 1] });
        }
    } else {
        uvs.resize(vertices.size(), { 0.0, 0.0 });
    }

    // Load Indices
    if (prim.indices >= 0) {
        const auto& accessor = model.accessors[prim.indices];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];

        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const unsigned short* data = reinterpret_cast<const unsigned short*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
            for (size_t i = 0; i < accessor.count; ++i) indices.push_back(data[i]);
        } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            const unsigned int* data = reinterpret_cast<const unsigned int*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
            for (size_t i = 0; i < accessor.count; ++i) indices.push_back(data[i]);
        }
    }
}

void MACSimplifier::computeQuadrics() {
    std::cout << "[Info] Computing Quadrics..." << std::endl;

    // UV Adaptive Weight Calculation (Paper 1.1.1.2)
    double u_min = 1e9, u_max = -1e9, v_min = 1e9, v_max = -1e9;
    for (const auto& uv : uvs) {
        if (uv.first < u_min) u_min = uv.first;
        if (uv.first > u_max) u_max = uv.first;
        if (uv.second < v_min) v_min = uv.second;
        if (uv.second > v_max) v_max = uv.second;
    }
    double uv_span = std::max(u_max - u_min, v_max - v_min);
    double uv_scale_factor = (uv_span > 1e-6) ? (1.0 / uv_span) : 1.0;
    double w_uv_adaptive = w_uv_base * uv_scale_factor;

    std::cout << "[Info] Adaptive UV Weight: " << w_uv_adaptive << " (Scale Factor: " << uv_scale_factor << ")" << std::endl;

    for (auto& v : vertices) {
        v.q.setZero();
        v.q += Quadric::AttributePenalty(w_norm);
        v.q += Quadric::AttributePenalty(w_uv_adaptive);
    }

    int numFaces = indices.size() / 3;
    for (int i = 0; i < numFaces; ++i) {
        int i0 = indices[i * 3];
        int i1 = indices[i * 3 + 1];
        int i2 = indices[i * 3 + 2];

        Vec3 p0 = vertices[i0].p;
        Vec3 p1 = vertices[i1].p;
        Vec3 p2 = vertices[i2].p;

        Vec3 n = (p1 - p0).cross(p2 - p0).normalize();
        double d = -n.dot(p0);

        Quadric Kp = Quadric::FromPlane(n.x, n.y, n.z, d);
        Kp = Kp * w_geo;

        vertices[i0].q += Kp;
        vertices[i1].q += Kp;
        vertices[i2].q += Kp;

        vertices[i0].neighbors.push_back(i);
        vertices[i1].neighbors.push_back(i);
        vertices[i2].neighbors.push_back(i);
    }
}

void MACSimplifier::run(double ratio) {
    if (indices.empty()) return;

    computeQuadrics();

    std::cout << "[Info] Building Edges..." << std::endl;
    std::vector<Edge*> heap;
    std::set<std::pair<int, int>> edgeSet;
    int numFaces = indices.size() / 3;

    for (int i = 0; i < numFaces; ++i) {
        for (int j = 0; j < 3; ++j) {
            int v1 = indices[i * 3 + j];
            int v2 = indices[i * 3 + (j + 1) % 3];
            if (v1 > v2) std::swap(v1, v2);
            if (edgeSet.find({ v1, v2 }) == edgeSet.end()) {
                edgeSet.insert({ v1, v2 });

                Edge* e = new Edge();
                e->v1 = v1; e->v2 = v2;

                Quadric Qbar = vertices[v1].q + vertices[v2].q;
                if (Qbar.optimize(e->target)) {
                    e->cost = Qbar.evaluate(e->target);
                } else {
                    Vec3 mp = (vertices[v1].p + vertices[v2].p) * 0.5;
                    e->cost = Qbar.evaluate(mp);
                    e->target = mp;
                }
                heap.push_back(e);
            }
        }
    }

    std::make_heap(heap.begin(), heap.end(), [](Edge* a, Edge* b) { return a->cost > b->cost; });

    int targetFaces = (int)(numFaces * (1.0 - ratio));
    if (targetFaces < 4) targetFaces = 4;
    int currentFaces = numFaces;

    std::cout << "[Info] Start Collapse. Target Faces: " << targetFaces << std::endl;

    std::vector<int> map(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) map[i] = i;

    auto get_root = [&](int id) {
        while (id != map[id]) { map[id] = map[map[id]]; id = map[id]; }
        return id;
    };

    while (currentFaces > targetFaces && !heap.empty()) {
        std::pop_heap(heap.begin(), heap.end(), [](Edge* a, Edge* b) { return a->cost > b->cost; });
        Edge* e = heap.back();
        heap.pop_back();

        int r1 = get_root(e->v1);
        int r2 = get_root(e->v2);

        if (r1 == r2 || vertices[r1].removed || vertices[r2].removed) {
            delete e;
            continue;
        }

        vertices[r1].p = e->target;
        vertices[r1].q += vertices[r2].q;
        vertices[r2].removed = true;
        map[r2] = r1;

        currentFaces -= 2;
        delete e;
    }

    // Rebuild Indices
    std::vector<int> newIndices;
    for (int i = 0; i < numFaces; ++i) {
        int v0 = get_root(indices[i * 3]);
        int v1 = get_root(indices[i * 3 + 1]);
        int v2 = get_root(indices[i * 3 + 2]);

        if (v0 != v1 && v1 != v2 && v2 != v0) {
            newIndices.push_back(v0);
            newIndices.push_back(v1);
            newIndices.push_back(v2);
        }
    }
    indices = newIndices;
    std::cout << "[Info] Simplification Done. Final Faces: " << indices.size() / 3 << std::endl;
}

void MACSimplifier::saveToGLTF(const std::string& filename) {
    tinygltf::Model model;
    tinygltf::Scene scene;
    tinygltf::Node node;
    tinygltf::Mesh mesh;
    tinygltf::Primitive primitive;

    std::vector<float> posBuffer;
    std::vector<unsigned int> indBuffer;
    std::map<int, int> oldToNewMap;
    int newCount = 0;

    for (int idx : indices) {
        if (oldToNewMap.find(idx) == oldToNewMap.end()) {
            oldToNewMap[idx] = newCount;
            posBuffer.push_back((float)vertices[idx].p.x);
            posBuffer.push_back((float)vertices[idx].p.y);
            posBuffer.push_back((float)vertices[idx].p.z);
            newCount++;
        }
        indBuffer.push_back(oldToNewMap[idx]);
    }

    tinygltf::Buffer buffer;
    size_t posSize = posBuffer.size() * sizeof(float);
    size_t indSize = indBuffer.size() * sizeof(unsigned int);
    buffer.data.resize(posSize + indSize);
    memcpy(buffer.data.data(), posBuffer.data(), posSize);
    memcpy(buffer.data.data() + posSize, indBuffer.data(), indSize);
    model.buffers.push_back(buffer);

    tinygltf::BufferView posView;
    posView.buffer = 0; posView.byteOffset = 0; posView.byteLength = posSize; posView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    model.bufferViews.push_back(posView);

    tinygltf::BufferView indView;
    indView.buffer = 0; indView.byteOffset = posSize; indView.byteLength = indSize; indView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    model.bufferViews.push_back(indView);

    tinygltf::Accessor posAcc;
    posAcc.bufferView = 0; posAcc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    posAcc.count = newCount; posAcc.type = TINYGLTF_TYPE_VEC3;
    posAcc.maxValues = { 1000.0, 1000.0, 1000.0 }; posAcc.minValues = { -1000.0, -1000.0, -1000.0 };
    model.accessors.push_back(posAcc);

    tinygltf::Accessor indAcc;
    indAcc.bufferView = 1; indAcc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    indAcc.count = indBuffer.size(); indAcc.type = TINYGLTF_TYPE_SCALAR;
    model.accessors.push_back(indAcc);

    primitive.attributes["POSITION"] = 0;
    primitive.indices = 1;
    primitive.mode = TINYGLTF_MODE_TRIANGLES;
    mesh.primitives.push_back(primitive);
    model.meshes.push_back(mesh);

    node.mesh = 0; model.nodes.push_back(node); scene.nodes.push_back(0); model.scenes.push_back(scene); model.defaultScene = 0;

    tinygltf::TinyGLTF loader;
    loader.WriteGltfSceneToFile(&model, filename, true, true, true, false);
}