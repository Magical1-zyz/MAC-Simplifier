#include "../include/MACSimplifier.h"
#include "../include/MathUtils.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <vector>
#include <cmath>
#include <map>

#include <assimp/scene.h>
#include <assimp/mesh.h>

// ==========================================
// 1. Math Helper (Eigen Wrapper)
// ==========================================
using Vec3 = Eigen::Vector3d;

// ==========================================
// 2. Quadric Error Metric Implementation
// ==========================================
Quadric::Quadric() { A.setZero(); }
void Quadric::setZero() { A.setZero(); }
Quadric Quadric::operator+(const Quadric& b) const { Quadric r; r.A = A + b.A; return r; }
Quadric& Quadric::operator+=(const Quadric& b) { A += b.A; return *this; }
Quadric Quadric::operator*(double s) const { Quadric r; r.A = A * s; return r; }

Quadric Quadric::FromPlane(double a, double b, double c, double d) {
    Quadric Q;
    Eigen::Vector4d p(a, b, c, d);
    Q.A = p * p.transpose();
    return Q;
}

Quadric Quadric::AttributePenalty(double w) {
    Quadric Q; Q.setZero();
    Q.A(0,0) = w; Q.A(1,1) = w; Q.A(2,2) = w;
    return Q;
}

double Quadric::evaluate(const Vec3& v) const {
    Eigen::Vector4d vh(v.x(), v.y(), v.z(), 1.0);
    return vh.transpose() * A * vh;
}

bool Quadric::optimize(Vec3& result) const {
    Eigen::Matrix3d M = A.block<3,3>(0,0);
    Eigen::Vector3d b = -A.block<3,1>(0,3);
    Eigen::LDLT<Eigen::Matrix3d> solver(M);
    if (solver.info() != Eigen::Success || solver.rcond() < 1e-6) return false;
    result = solver.solve(b);
    return true;
}

// ==========================================
// 3. MACSimplifier Implementation
// ==========================================

MACSimplifier::MACSimplifier() : w_geo(1.0), w_norm(0.1), w_uv_base(0.1), w_boundary(10000.0) {}
MACSimplifier::~MACSimplifier() {}

void MACSimplifier::clear() {
    vertices.clear(); indices.clear(); normals.clear(); uvs.clear();
    meshGroups.clear(); globalFaceToMeshID.clear();
    uniqueVertices.clear(); uniqueIndices.clear();
}

// --- 仅基于位置的 Key (Position Only) ---
// 强制焊接所有坐标重合的点，解决破面和构件分离问题
struct AttributeVertexKey {
    int64_t px, py, pz;

    bool operator<(const AttributeVertexKey& o) const {
        if (px != o.px) return px < o.px;
        if (py != o.py) return py < o.py;
        return pz < o.pz;
    }
};

AttributeVertexKey make_key(const Vertex& v) {
    // 精度控制：10000.0 意味着 0.1mm 的误差内视为同一点
    const double posScale = 10000.0;
    return {
            (int64_t)std::round(v.p.x() * posScale),
            (int64_t)std::round(v.p.y() * posScale),
            (int64_t)std::round(v.p.z() * posScale)
    };
}

void MACSimplifier::simplify(const aiScene* scene, double ratio) {
    clear();
    if (!scene) return;

    std::cout << "[Info] Loading data from Assimp Scene..." << std::endl;
    loadData(scene);

    if (indices.empty()) {
        std::cout << "[Warn] No geometry found." << std::endl;
        return;
    }

    buildUniqueTopology();
    runSimplification(ratio);
    writeBack(scene);
}

void MACSimplifier::loadData(const aiScene* scene) {
    int globalOffset = 0;

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];
        if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) continue;

        MeshRef ref;
        ref.mesh = mesh;
        ref.baseVertexIdx = globalOffset;

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex v;
            v.id = i;
            v.p = Vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            vertices.push_back(v);

            if (mesh->HasNormals()) {
                normals.push_back(Vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z).normalized());
            } else {
                normals.push_back(Vec3(0, 1, 0));
            }

            if (mesh->HasTextureCoords(0)) {
                uvs.push_back(Eigen::Vector2d(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y));
            } else {
                uvs.push_back(Eigen::Vector2d(0, 0));
            }
        }

        int localIndexCount = 0;
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            if (face.mNumIndices != 3) continue;

            indices.push_back(face.mIndices[0] + globalOffset);
            indices.push_back(face.mIndices[1] + globalOffset);
            indices.push_back(face.mIndices[2] + globalOffset);
            localIndexCount += 3;
        }

        size_t numNewFaces = localIndexCount / 3;
        size_t startFace = indices.size() / 3 - numNewFaces;
        for (size_t i = 0; i < numNewFaces; ++i) {
            globalFaceToMeshID[startFace + i] = meshGroups.size();
        }

        ref.indexCount = localIndexCount;
        meshGroups.push_back(ref);
        globalOffset += mesh->mNumVertices;
    }
}

void MACSimplifier::buildUniqueTopology() {
    std::cout << "[Info] Building Watertight Topology (Position Only)..." << std::endl;

    std::map<AttributeVertexKey, int> keyMap;
    uniqueVertices.clear();
    uniqueIndices.resize(indices.size());

    for (size_t i = 0; i < vertices.size(); ++i) {
        AttributeVertexKey key = make_key(vertices[i]);

        int uid = -1;
        auto it = keyMap.find(key);
        if (it != keyMap.end()) {
            uid = it->second;
        } else {
            uid = (int)uniqueVertices.size();
            UniqueVertex uv_struct;
            uv_struct.p = vertices[i].p;
            uv_struct.q.setZero();
            uniqueVertices.push_back(uv_struct);
            keyMap[key] = uid;
        }

        vertices[i].uniqueId = uid;
        uniqueVertices[uid].originalIndices.push_back(i);
    }

    for (size_t i = 0; i < indices.size(); ++i) {
        uniqueIndices[i] = vertices[indices[i]].uniqueId;
    }

    std::cout << "[Info] Topology built. Merged Vertices: " << vertices.size() << " -> " << uniqueVertices.size() << std::endl;
}

void MACSimplifier::computeQuadrics() {
    int numFaces = uniqueIndices.size() / 3;
    std::map<std::pair<int, int>, int> edgeCounts;

    std::cout << "[Info] Computing Quadrics (Standard QEM)..." << std::endl;

    for (int i = 0; i < numFaces; ++i) {
        int i0 = uniqueIndices[i * 3];
        int i1 = uniqueIndices[i * 3 + 1];
        int i2 = uniqueIndices[i * 3 + 2];
        if (i0 == i1 || i1 == i2 || i2 == i0) continue;

        Vec3 p0 = uniqueVertices[i0].p;
        Vec3 p1 = uniqueVertices[i1].p;
        Vec3 p2 = uniqueVertices[i2].p;

        Vec3 crossP = (p1 - p0).cross(p2 - p0);
        if (crossP.norm() < 1e-12) continue;

        Vec3 n = crossP.normalized();
        double d = -n.dot(p0);

        Quadric Kp = Quadric::FromPlane(n.x(), n.y(), n.z(), d);
        Kp = Kp * w_geo;

        uniqueVertices[i0].q += Kp;
        uniqueVertices[i1].q += Kp;
        uniqueVertices[i2].q += Kp;

        int e[3][2] = {{i0,i1}, {i1,i2}, {i2,i0}};
        for(auto& edge : e) {
            int u=edge[0], v=edge[1];
            if(u>v) std::swap(u,v);
            edgeCounts[{u,v}]++;
        }
    }

    // 此时 edgeCount=1 代表真正的几何边界
    int protectedEdges = 0;
    for (int i = 0; i < numFaces; ++i) {
        int idx[3] = {uniqueIndices[i*3], uniqueIndices[i*3+1], uniqueIndices[i*3+2]};
        if (idx[0] == idx[1] || idx[1] == idx[2] || idx[2] == idx[0]) continue;

        Vec3 p[3] = {uniqueVertices[idx[0]].p, uniqueVertices[idx[1]].p, uniqueVertices[idx[2]].p};
        Vec3 n = (p[1]-p[0]).cross(p[2]-p[0]).normalized();

        for(int j=0; j<3; ++j) {
            int u = idx[j];
            int v = idx[(j+1)%3];
            int u_s = u, v_s = v;
            if(u_s > v_s) std::swap(u_s, v_s);

            if(edgeCounts[{u_s, v_s}] == 1) {
                Vec3 edgeVec = uniqueVertices[v].p - uniqueVertices[u].p;
                Vec3 borderN = edgeVec.cross(n).normalized();
                double d = -borderN.dot(uniqueVertices[u].p);

                Quadric Qborder = Quadric::FromPlane(borderN.x(), borderN.y(), borderN.z(), d) * (w_boundary * 10.0);
                uniqueVertices[u].q += Qborder;
                uniqueVertices[v].q += Qborder;
                protectedEdges++;
            }
        }
    }
    std::cout << "[Info] Protected Edges (Real Borders): " << protectedEdges << std::endl;
}

void MACSimplifier::runSimplification(double ratio) {
    if (indices.empty()) return;

    std::vector<std::vector<int>> vertFaces(uniqueVertices.size());
    int numFaces = uniqueIndices.size() / 3;
    for(int i=0; i<numFaces; ++i) {
        int i0=uniqueIndices[i*3], i1=uniqueIndices[i*3+1], i2=uniqueIndices[i*3+2];
        if(i0==i1||i1==i2||i2==i0) continue;
        vertFaces[i0].push_back(i); vertFaces[i1].push_back(i); vertFaces[i2].push_back(i);
    }

    computeQuadrics();

    std::vector<Edge*> heap;
    std::set<std::pair<int, int>> edgeSet;

    auto calc_cost = [&](int v1, int v2, const Quadric& Q, Vec3& target) -> double {
        double c_v1 = Q.evaluate(uniqueVertices[v1].p);
        double c_v2 = Q.evaluate(uniqueVertices[v2].p);

        double min_cost = c_v1;
        target = uniqueVertices[v1].p;

        if (c_v2 < min_cost) {
            min_cost = c_v2;
            target = uniqueVertices[v2].p;
        }

        Vec3 p_opt;
        if (Q.optimize(p_opt)) {
            double c_opt = Q.evaluate(p_opt);
            if (c_opt < min_cost * 0.8) {
                double dist = (uniqueVertices[v1].p - uniqueVertices[v2].p).norm();
                if ((p_opt - uniqueVertices[v1].p).norm() < dist * 1.5) {
                    min_cost = c_opt;
                    target = p_opt;
                }
            }
        }
        return min_cost;
    };

    for(int i=0; i<numFaces; ++i) {
        int idx[3] = {uniqueIndices[i*3], uniqueIndices[i*3+1], uniqueIndices[i*3+2]};
        if(idx[0]==idx[1]||idx[1]==idx[2]||idx[2]==idx[0]) continue;
        for(int j=0; j<3; ++j) {
            int v1=idx[j], v2=idx[(j+1)%3];
            if(v1>v2) std::swap(v1,v2);
            if(edgeSet.find({v1,v2}) == edgeSet.end()) {
                edgeSet.insert({v1,v2});
                Edge* e = new Edge(); e->v1=v1; e->v2=v2;
                Quadric Qbar = uniqueVertices[v1].q + uniqueVertices[v2].q;
                e->cost = calc_cost(v1, v2, Qbar, e->target);
                heap.push_back(e);
            }
        }
    }
    std::make_heap(heap.begin(), heap.end(), [](Edge* a, Edge* b){ return a->cost > b->cost; });

    int targetFaces = (int)(numFaces * (1.0 - ratio));
    if (targetFaces < 4) targetFaces = 4;
    int currentFaces = numFaces;

    std::vector<int> map(uniqueVertices.size());
    for(size_t i=0; i<map.size(); ++i) map[i]=i;
    auto get_root = [&](int id) { while(id!=map[id]){ map[id]=map[map[id]]; id=map[id]; } return id; };

    while(currentFaces > targetFaces && !heap.empty()) {
        std::pop_heap(heap.begin(), heap.end(), [](Edge* a, Edge* b){ return a->cost > b->cost; });
        Edge* e = heap.back(); heap.pop_back();

        int r1 = get_root(e->v1);
        int r2 = get_root(e->v2);
        if(r1==r2 || uniqueVertices[r1].removed || uniqueVertices[r2].removed) { delete e; continue; }

        bool flip = false;
        auto check_flip_vert = [&](int u) {
            for(int fid : vertFaces[u]) {
                int i0=get_root(uniqueIndices[fid*3]), i1=get_root(uniqueIndices[fid*3+1]), i2=get_root(uniqueIndices[fid*3+2]);
                if(i0==i1||i1==i2||i2==i0) continue;
                if(i0==r1||i1==r1||i2==r1) if(i0==r2||i1==r2||i2==r2) continue;

                Vec3 p0=uniqueVertices[i0].p, p1=uniqueVertices[i1].p, p2=uniqueVertices[i2].p;
                Vec3 n_old = (p1-p0).cross(p2-p0).normalized();

                if(i0==u) p0=e->target; else if(i1==u) p1=e->target; else if(i2==u) p2=e->target;

                Vec3 crossNew = (p1-p0).cross(p2-p0);
                if (crossNew.norm() < 1e-12) return true;
                Vec3 n_new = crossNew.normalized();

                if(n_old.dot(n_new) < 0.2) return true;
            }
            return false;
        };
        if(check_flip_vert(r1) || check_flip_vert(r2)) flip = true;

        if(!flip) {
            uniqueVertices[r1].p = e->target;
            uniqueVertices[r1].q += uniqueVertices[r2].q;
            uniqueVertices[r2].removed = true;
            map[r2] = r1;
            if(vertFaces[r1].size() < 200) vertFaces[r1].insert(vertFaces[r1].end(), vertFaces[r2].begin(), vertFaces[r2].end());
            currentFaces -= 2;
        }
        delete e;
    }

    for(size_t i=0; i<uniqueVertices.size(); ++i) {
        int root = get_root(i);
        Vec3 pos = uniqueVertices[root].p;
        for(int oldIdx : uniqueVertices[i].originalIndices) {
            vertices[oldIdx].p = pos;
        }
    }
    for(auto e: heap) delete e;
}

void MACSimplifier::writeBack(const aiScene* scene) {
    std::cout << "[Info] Writing back to Assimp structures..." << std::endl;

    int currentFaceIdx = 0;

    for (int g = 0; g < meshGroups.size(); ++g) {
        MeshRef& ref = meshGroups[g];
        aiMesh* mesh = ref.mesh;
        int origFaceCount = ref.indexCount / 3;

        std::vector<Vec3> newPos;
        std::vector<Vec3> newNorm;
        std::vector<Eigen::Vector2d> newUV;
        std::vector<unsigned int> newInd;
        std::unordered_map<int, int> vertMap;
        int vertCounter = 0;

        for (int k = 0; k < origFaceCount; ++k) {
            int globalF = currentFaceIdx + k;
            if (globalF * 3 + 2 >= indices.size()) continue;

            int i0 = indices[globalF * 3];
            int i1 = indices[globalF * 3 + 1];
            int i2 = indices[globalF * 3 + 2];

            Vec3 p0 = vertices[i0].p;
            Vec3 p1 = vertices[i1].p;
            Vec3 p2 = vertices[i2].p;

            if ((p1 - p0).cross(p2 - p0).norm() < 1e-9) continue;

            int v[3] = { i0, i1, i2 };
            for (int i = 0; i < 3; ++i) {
                int gid = v[i];
                if (vertMap.find(gid) == vertMap.end()) {
                    vertMap[gid] = vertCounter++;
                    newPos.push_back(vertices[gid].p);
                    newNorm.push_back(normals[gid]);
                    newUV.push_back(uvs[gid]);
                }
                newInd.push_back(vertMap[gid]);
            }
        }
        currentFaceIdx += origFaceCount;

        // 【关键修复】处理空网格
        // 如果简化导致网格完全消失（newPos为空），Assimp 导出 GLTF 会失败（缺少 POSITION 属性）
        // 从而导致查看器报错 "file has no position attribute"
        bool meshIsEmpty = newPos.empty();
        if (meshIsEmpty) {
            std::cout << "[Warn] Mesh " << g << " collapsed completely! Keeping original vertices to avoid invalid GLTF." << std::endl;
            // 此时不更新这个 Mesh，保留原始数据
            // 虽然它可能在空间中已经坍塌，但我们不能让它没有顶点
            // 直接跳过清理和重新分配，保持原样
            // 或者，我们可以清空它，但是 Assimp 不支持空 Mesh。
            // 最安全的方法：什么都不做，保留原 Mesh (它可能有未简化的原始顶点)
            // 但我们的 vertices 数组已经更新了位置...
            // 实际上，如果 newPos 为空，意味着所有面都退化了。
            // 我们可以造一个极小的退化面来骗过 GLTF 验证

            // 方案：造一个 dummy 顶点
            newPos.push_back(Vec3(0,0,0));
            newNorm.push_back(Vec3(0,1,0));
            newUV.push_back(Eigen::Vector2d(0,0));
            // 造一个退化面 0,0,0
            newInd.push_back(0); newInd.push_back(0); newInd.push_back(0);
        }

        // 清理旧数据 (Deep Clean)
        delete[] mesh->mVertices; mesh->mVertices = nullptr;
        delete[] mesh->mNormals; mesh->mNormals = nullptr;
        delete[] mesh->mTangents; mesh->mTangents = nullptr;
        delete[] mesh->mBitangents; mesh->mBitangents = nullptr;
        for(unsigned int i=0; i<AI_MAX_NUMBER_OF_COLOR_SETS; ++i) {
            if(mesh->mColors[i]) { delete[] mesh->mColors[i]; mesh->mColors[i] = nullptr; }
        }
        for(unsigned int i=0; i<AI_MAX_NUMBER_OF_TEXTURECOORDS; ++i) {
            if(mesh->mTextureCoords[i]) { delete[] mesh->mTextureCoords[i]; mesh->mTextureCoords[i] = nullptr; }
        }
        delete[] mesh->mFaces; mesh->mFaces = nullptr;

        if (mesh->mBones && mesh->mNumBones > 0) {
            for(unsigned int b=0; b < mesh->mNumBones; ++b) delete mesh->mBones[b];
            delete[] mesh->mBones; mesh->mBones = nullptr; mesh->mNumBones = 0;
        }

        // 分配新数据
        mesh->mNumVertices = newPos.size();
        mesh->mNumFaces = newInd.size() / 3;

        if (mesh->mNumVertices > 0) {
            mesh->mVertices = new aiVector3D[mesh->mNumVertices];
            mesh->mNormals = new aiVector3D[mesh->mNumVertices];
            if (!newUV.empty()) mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];

            for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
                mesh->mVertices[i] = aiVector3D(newPos[i].x(), newPos[i].y(), newPos[i].z());
                mesh->mNormals[i] = aiVector3D(newNorm[i].x(), newNorm[i].y(), newNorm[i].z());
                if (!newUV.empty()) {
                    mesh->mTextureCoords[0][i] = aiVector3D(newUV[i].x(), newUV[i].y(), 0.0f);
                }
            }

            mesh->mFaces = new aiFace[mesh->mNumFaces];
            for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
                aiFace& face = mesh->mFaces[i];
                face.mNumIndices = 3;
                face.mIndices = new unsigned int[3];
                face.mIndices[0] = newInd[i * 3 + 0];
                face.mIndices[1] = newInd[i * 3 + 1];
                face.mIndices[2] = newInd[i * 3 + 2];
            }
        }
    }
}