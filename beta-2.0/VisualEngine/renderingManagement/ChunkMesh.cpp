#include "ChunkMesh.h"
#include "../inputManagement/Collision.h"
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

static glm::vec3 rotatePoint(const glm::vec3& v, const glm::vec3& rotDeg) {
    if (rotDeg.x == 0.0f && rotDeg.y == 0.0f && rotDeg.z == 0.0f)
        return v;
    glm::mat4 m(1.0f);
    m = glm::rotate(m, glm::radians(rotDeg.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rotDeg.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rotDeg.z), glm::vec3(0, 0, 1));
    return glm::vec3(m * glm::vec4(v, 1.0f));
}

// ── Mesh registry ───────────────────────────────────────────────────

static std::unordered_map<std::string, RegisteredMesh> gMeshes;
static std::vector<DrawInstance> gDrawList;
static glm::vec3 gPaintPalette[16] = {};
static std::shared_ptr<Texture> gPaletteTexture;

void setPaintPalette(const glm::vec3 palette[16]) {
    for (int i = 0; i < 16; i++)
        gPaintPalette[i] = palette[i];

    unsigned char pixels[16 * 3];
    for (int i = 0; i < 16; i++) {
        pixels[i * 3 + 0] = (unsigned char)(std::clamp(palette[i].r, 0.0f, 1.0f) * 255.0f);
        pixels[i * 3 + 1] = (unsigned char)(std::clamp(palette[i].g, 0.0f, 1.0f) * 255.0f);
        pixels[i * 3 + 2] = (unsigned char)(std::clamp(palette[i].b, 0.0f, 1.0f) * 255.0f);
    }
    gPaletteTexture = std::make_shared<Texture>(pixels, 16, 1, 3);
}

const glm::vec3* getPaintPalette() {
    return gPaintPalette;
}

void registerMesh(const char* name, const VE::MeshDef& def) {
    RegisteredMesh reg;
    reg.vertices.assign(def.vertices, def.vertices + def.vertexCount * 5);
    reg.vertexCount = def.vertexCount;
    reg.indices.assign(def.indices, def.indices + def.indexCount);
    reg.indexCount = def.indexCount;
    reg.texture = def.texturePath ? std::make_shared<Texture>(def.texturePath) : nullptr;
    reg.rectangular = isMeshRectangular(def.vertices, def.vertexCount) && def.indexCount == 36;
    // Rectangular meshes default to all faces state 2 (solid)
    if (reg.rectangular)
        for (int i = 0; i < 6; i++) reg.faceState[i] = 2;
    gMeshes[name] = std::move(reg);
}

void registerMeshFromFile(const char* name, const char* meshFilePath) {
    std::ifstream file(meshFilePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to load mesh: " << meshFilePath << std::endl;
        return;
    }

    // Check for "VN" magic (normals included)
    char magic[2] = {0, 0};
    file.read(magic, 2);
    bool hasNormals = (magic[0] == 'V' && magic[1] == 'N');
    if (!hasNormals)
        file.seekg(0); // rewind if no magic

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
    reg.floatsPerVertex = hasNormals ? 8 : 5;
    reg.vertices.resize(vertexCount * reg.floatsPerVertex);
    reg.indices.resize(indexCount);

    file.read(reinterpret_cast<char*>(reg.vertices.data()), vertexCount * reg.floatsPerVertex * sizeof(float));
    file.read(reinterpret_cast<char*>(reg.indices.data()), indexCount * sizeof(uint32_t));

    std::string texPath(texturePathLen, '\0');
    file.read(texPath.data(), texturePathLen);
    if (!file && texturePathLen > 0) return;

    if (!texPath.empty() && texPath.back() == '\0') texPath.pop_back();

    reg.texture = !texPath.empty() ? std::make_shared<Texture>(texPath.c_str()) : nullptr;
    reg.rectangular = !hasNormals && isMeshRectangular(reg.vertices.data(), reg.vertexCount) && reg.indexCount == 36;
    if (reg.rectangular)
        for (int i = 0; i < 6; i++) reg.faceState[i] = 2;
    gMeshes[name] = std::move(reg);
}

void addDrawInstance(const char* meshName, float x, float y, float z,
                     float rx, float ry, float rz) {
    gDrawList.push_back({glm::vec3(x, y, z), glm::vec3(rx, ry, rz), meshName});
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

static const int neighborDir[6][3] = {
    { 0, 0, 1}, { 0, 0,-1}, {-1, 0, 0},
    { 1, 0, 0}, { 0, 1, 0}, { 0,-1, 0},
};

void registerMeshWithStates(const char* name, float* vertices, int vertexCount,
                            unsigned int* interleavedIndices, int triCount,
                            const int* faceStates,
                            const char* texturePath, bool isPrefab) {
    RegisteredMesh reg;
    reg.vertices.assign(vertices, vertices + vertexCount * 5);
    reg.vertexCount = vertexCount;
    reg.texture = texturePath ? std::make_shared<Texture>(texturePath) : nullptr;
    reg.rectangular = (vertexCount == 24);
    reg.isPrefab = isPrefab;

    // Parse interleaved: v0,v1,v2,faceDir, ...
    reg.indices.reserve(triCount * 3);
    reg.triFaceDir.reserve(triCount);
    for (int t = 0; t < triCount; t++) {
        int base = t * 4;
        reg.indices.push_back(interleavedIndices[base + 0]);
        reg.indices.push_back(interleavedIndices[base + 1]);
        reg.indices.push_back(interleavedIndices[base + 2]);
        reg.triFaceDir.push_back((int)interleavedIndices[base + 3]);
    }
    reg.indexCount = triCount * 3;

    // Copy face states
    if (faceStates) {
        for (int i = 0; i < 6; i++) reg.faceState[i] = faceStates[i];
    }

    gMeshes[name] = std::move(reg);
}

// ── Face culling + mesh merging ─────────────────────────────────────

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 10) ^ (std::hash<int>()(v.z) << 20);
    }
};

std::vector<MergedMeshEntry> buildMergedMeshes() {
    // TODO: chunk-based meshing (not yet implemented)
    return buildSingleMeshes();
}

std::vector<MergedMeshEntry> buildSingleMeshes() {
    // ── Cardinal directions matching faceState indices ──
    // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    static const glm::vec3 cardinalDirs[6] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };
    static const int oppositeFace[6] = {1, 0, 3, 2, 5, 4};

    // ── Helper: build rotation map for 90-degree-aligned rotations ──
    // rotMap[localFace] = worldFace. Returns false if not 90-aligned.
    auto buildRotMap = [&](const glm::vec3& rotation, int rotMap[6]) -> bool {
        for (int f = 0; f < 6; f++) {
            glm::vec3 rotated = rotatePoint(cardinalDirs[f], rotation);
            int best = -1;
            float bestDot = 0.5f;
            for (int c = 0; c < 6; c++) {
                float d = glm::dot(rotated, cardinalDirs[c]);
                if (d > bestDot) { bestDot = d; best = c; }
            }
            if (best < 0) return false;
            rotMap[f] = best;
        }
        return true;
    };

    // ── STEP 1: Collect all face entries ──
    // Each entry: a face in world space with its state
    struct FaceEntry {
        glm::ivec3 pos;       // block position
        int worldFace;        // world face direction (0-5)
        int state;            // 1 or 2
    };
    std::vector<FaceEntry> faceEntries;

    // Also store per-instance data needed for mesh building
    struct InstanceData {
        const DrawInstance* inst;
        std::string meshName;
        int rotMap[6];
        bool is90Aligned;
    };
    std::vector<InstanceData> instances;

    for (const auto& d : gDrawList) {
        glm::ivec3 pos((int)roundf(d.position.x), (int)roundf(d.position.y), (int)roundf(d.position.z));
        auto mit = gMeshes.find(d.meshName);
        if (mit == gMeshes.end()) continue;
        const RegisteredMesh& reg = mit->second;

        InstanceData idata;
        idata.inst = &d;
        idata.meshName = d.meshName;
        idata.is90Aligned = buildRotMap(d.rotation, idata.rotMap);

        if (idata.is90Aligned) {
            for (int f = 0; f < 6; f++) {
                int state = reg.faceState[f];
                if (state > 0) {
                    FaceEntry entry;
                    entry.pos = pos;
                    entry.worldFace = idata.rotMap[f];
                    entry.state = state;
                    faceEntries.push_back(entry);
                }
            }
        }

        instances.push_back(std::move(idata));
    }

    // ── STEP 2: Build face pair map and determine culls ──
    // Key: (position, worldFace) -> state
    // Two faces touch when face A at pos with direction D
    // pairs with face B at pos+D with direction opposite(D)
    struct FaceKey {
        glm::ivec3 pos;
        int face;
        bool operator==(const FaceKey& o) const { return pos == o.pos && face == o.face; }
    };
    struct FaceKeyHash {
        size_t operator()(const FaceKey& k) const {
            return std::hash<int>()(k.pos.x) ^ (std::hash<int>()(k.pos.y) << 10)
                 ^ (std::hash<int>()(k.pos.z) << 20) ^ (std::hash<int>()(k.face) << 28);
        }
    };
    std::unordered_map<FaceKey, int, FaceKeyHash> faceMap; // key -> state

    for (const auto& entry : faceEntries)
        faceMap[{entry.pos, entry.worldFace}] = entry.state;

    // Cull set: (position, worldFace) that should be hidden
    std::unordered_set<FaceKey, FaceKeyHash> cullSet;

    for (const auto& entry : faceEntries) {
        // Find the paired face: neighbor at pos + direction, opposite face
        glm::ivec3 neighborPos = entry.pos + glm::ivec3(glm::round(cardinalDirs[entry.worldFace]));
        int oppFace = oppositeFace[entry.worldFace];
        FaceKey neighborKey = {neighborPos, oppFace};

        auto nit = faceMap.find(neighborKey);
        if (nit == faceMap.end()) continue;

        int myState = entry.state;
        int neighborState = nit->second;

        // 2 vs 2 -> cull both (this side handled here, other side handled when it's iterated)
        if (myState == 2 && neighborState == 2)
            cullSet.insert({entry.pos, entry.worldFace});
        // 1 vs 2 -> cull the 1
        if (myState == 1 && neighborState == 2)
            cullSet.insert({entry.pos, entry.worldFace});
    }

    // ── STEP 3: Build meshes, skipping culled faces ──
    std::unordered_map<std::string, std::vector<int>> meshGroups; // meshName -> instance indices
    for (int i = 0; i < (int)instances.size(); i++)
        meshGroups[instances[i].meshName].push_back(i);

    std::vector<MergedMeshEntry> result;

    for (const auto& [name, instIndices] : meshGroups) {
        auto mit = gMeshes.find(name);
        if (mit == gMeshes.end()) continue;
        const RegisteredMesh& reg = mit->second;
        int fpv = reg.floatsPerVertex;

        std::vector<float> verts;
        std::vector<unsigned int> indices;
        struct PaintBucket { std::vector<float> verts; std::vector<unsigned int> indices; };
        PaintBucket paintBuckets[16];

        for (int ii : instIndices) {
            const InstanceData& idata = instances[ii];
            const DrawInstance* inst = idata.inst;
            glm::ivec3 pos((int)roundf(inst->position.x), (int)roundf(inst->position.y), (int)roundf(inst->position.z));
            const BlockCollider* col = getColliderAt(pos.x, pos.y, pos.z);

            // Pre-transform vertices
            std::vector<float> instVerts(reg.vertexCount * fpv);
            for (int v = 0; v < reg.vertexCount; v++) {
                int src = v * fpv;
                glm::vec3 local(reg.vertices[src], reg.vertices[src + 1], reg.vertices[src + 2]);
                glm::vec3 world = rotatePoint(local, inst->rotation) + inst->position;
                instVerts[v * fpv + 0] = world.x;
                instVerts[v * fpv + 1] = world.y;
                instVerts[v * fpv + 2] = world.z;
                instVerts[v * fpv + 3] = reg.vertices[src + 3];
                instVerts[v * fpv + 4] = reg.vertices[src + 4];
                if (fpv == 8) {
                    glm::vec3 n(reg.vertices[src + 5], reg.vertices[src + 6], reg.vertices[src + 7]);
                    glm::vec3 rn = rotatePoint(n, inst->rotation);
                    instVerts[v * fpv + 5] = rn.x;
                    instVerts[v * fpv + 6] = rn.y;
                    instVerts[v * fpv + 7] = rn.z;
                }
            }

            auto emitTri = [&](std::vector<float>& tv, std::vector<unsigned int>& ti,
                               unsigned int i0, unsigned int i1, unsigned int i2) {
                unsigned int base = (unsigned int)(tv.size() / fpv);
                for (int vi : {(int)i0, (int)i1, (int)i2})
                    for (int j = 0; j < fpv; j++)
                        tv.push_back(instVerts[vi * fpv + j]);
                ti.push_back(base); ti.push_back(base + 1); ti.push_back(base + 2);
            };

            int triCount = reg.indexCount / 3;
            for (int t = 0; t < triCount; t++) {
                unsigned int i0 = reg.indices[t * 3], i1 = reg.indices[t * 3 + 1], i2 = reg.indices[t * 3 + 2];

                // Map triangle's face direction to world space
                int fd = (t < (int)reg.triFaceDir.size()) ? reg.triFaceDir[t] : -1;
                int worldFd = (idata.is90Aligned && fd >= 0 && fd < 6) ? idata.rotMap[fd] : fd;

                // Check cull set
                if (worldFd >= 0 && worldFd < 6) {
                    if (cullSet.count({pos, worldFd}))
                        continue;
                }

                // Paint color
                int8_t colorIdx = -1;
                if (col && t < (int)col->triColors.size())
                    colorIdx = col->triColors[t];

                if (colorIdx >= 0)
                    emitTri(paintBuckets[colorIdx].verts, paintBuckets[colorIdx].indices, i0, i1, i2);
                else
                    emitTri(verts, indices, i0, i1, i2);
            }
        }

        if (!verts.empty()) {
            std::unique_ptr<Mesh> mesh;
            if (fpv == 8)
                mesh = std::make_unique<Mesh>(verts.data(), (int)(verts.size() / 8), indices.data(), (int)indices.size(), true);
            else
                mesh = std::make_unique<Mesh>(verts.data(), (int)(verts.size() / 5), indices.data(), (int)indices.size());
            if (reg.texture) mesh->setTexture(reg.texture.get());
            result.push_back({std::move(mesh), reg.texture});
        }

        for (int c = 0; c < 16; c++) {
            auto& pb = paintBuckets[c];
            if (pb.verts.empty()) continue;
            std::unique_ptr<Mesh> mesh;
            if (fpv == 8)
                mesh = std::make_unique<Mesh>(pb.verts.data(), (int)(pb.verts.size() / 8), pb.indices.data(), (int)pb.indices.size(), true);
            else
                mesh = std::make_unique<Mesh>(pb.verts.data(), (int)(pb.verts.size() / 5), pb.indices.data(), (int)pb.indices.size());
            mesh->setColor(gPaintPalette[c]);
            result.push_back({std::move(mesh), nullptr});
        }
    }

    return result;
}
