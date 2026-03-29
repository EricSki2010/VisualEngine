#ifndef REGISTRY_H
#define REGISTRY_H

#include "memory.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>

// ── Base definition ─────────────────────────────────────────────────

struct BaseDef {
    int32_t id = 0;
    std::string name;
    std::string modelFile;
    std::string vertexFile;
    bool hasVertexData = false;

    virtual ~BaseDef() = default;
};

// ── Generic registry ────────────────────────────────────────────────

template <typename T>
struct Registry {
    std::unordered_map<std::string, T> entries;
    std::unordered_map<int32_t, std::string> idToName;
    int32_t nextID = 1;  // 0 = empty/no object
    const char* tag = "REGISTRY";

    const T* get(const std::string& name) const {
        auto it = entries.find(name);
        return it != entries.end() ? &it->second : nullptr;
    }

    const T* getByID(int32_t id) const {
        auto it = idToName.find(id);
        if (it == idToName.end()) return nullptr;
        return get(it->second);
    }

    bool has(const std::string& name) const {
        return entries.count(name) > 0;
    }

    void add(const std::string& name, const T& def) {
        T d = def;
        d.id = nextID++;
        entries[name] = d;
        idToName[d.id] = name;
    }

    // Scan a folder for .glb files (and matching .vertex files) and register each by filename
    void scanModels(const std::string& folder) {
        if (!std::filesystem::exists(folder)) {
            printf("%s: Folder not found: %s\n", tag, folder.c_str());
            return;
        }
        int count = 0;
        int vertexCount = 0;
        for (auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.path().extension() == ".glb") {
                std::string name = entry.path().stem().string();
                if (!has(name)) {
                    T def;
                    def.name = name;
                    def.modelFile = entry.path().string();

                    std::string vPath = folder + "/" + name + ".vertex";
                    if (std::filesystem::exists(vPath)) {
                        def.vertexFile = vPath;
                        def.hasVertexData = true;
                        vertexCount++;
                    }

                    add(name, def);
                    count++;
                }
            }
        }
        printf("%s: Found %d models (%d with vertex data) in %s\n", tag, count, vertexCount, folder.c_str());
    }
};

// ── Block definition ────────────────────────────────────────────────

struct BlockDef : BaseDef {
    std::string collisionType = "solid";
    unsigned char r = 128, g = 128, b = 128, a = 255;
    uint8_t solidFaces = FACE_ALL;
};

// Calculate solidFaces from .vertex file
// Checks each of the 6 cube walls: if triangles on that wall cover the full face, it's solid
inline uint8_t calcSolidFaces(const std::string& vertexFile) {
    std::ifstream file(vertexFile, std::ios::binary);
    if (!file) return FACE_ALL;

    uint32_t vertCount = 0, triCount = 0;
    file.read(reinterpret_cast<char*>(&vertCount), sizeof(vertCount));
    file.read(reinterpret_cast<char*>(&triCount), sizeof(triCount));

    if (!file || vertCount > 100000 || triCount > 200000) {
        printf("WARNING: Invalid .vertex file: %s\n", vertexFile.c_str());
        return FACE_ALL;
    }

    struct Vec3f { float x, y, z; };
    std::vector<Vec3f> verts(vertCount);
    for (uint32_t i = 0; i < vertCount; i++)
        file.read(reinterpret_cast<char*>(&verts[i]), sizeof(Vec3f));

    struct Tri { uint32_t v0, v1, v2; };
    std::vector<Tri> tris(triCount);
    for (uint32_t i = 0; i < triCount; i++)
        file.read(reinterpret_cast<char*>(&tris[i]), sizeof(Tri));

    if (!file) {
        printf("WARNING: Truncated .vertex file: %s\n", vertexFile.c_str());
        return FACE_ALL;
    }

    // Single-pass: bin each triangle into its face plane and accumulate area
    // Face planes: +X(x=1)=0x01, -X(x=0)=0x02, +Y(y=1)=0x04, -Y(y=0)=0x08, +Z(z=1)=0x10, -Z(z=0)=0x20
    const float eps = 0.001f;
    float area[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    // face index: 0=+X(1), 1=-X(0), 2=+Y(1), 3=-Y(0), 4=+Z(1), 5=-Z(0)
    // axis:       0       0        1        1        2        2
    // plane val:  1       0        1        0        1        0

    for (const auto& tri : tris) {
        if (tri.v0 >= vertCount || tri.v1 >= vertCount || tri.v2 >= vertCount)
            continue;
        const Vec3f& a = verts[tri.v0];
        const Vec3f& b = verts[tri.v1];
        const Vec3f& c = verts[tri.v2];

        float coords[3][3] = {
            {a.x, a.y, a.z},
            {b.x, b.y, b.z},
            {c.x, c.y, c.z}
        };

        // Check each axis for coplanar triangles
        for (int ax = 0; ax < 3; ax++) {
            float va = coords[0][ax], vb = coords[1][ax], vc = coords[2][ax];

            // Check plane at 1.0 (positive face)
            if (std::fabs(va - 1.0f) < eps && std::fabs(vb - 1.0f) < eps && std::fabs(vc - 1.0f) < eps) {
                int u = (ax + 1) % 3, v = (ax + 2) % 3;
                area[ax * 2] += std::fabs((coords[1][u] - coords[0][u]) * (coords[2][v] - coords[0][v])
                                        - (coords[2][u] - coords[0][u]) * (coords[1][v] - coords[0][v])) * 0.5f;
            }
            // Check plane at 0.0 (negative face)
            if (std::fabs(va) < eps && std::fabs(vb) < eps && std::fabs(vc) < eps) {
                int u = (ax + 1) % 3, v = (ax + 2) % 3;
                area[ax * 2 + 1] += std::fabs((coords[1][u] - coords[0][u]) * (coords[2][v] - coords[0][v])
                                             - (coords[2][u] - coords[0][u]) * (coords[1][v] - coords[0][v])) * 0.5f;
            }
        }
    }

    uint8_t solid = 0;
    const uint8_t bits[6] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
    for (int f = 0; f < 6; f++) {
        if (area[f] >= 1.0f - eps)
            solid |= bits[f];
    }

    return solid;
}

using BlockRegistry = Registry<BlockDef>;

// ── Entity definition ───────────────────────────────────────────────

struct EntityDef : BaseDef {
};

using EntityRegistry = Registry<EntityDef>;

// ── Paths ───────────────────────────────────────────────────────────

inline std::filesystem::path getEntitiesPath() {
    return getBasePath() / "assets" / "entities";
}

inline std::filesystem::path getBlockModelsPath() {
    return getEntitiesPath() / "blockModels";
}

inline std::filesystem::path getBlocksPath() {
    return getEntitiesPath() / "blocks";
}

// ── Load all registries ─────────────────────────────────────────────

inline void loadRegistries(BlockRegistry& blocks, EntityRegistry& entities) {
    blocks.tag = "BLOCKS";
    blocks.scanModels(getBlockModelsPath().string());

    // Calculate solidFaces from vertex data
    for (auto& [name, def] : blocks.entries) {
        if (def.hasVertexData) {
            def.solidFaces = calcSolidFaces(def.vertexFile);
            printf("  %s: solidFaces = 0x%02X\n", name.c_str(), def.solidFaces);
        }
    }

    entities.tag = "ENTITIES";
    entities.scanModels(getEntitiesPath().string());
}

#endif // REGISTRY_H
