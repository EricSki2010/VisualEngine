#include "GltfExporter.h"
#include "../../../../VisualEngine/inputManagement/Collision.h"
#include "../../../../VisualEngine/renderingManagement/meshing/ChunkMesh.h"
#include <json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <iostream>

using json = nlohmann::json;

static const glm::vec3 kUnpaintedColor(0.8f);

static glm::vec3 rotatePoint(const glm::vec3& v, const glm::vec3& rotDeg) {
    if (rotDeg.x == 0.0f && rotDeg.y == 0.0f && rotDeg.z == 0.0f) return v;
    glm::mat4 m(1.0f);
    m = glm::rotate(m, glm::radians(rotDeg.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rotDeg.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rotDeg.z), glm::vec3(0, 0, 1));
    return glm::vec3(m * glm::vec4(v, 1.0f));
}

// Single output primitive: positions + normals + per-vertex colors + indices.
// Verts are emitted three-per-triangle (no shared verts) so each triangle
// can carry its own color without conflicts.
struct VertexColorPrim {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<uint32_t> indices;
};

// Emits a triangle with an explicit per-triangle normal. Callers are
// responsible for passing a correctly-oriented normal — either computed
// from winding (prefab cubes, consistent CCW-outward) or pulled from the
// source mesh's stored normal (vectorMesh VN files, where the save-time
// flip already accounts for the user's CW click convention).
static void addTriangle(VertexColorPrim& p,
                        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                        const glm::vec3& normal,
                        const glm::vec3& color) {
    int triIdx = (int)(p.indices.size() / 3);
    if (triIdx < 12) {
        std::cout << "[gltf] tri " << triIdx
                  << " v0=(" << v0.x << "," << v0.y << "," << v0.z << ")"
                  << " v1=(" << v1.x << "," << v1.y << "," << v1.z << ")"
                  << " v2=(" << v2.x << "," << v2.y << "," << v2.z << ")"
                  << " -> emitted normal=(" << normal.x << "," << normal.y << "," << normal.z << ")"
                  << std::endl;
    }
    uint32_t base = (uint32_t)(p.positions.size() / 3);
    auto push = [&](const glm::vec3& v) {
        p.positions.push_back(v.x);
        p.positions.push_back(v.y);
        p.positions.push_back(v.z);
        p.normals.push_back(normal.x);
        p.normals.push_back(normal.y);
        p.normals.push_back(normal.z);
        p.colors.push_back(color.r);
        p.colors.push_back(color.g);
        p.colors.push_back(color.b);
    };
    push(v0); push(v1); push(v2);
    p.indices.push_back(base);
    p.indices.push_back(base + 1);
    p.indices.push_back(base + 2);
}

static void padBuffer(std::vector<uint8_t>& buf) {
    while (buf.size() % 4 != 0) buf.push_back(0);
}

bool exportModelToGlb(const std::string& outPath) {
    const auto& colliders = getAllColliders();
    const glm::vec3* palette = getPaintPalette();

    // Shared face-pair cull set — same logic the editor renderer uses.
    FaceCullSet cullSet = computeFaceCullSet();

    VertexColorPrim prim;

    auto colorForIndex = [&](int paletteIndex) -> glm::vec3 {
        if (paletteIndex < 0) return kUnpaintedColor;
        return palette[paletteIndex];
    };

    int culledTris = 0;
    int emittedTris = 0;

    for (const auto& col : colliders) {
        if (col.meshName == "_ghost") continue;
        const RegisteredMesh* reg = getRegisteredMesh(col.meshName.c_str());
        if (!reg) continue;

        glm::vec3 pos = col.position;
        glm::vec3 rot = col.rotation;
        int fpv = reg->floatsPerVertex;
        glm::ivec3 ipos((int)roundf(pos.x), (int)roundf(pos.y), (int)roundf(pos.z));

        int rotMap[6];
        bool is90 = buildFaceRotMap(rot, rotMap);

        int triCount = reg->indexCount / 3;
        for (int t = 0; t < triCount; t++) {
            uint32_t i0 = reg->indices[t * 3];
            uint32_t i1 = reg->indices[t * 3 + 1];
            uint32_t i2 = reg->indices[t * 3 + 2];

            // Per-triangle face direction (cardinal 0..5, or -1 for "no face").
            int fd = (t < (int)reg->triFaceDir.size()) ? reg->triFaceDir[t] : -1;
            int worldFd = (is90 && fd >= 0 && fd < 6) ? rotMap[fd] : fd;

            // Skip faces hidden by neighbor blocks.
            if (worldFd >= 0 && worldFd < 6) {
                if (cullSet.count({ipos, worldFd})) {
                    culledTris++;
                    continue;
                }
            }

            // Look up triangle color. For rectangular meshes triColors is
            // indexed by cardinal face direction (which equals the raycast
            // face index). For non-rectangular meshes it's indexed by
            // triangle.
            int paletteIndex = -1;
            if (reg->rectangular) {
                if (fd >= 0 && fd < (int)col.triColors.size())
                    paletteIndex = col.triColors[fd];
            } else {
                if (t < (int)col.triColors.size())
                    paletteIndex = col.triColors[t];
            }
            glm::vec3 color = colorForIndex(paletteIndex);

            // Transform vertices into world space.
            glm::vec3 v0(reg->vertices[i0 * fpv],
                         reg->vertices[i0 * fpv + 1],
                         reg->vertices[i0 * fpv + 2]);
            glm::vec3 v1(reg->vertices[i1 * fpv],
                         reg->vertices[i1 * fpv + 1],
                         reg->vertices[i1 * fpv + 2]);
            glm::vec3 v2(reg->vertices[i2 * fpv],
                         reg->vertices[i2 * fpv + 1],
                         reg->vertices[i2 * fpv + 2]);
            v0 = rotatePoint(v0, rot) + pos;
            v1 = rotatePoint(v1, rot) + pos;
            v2 = rotatePoint(v2, rot) + pos;

            // Pick a normal. VN meshes (fpv == 8) carry per-vertex normals
            // in the source mesh — those already account for the vectorMesh
            // save-time flip, so we rotate and use them. Non-VN meshes
            // (prefabs, fpv == 5) have no stored normals; compute from
            // winding. Both paths respect the collider's rotation.
            glm::vec3 normal;
            if (fpv == 8) {
                glm::vec3 localN(reg->vertices[i0 * fpv + 5],
                                 reg->vertices[i0 * fpv + 6],
                                 reg->vertices[i0 * fpv + 7]);
                normal = glm::normalize(rotatePoint(localN, rot));
            } else {
                normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
            }

            addTriangle(prim, v0, v1, v2, normal, color);
            emittedTris++;
        }
    }

    if (prim.positions.empty()) {
        std::cerr << "[gltf] Nothing to export" << std::endl;
        return false;
    }

    uint32_t vertCount = (uint32_t)(prim.positions.size() / 3);
    uint32_t idxCount  = (uint32_t)prim.indices.size();

    glm::vec3 minPos( std::numeric_limits<float>::max());
    glm::vec3 maxPos(-std::numeric_limits<float>::max());
    for (uint32_t i = 0; i < vertCount; i++) {
        glm::vec3 v(prim.positions[i * 3],
                    prim.positions[i * 3 + 1],
                    prim.positions[i * 3 + 2]);
        minPos = glm::min(minPos, v);
        maxPos = glm::max(maxPos, v);
    }

    // Build the binary buffer: positions, normals, colors, indices.
    std::vector<uint8_t> binBuffer;
    struct BufferView {
        size_t offset;
        size_t length;
        int target; // 34962 = ARRAY_BUFFER, 34963 = ELEMENT_ARRAY_BUFFER
    };
    std::vector<BufferView> bufferViews;

    auto appendBufferView = [&](const void* src, size_t bytes, int target) -> int {
        BufferView bv;
        bv.offset = binBuffer.size();
        bv.length = bytes;
        bv.target = target;
        size_t oldSize = binBuffer.size();
        binBuffer.resize(oldSize + bytes);
        std::memcpy(binBuffer.data() + oldSize, src, bytes);
        padBuffer(binBuffer);
        int viewIndex = (int)bufferViews.size();
        bufferViews.push_back(bv);
        return viewIndex;
    };

    int posView   = appendBufferView(prim.positions.data(),
                                     prim.positions.size() * sizeof(float), 34962);
    int normView  = appendBufferView(prim.normals.data(),
                                     prim.normals.size() * sizeof(float),   34962);
    int colorView = appendBufferView(prim.colors.data(),
                                     prim.colors.size() * sizeof(float),    34962);
    int idxView   = appendBufferView(prim.indices.data(),
                                     prim.indices.size() * sizeof(uint32_t), 34963);

    // Build glTF JSON
    json gltf;
    gltf["asset"]  = { {"version", "2.0"}, {"generator", "modelEditor"} };
    gltf["scene"]  = 0;
    gltf["scenes"] = json::array({ {{"nodes", json::array({0})}} });
    gltf["nodes"]  = json::array({ {{"mesh", 0}} });

    // Single unlit, double-sided, vertex-colored material.
    json material;
    material["name"] = "vertexColored";
    material["pbrMetallicRoughness"] = {
        {"baseColorFactor", json::array({1.0f, 1.0f, 1.0f, 1.0f})},
        {"metallicFactor", 0.0f},
        {"roughnessFactor", 1.0f}
    };
    material["doubleSided"] = true;
    material["extensions"] = { {"KHR_materials_unlit", json::object()} };
    gltf["materials"] = json::array({ material });
    gltf["extensionsUsed"] = json::array({"KHR_materials_unlit"});

    // BufferViews
    json bufferViewsJson = json::array();
    for (const auto& bv : bufferViews) {
        bufferViewsJson.push_back({
            {"buffer", 0},
            {"byteOffset", bv.offset},
            {"byteLength", bv.length},
            {"target", bv.target}
        });
    }
    gltf["bufferViews"] = bufferViewsJson;

    // Accessors: position, normal, color, indices
    json accessors = json::array();
    int posAcc = (int)accessors.size();
    accessors.push_back({
        {"bufferView", posView},
        {"componentType", 5126}, // FLOAT
        {"count", vertCount},
        {"type", "VEC3"},
        {"min", json::array({minPos.x, minPos.y, minPos.z})},
        {"max", json::array({maxPos.x, maxPos.y, maxPos.z})}
    });

    int normAcc = (int)accessors.size();
    accessors.push_back({
        {"bufferView", normView},
        {"componentType", 5126},
        {"count", vertCount},
        {"type", "VEC3"}
    });

    int colorAcc = (int)accessors.size();
    accessors.push_back({
        {"bufferView", colorView},
        {"componentType", 5126},
        {"count", vertCount},
        {"type", "VEC3"}
    });

    int idxAcc = (int)accessors.size();
    accessors.push_back({
        {"bufferView", idxView},
        {"componentType", 5125}, // UNSIGNED_INT
        {"count", idxCount},
        {"type", "SCALAR"}
    });

    gltf["accessors"] = accessors;

    // One primitive, one material, COLOR_0 attribute
    json primitive;
    primitive["attributes"] = {
        {"POSITION", posAcc},
        {"NORMAL",   normAcc},
        {"COLOR_0",  colorAcc}
    };
    primitive["indices"]  = idxAcc;
    primitive["material"] = 0;
    gltf["meshes"] = json::array({ {{"primitives", json::array({ primitive })}} });

    gltf["buffers"] = json::array({ {{"byteLength", binBuffer.size()}} });

    // Serialize and pad JSON to 4 bytes
    std::string jsonStr = gltf.dump();
    while (jsonStr.size() % 4 != 0) jsonStr.push_back(' ');

    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        std::cerr << "[gltf] Failed to open: " << outPath << std::endl;
        return false;
    }

    uint32_t jsonChunkLen = (uint32_t)jsonStr.size();
    uint32_t binChunkLen  = (uint32_t)binBuffer.size();
    uint32_t totalLen     = 12 + 8 + jsonChunkLen + 8 + binChunkLen;

    uint32_t magic   = 0x46546C67; // "glTF"
    uint32_t version = 2;
    out.write(reinterpret_cast<const char*>(&magic),    4);
    out.write(reinterpret_cast<const char*>(&version),  4);
    out.write(reinterpret_cast<const char*>(&totalLen), 4);

    uint32_t jsonType = 0x4E4F534A; // "JSON"
    out.write(reinterpret_cast<const char*>(&jsonChunkLen), 4);
    out.write(reinterpret_cast<const char*>(&jsonType),     4);
    out.write(jsonStr.data(), jsonChunkLen);

    uint32_t binType = 0x004E4942; // "BIN\0"
    out.write(reinterpret_cast<const char*>(&binChunkLen), 4);
    out.write(reinterpret_cast<const char*>(&binType),     4);
    out.write(reinterpret_cast<const char*>(binBuffer.data()), binChunkLen);

    std::cout << "[gltf] Exported " << emittedTris << " tris ("
              << culledTris << " culled), "
              << vertCount << " verts -> " << outPath << std::endl;
    return true;
}
