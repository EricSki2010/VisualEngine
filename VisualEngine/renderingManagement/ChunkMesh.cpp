#include "ChunkMesh.h"
#include "../inputManagement/Collision.h"
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

// ── Mesh registry ───────────────────────────────────────────────────

static std::unordered_map<std::string, RegisteredMesh> gMeshes;
static std::vector<DrawInstance> gDrawList;

void registerMesh(const char* name, const VE::MeshDef& def) {
    RegisteredMesh reg;
    reg.vertices.assign(def.vertices, def.vertices + def.vertexCount * 5);
    reg.vertexCount = def.vertexCount;
    reg.indices.assign(def.indices, def.indices + def.indexCount);
    reg.indexCount = def.indexCount;
    reg.texture = def.texturePath ? std::make_shared<Texture>(def.texturePath) : nullptr;
    reg.rectangular = isMeshRectangular(def.vertices, def.vertexCount);
    gMeshes[name] = std::move(reg);
}

void registerMeshFromFile(const char* name, const char* meshFilePath) {
    std::ifstream file(meshFilePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to load mesh: " << meshFilePath << std::endl;
        return;
    }

    uint32_t vertexCount = 0, indexCount = 0, texturePathLen = 0;
    file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
    file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
    file.read(reinterpret_cast<char*>(&texturePathLen), sizeof(texturePathLen));
    if (!file) return;

    if (vertexCount > 1000000 || indexCount > 3000000 || texturePathLen > 4096) {
        std::cerr << "Mesh file has invalid header: " << meshFilePath << std::endl;
        return;
    }

    RegisteredMesh reg;
    reg.vertexCount = vertexCount;
    reg.indexCount = indexCount;
    reg.vertices.resize(vertexCount * 5);
    reg.indices.resize(indexCount);

    file.read(reinterpret_cast<char*>(reg.vertices.data()), vertexCount * 5 * sizeof(float));
    file.read(reinterpret_cast<char*>(reg.indices.data()), indexCount * sizeof(uint32_t));

    std::string texPath(texturePathLen, '\0');
    file.read(texPath.data(), texturePathLen);
    if (!file) return;

    if (!texPath.empty() && texPath.back() == '\0') texPath.pop_back();

    reg.texture = !texPath.empty() ? std::make_shared<Texture>(texPath.c_str()) : nullptr;
    reg.rectangular = isMeshRectangular(reg.vertices.data(), reg.vertexCount);
    gMeshes[name] = std::move(reg);
}

void addDrawInstance(const char* meshName, float x, float y, float z) {
    gDrawList.push_back({glm::vec3(x, y, z), meshName});
}

void removeDrawInstance(float x, float y, float z) {
    glm::ivec3 target((int)roundf(x), (int)roundf(y), (int)roundf(z));
    gDrawList.erase(
        std::remove_if(gDrawList.begin(), gDrawList.end(), [&](const DrawInstance& d) {
            return glm::ivec3((int)roundf(d.position.x), (int)roundf(d.position.y), (int)roundf(d.position.z)) == target;
        }),
        gDrawList.end()
    );
}

void clearDrawInstances() {
    gDrawList.clear();
}

void clearMeshData() {
    gMeshes.clear();
    gDrawList.clear();
}

const RegisteredMesh* getRegisteredMesh(const char* name) {
    auto it = gMeshes.find(name);
    return (it != gMeshes.end()) ? &it->second : nullptr;
}

// ── Face culling + mesh merging ─────────────────────────────────────

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 10) ^ (std::hash<int>()(v.z) << 20);
    }
};

static const int neighborDir[6][3] = {
    { 0, 0, 1}, { 0, 0,-1}, {-1, 0, 0},
    { 1, 0, 0}, { 0, 1, 0}, { 0,-1, 0},
};

static const int faceVertStart[6] = {0, 4, 8, 12, 16, 20};

static const unsigned int faceIdx[6][6] = {
    {0,1,2, 2,3,0},
    {0,2,1, 2,0,3},
    {0,1,2, 2,3,0},
    {0,1,2, 2,3,0},
    {0,1,2, 2,3,0},
    {0,1,2, 2,3,0},
};

std::vector<MergedMeshEntry> buildMergedMeshes() {
    // TODO: chunk-based meshing (not yet implemented)
    return buildSingleMeshes();
}

std::vector<MergedMeshEntry> buildSingleMeshes() {
    std::unordered_set<glm::ivec3, IVec3Hash> occupied;
    for (const auto& d : gDrawList)
        occupied.insert(glm::ivec3((int)roundf(d.position.x), (int)roundf(d.position.y), (int)roundf(d.position.z)));

    std::unordered_map<std::string, std::vector<const DrawInstance*>> groups;
    for (const auto& d : gDrawList)
        groups[d.meshName].push_back(&d);

    std::vector<MergedMeshEntry> result;

    for (const auto& [name, group] : groups) {
        auto mit = gMeshes.find(name);
        if (mit == gMeshes.end()) continue;
        const RegisteredMesh& reg = mit->second;

        std::vector<float> verts;
        std::vector<unsigned int> indices;

        for (const DrawInstance* inst : group) {
            glm::ivec3 pos((int)roundf(inst->position.x), (int)roundf(inst->position.y), (int)roundf(inst->position.z));

            for (int f = 0; f < 6; f++) {
                glm::ivec3 neighbor = pos + glm::ivec3(neighborDir[f][0], neighborDir[f][1], neighborDir[f][2]);
                if (occupied.count(neighbor)) continue;

                unsigned int baseIdx = (unsigned int)(verts.size() / 5);

                for (int v = 0; v < 4; v++) {
                    int src = (faceVertStart[f] + v) * 5;
                    verts.push_back(reg.vertices[src + 0] + inst->position.x);
                    verts.push_back(reg.vertices[src + 1] + inst->position.y);
                    verts.push_back(reg.vertices[src + 2] + inst->position.z);
                    verts.push_back(reg.vertices[src + 3]);
                    verts.push_back(reg.vertices[src + 4]);
                }

                for (int i = 0; i < 6; i++)
                    indices.push_back(baseIdx + faceIdx[f][i]);
            }
        }

        if (!verts.empty()) {
            auto mesh = std::make_unique<Mesh>(verts.data(), (int)(verts.size() / 5), indices.data(), (int)indices.size());
            if (reg.texture) mesh->setTexture(reg.texture.get());
            result.push_back({std::move(mesh), reg.texture});
        }
    }

    return result;
}
