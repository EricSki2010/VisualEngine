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
static glm::vec3 gPaintPalette[8] = {};
static std::shared_ptr<Texture> gPaletteTexture;

void setPaintPalette(const glm::vec3 palette[8]) {
    for (int i = 0; i < 8; i++)
        gPaintPalette[i] = palette[i];

    unsigned char pixels[8 * 3];
    for (int i = 0; i < 8; i++) {
        pixels[i * 3 + 0] = (unsigned char)(std::clamp(palette[i].r, 0.0f, 1.0f) * 255.0f);
        pixels[i * 3 + 1] = (unsigned char)(std::clamp(palette[i].g, 0.0f, 1.0f) * 255.0f);
        pixels[i * 3 + 2] = (unsigned char)(std::clamp(palette[i].b, 0.0f, 1.0f) * 255.0f);
    }
    gPaletteTexture = std::make_shared<Texture>(pixels, 8, 1, 3);
}

void registerMesh(const char* name, const VE::MeshDef& def) {
    RegisteredMesh reg;
    reg.vertices.assign(def.vertices, def.vertices + def.vertexCount * 5);
    reg.vertexCount = def.vertexCount;
    reg.indices.assign(def.indices, def.indices + def.indexCount);
    reg.indexCount = def.indexCount;
    reg.texture = def.texturePath ? std::make_shared<Texture>(def.texturePath) : nullptr;
    reg.rectangular = isMeshRectangular(def.vertices, def.vertexCount) && def.indexCount == 36;
    // Rectangular meshes default to all faces solid
    if (reg.rectangular)
        for (int i = 0; i < 6; i++) reg.solidFaces[i] = true;
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
        for (int i = 0; i < 6; i++) reg.solidFaces[i] = true;
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

void setMeshSolidFaces(const char* name, bool faces[6]) {
    auto it = gMeshes.find(name);
    if (it != gMeshes.end())
        for (int i = 0; i < 6; i++) it->second.solidFaces[i] = faces[i];
}

static const int neighborDir[6][3] = {
    { 0, 0, 1}, { 0, 0,-1}, {-1, 0, 0},
    { 1, 0, 0}, { 0, 1, 0}, { 0,-1, 0},
};

// neighborDir mapping to solidFaces index
// neighborDir: 0=+Z,1=-Z,2=-X,3=+X,4=+Y,5=-Y
// solidFaces:  0=+X,1=-X,2=+Y,3=-Y,4=+Z,5=-Z
static const int ndirToSolid[6] = {4, 5, 1, 0, 2, 3};

static void deriveSolidFaces(RegisteredMesh& reg) {
    for (int i = 0; i < 6; i++) reg.solidFaces[i] = false;

    for (int t = 0; t < (int)reg.triCullState.size(); t++) {
        if (reg.triCullState[t] != 2) continue;

        // Compute triangle normal to find face direction
        int i0 = reg.indices[t * 3], i1 = reg.indices[t * 3 + 1], i2 = reg.indices[t * 3 + 2];
        glm::vec3 v0(reg.vertices[i0*5], reg.vertices[i0*5+1], reg.vertices[i0*5+2]);
        glm::vec3 v1(reg.vertices[i1*5], reg.vertices[i1*5+1], reg.vertices[i1*5+2]);
        glm::vec3 v2(reg.vertices[i2*5], reg.vertices[i2*5+1], reg.vertices[i2*5+2]);
        glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

        float bestDot = 0.5f;
        for (int f = 0; f < 6; f++) {
            glm::vec3 fn((float)neighborDir[f][0], (float)neighborDir[f][1], (float)neighborDir[f][2]);
            if (glm::dot(normal, fn) > bestDot) {
                reg.solidFaces[ndirToSolid[f]] = true;
            }
        }
    }
}

void registerMeshWithStates(const char* name, float* vertices, int vertexCount,
                            unsigned int* interleavedIndices, int triCount,
                            const char* texturePath) {
    RegisteredMesh reg;
    reg.vertices.assign(vertices, vertices + vertexCount * 5);
    reg.vertexCount = vertexCount;
    reg.texture = texturePath ? std::make_shared<Texture>(texturePath) : nullptr;
    reg.rectangular = (vertexCount == 24);

    // Parse interleaved: v0,v1,v2,state, v0,v1,v2,state, ...
    reg.indices.reserve(triCount * 3);
    reg.triCullState.reserve(triCount);
    for (int t = 0; t < triCount; t++) {
        int base = t * 4;
        reg.indices.push_back(interleavedIndices[base + 0]);
        reg.indices.push_back(interleavedIndices[base + 1]);
        reg.indices.push_back(interleavedIndices[base + 2]);
        reg.triCullState.push_back((int)interleavedIndices[base + 3]);
    }
    reg.indexCount = triCount * 3;

    deriveSolidFaces(reg);
    gMeshes[name] = std::move(reg);
}

// ── Face culling + mesh merging ─────────────────────────────────────

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 10) ^ (std::hash<int>()(v.z) << 20);
    }
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
    // Build map of position -> mesh name for neighbor checks
    std::unordered_map<glm::ivec3, std::string, IVec3Hash> occupiedMesh;
    for (const auto& d : gDrawList)
        occupiedMesh[glm::ivec3((int)roundf(d.position.x), (int)roundf(d.position.y), (int)roundf(d.position.z))] = d.meshName;

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

        // Opposite face index: +X<->-X, +Y<->-Y, +Z<->-Z
        static const int oppositeFace[6] = {1, 0, 3, 2, 5, 4};

        // Per-color paint buckets: [colorIdx] -> {verts, indices}
        struct PaintBucket { std::vector<float> verts; std::vector<unsigned int> indices; };
        PaintBucket paintBuckets[8];

        if (reg.rectangular) {
            // Rectangular mesh: face cull using solidFaces
            for (const DrawInstance* inst : group) {
                bool hasRot = (inst->rotation.x != 0.0f || inst->rotation.y != 0.0f || inst->rotation.z != 0.0f);
                glm::ivec3 pos((int)roundf(inst->position.x), (int)roundf(inst->position.y), (int)roundf(inst->position.z));
                const BlockCollider* col = getColliderAt(pos.x, pos.y, pos.z);

                for (int f = 0; f < 6; f++) {
                    int solidIdx = ndirToSolid[f];

                    // Skip face culling for rotated instances
                    if (!hasRot && reg.solidFaces[solidIdx]) {
                        glm::ivec3 neighbor = pos + glm::ivec3(neighborDir[f][0], neighborDir[f][1], neighborDir[f][2]);
                        auto nit = occupiedMesh.find(neighbor);
                        if (nit != occupiedMesh.end()) {
                            auto nmit = gMeshes.find(nit->second);
                            if (nmit != gMeshes.end() && nmit->second.solidFaces[oppositeFace[solidIdx]])
                                continue;
                        }
                    }

                    // Map mesh face order to raycast face order
                    // Mesh: 0=+Z, 1=-Z, 2=-X, 3=+X, 4=+Y, 5=-Y
                    // Raycast: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
                    static const int meshToRayFace[6] = {4, 5, 1, 0, 2, 3};
                    int rayFace = meshToRayFace[f];
                    int8_t colorIdx = -1;
                    if (col && rayFace < (int)col->triColors.size())
                        colorIdx = col->triColors[rayFace];

                    auto& tv = (colorIdx >= 0) ? paintBuckets[colorIdx].verts : verts;
                    auto& ti = (colorIdx >= 0) ? paintBuckets[colorIdx].indices : indices;

                    unsigned int baseIdx = (unsigned int)(tv.size() / 5);

                    for (int v = 0; v < 4; v++) {
                        int src = (faceVertStart[f] + v) * 5;
                        glm::vec3 local(reg.vertices[src + 0], reg.vertices[src + 1], reg.vertices[src + 2]);
                        glm::vec3 world = rotatePoint(local, inst->rotation) + inst->position;
                        tv.push_back(world.x);
                        tv.push_back(world.y);
                        tv.push_back(world.z);
                        tv.push_back(reg.vertices[src + 3]);
                        tv.push_back(reg.vertices[src + 4]);
                    }

                    for (int i = 0; i < 6; i++)
                        ti.push_back(baseIdx + faceIdx[f][i]);
                }
            }
        } else {
            // Non-rectangular mesh: per-triangle culling using triCullState
            int fpv = reg.floatsPerVertex; // 5 or 8
            for (const DrawInstance* inst : group) {
                bool hasRot = (inst->rotation.x != 0.0f || inst->rotation.y != 0.0f || inst->rotation.z != 0.0f);
                glm::ivec3 pos((int)roundf(inst->position.x), (int)roundf(inst->position.y), (int)roundf(inst->position.z));
                const BlockCollider* col = getColliderAt(pos.x, pos.y, pos.z);

                // Pre-transform all vertices for this instance
                std::vector<float> instVerts(reg.vertexCount * fpv);
                for (int v = 0; v < reg.vertexCount; v++) {
                    int src = v * fpv;
                    glm::vec3 local(reg.vertices[src + 0], reg.vertices[src + 1], reg.vertices[src + 2]);
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

                // Helper to emit a triangle to a target buffer
                auto emitTri = [&](std::vector<float>& tv, std::vector<unsigned int>& ti,
                                   unsigned int i0, unsigned int i1, unsigned int i2) {
                    unsigned int base = (unsigned int)(tv.size() / fpv);
                    for (int vi : {(int)i0, (int)i1, (int)i2}) {
                        for (int j = 0; j < fpv; j++)
                            tv.push_back(instVerts[vi * fpv + j]);
                    }
                    ti.push_back(base); ti.push_back(base + 1); ti.push_back(base + 2);
                };

                int triCount = reg.indexCount / 3;
                for (int t = 0; t < triCount; t++) {
                    unsigned int i0 = reg.indices[t*3], i1 = reg.indices[t*3+1], i2 = reg.indices[t*3+2];
                    int state = (t < (int)reg.triCullState.size()) ? reg.triCullState[t] : 0;

                    bool draw = true;
                    if (state != 0 && !hasRot) {
                        glm::vec3 v0(reg.vertices[i0*fpv], reg.vertices[i0*fpv+1], reg.vertices[i0*fpv+2]);
                        glm::vec3 v1(reg.vertices[i1*fpv], reg.vertices[i1*fpv+1], reg.vertices[i1*fpv+2]);
                        glm::vec3 v2(reg.vertices[i2*fpv], reg.vertices[i2*fpv+1], reg.vertices[i2*fpv+2]);
                        glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                        int faceDir = -1;
                        float bestDot = 0.5f;
                        for (int f = 0; f < 6; f++) {
                            glm::vec3 fn((float)neighborDir[f][0], (float)neighborDir[f][1], (float)neighborDir[f][2]);
                            float d = glm::dot(normal, fn);
                            if (d > bestDot) { bestDot = d; faceDir = f; }
                        }
                        if (faceDir >= 0) {
                            int solidIdx = ndirToSolid[faceDir];
                            glm::ivec3 neighbor = pos + glm::ivec3(neighborDir[faceDir][0], neighborDir[faceDir][1], neighborDir[faceDir][2]);
                            auto nit = occupiedMesh.find(neighbor);
                            if (nit != occupiedMesh.end()) {
                                auto nmit = gMeshes.find(nit->second);
                                if (nmit != gMeshes.end() && nmit->second.solidFaces[oppositeFace[solidIdx]])
                                    draw = false;
                            }
                        }
                    }

                    if (draw) {
                        int8_t colorIdx = -1;
                        if (col && t < (int)col->triColors.size())
                            colorIdx = col->triColors[t];
                        if (colorIdx >= 0)
                            emitTri(paintBuckets[colorIdx].verts, paintBuckets[colorIdx].indices, i0, i1, i2);
                        else
                            emitTri(verts, indices, i0, i1, i2);
                    }
                }
            }
        }

        if (!verts.empty()) {
            int fpv = reg.floatsPerVertex;
            std::unique_ptr<Mesh> mesh;
            if (fpv == 8)
                mesh = std::make_unique<Mesh>(verts.data(), (int)(verts.size() / 8), indices.data(), (int)indices.size(), true);
            else
                mesh = std::make_unique<Mesh>(verts.data(), (int)(verts.size() / 5), indices.data(), (int)indices.size());
            if (reg.texture) mesh->setTexture(reg.texture.get());
            result.push_back({std::move(mesh), reg.texture});
        }

        // Emit painted triangle batches (one mesh per color)
        for (int c = 0; c < 8; c++) {
            auto& pb = paintBuckets[c];
            if (pb.verts.empty()) continue;
            int fpv = reg.rectangular ? 5 : reg.floatsPerVertex;
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
