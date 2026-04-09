#include "GltfExporter.h"
#include "../../../VisualEngine/inputManagement/Collision.h"
#include "../../../VisualEngine/renderingManagement/ChunkMesh.h"
#include <json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <iostream>

using json = nlohmann::json;

// Cube face vertex layout in ChunkMesh.cpp:
// f=0 -> +Z, f=1 -> -Z, f=2 -> -X, f=3 -> +X, f=4 -> +Y, f=5 -> -Y
static const int kFaceVertStart[6] = {0, 4, 8, 12, 16, 20};
// mesh face order -> raycast face order (how triColors is indexed for rectangular meshes)
static const int kMeshToRayFace[6] = {4, 5, 1, 0, 2, 3};
// Two-triangle layout per quad face
static const unsigned int kFaceIdx[6] = {0, 1, 2, 2, 3, 0};

static glm::vec3 rotatePoint(const glm::vec3& v, const glm::vec3& rotDeg) {
    if (rotDeg.x == 0.0f && rotDeg.y == 0.0f && rotDeg.z == 0.0f) return v;
    glm::mat4 m(1.0f);
    m = glm::rotate(m, glm::radians(rotDeg.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rotDeg.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rotDeg.z), glm::vec3(0, 0, 1));
    return glm::vec3(m * glm::vec4(v, 1.0f));
}

// Per-color primitive data: position + normal + indices
// Color index -1 = unpainted (goes in its own bucket at slot 16)
struct Primitive {
    std::vector<float> positions; // vec3
    std::vector<float> normals;   // vec3
    std::vector<uint32_t> indices;
};

static void addTriangle(Primitive& p, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {
    glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    uint32_t base = (uint32_t)(p.positions.size() / 3);
    auto push = [&](const glm::vec3& v) {
        p.positions.push_back(v.x);
        p.positions.push_back(v.y);
        p.positions.push_back(v.z);
        p.normals.push_back(normal.x);
        p.normals.push_back(normal.y);
        p.normals.push_back(normal.z);
    };
    push(v0); push(v1); push(v2);
    p.indices.push_back(base);
    p.indices.push_back(base + 1);
    p.indices.push_back(base + 2);
}

// Pad buffer to 4-byte alignment
static void padBuffer(std::vector<uint8_t>& buf) {
    while (buf.size() % 4 != 0) buf.push_back(0);
}

bool exportModelToGlb(const std::string& outPath) {
    const auto& colliders = getAllColliders();
    const glm::vec3* palette = getPaintPalette();

    // 17 buckets: 16 palette colors + 1 for unpainted
    Primitive buckets[17];
    const int UNPAINTED = 16;

    for (const auto& col : colliders) {
        if (col.meshName == "_ghost") continue;
        const RegisteredMesh* reg = getRegisteredMesh(col.meshName.c_str());
        if (!reg) continue;

        glm::vec3 pos = col.position;
        glm::vec3 rot = col.rotation;
        int fpv = reg->floatsPerVertex;

        if (reg->rectangular) {
            // 6 faces × 2 triangles per face
            for (int f = 0; f < 6; f++) {
                glm::vec3 quad[4];
                for (int v = 0; v < 4; v++) {
                    int src = (kFaceVertStart[f] + v) * fpv;
                    glm::vec3 local(reg->vertices[src + 0], reg->vertices[src + 1], reg->vertices[src + 2]);
                    quad[v] = rotatePoint(local, rot) + pos;
                }
                // Pick bucket: triColors indexed by raycast face order
                int rayFace = kMeshToRayFace[f];
                int bucket = UNPAINTED;
                if (rayFace < (int)col.triColors.size() && col.triColors[rayFace] >= 0)
                    bucket = col.triColors[rayFace];
                // Two triangles: (0,1,2) and (2,3,0)
                addTriangle(buckets[bucket], quad[0], quad[1], quad[2]);
                addTriangle(buckets[bucket], quad[2], quad[3], quad[0]);
            }
        } else {
            // Non-rectangular: iterate triangles from the registered mesh
            int triCount = reg->indexCount / 3;
            for (int t = 0; t < triCount; t++) {
                uint32_t i0 = reg->indices[t * 3];
                uint32_t i1 = reg->indices[t * 3 + 1];
                uint32_t i2 = reg->indices[t * 3 + 2];
                glm::vec3 v0(reg->vertices[i0 * fpv], reg->vertices[i0 * fpv + 1], reg->vertices[i0 * fpv + 2]);
                glm::vec3 v1(reg->vertices[i1 * fpv], reg->vertices[i1 * fpv + 1], reg->vertices[i1 * fpv + 2]);
                glm::vec3 v2(reg->vertices[i2 * fpv], reg->vertices[i2 * fpv + 1], reg->vertices[i2 * fpv + 2]);
                v0 = rotatePoint(v0, rot) + pos;
                v1 = rotatePoint(v1, rot) + pos;
                v2 = rotatePoint(v2, rot) + pos;

                int bucket = UNPAINTED;
                if (t < (int)col.triColors.size() && col.triColors[t] >= 0)
                    bucket = col.triColors[t];
                addTriangle(buckets[bucket], v0, v1, v2);
            }
        }
    }

    // Collect non-empty buckets
    std::vector<int> usedBuckets;
    for (int i = 0; i < 17; i++)
        if (!buckets[i].positions.empty())
            usedBuckets.push_back(i);

    if (usedBuckets.empty()) {
        std::cerr << "[gltf] Nothing to export" << std::endl;
        return false;
    }

    // Build binary buffer: for each primitive, write positions then normals then indices
    std::vector<uint8_t> binBuffer;
    struct BufferView {
        size_t offset;
        size_t length;
        int target; // 34962 = ARRAY_BUFFER, 34963 = ELEMENT_ARRAY_BUFFER
    };
    std::vector<BufferView> bufferViews;
    // Per primitive: posView, normView, idxView, vertCount, idxCount, min/max pos
    struct PrimMeta {
        int posView, normView, idxView;
        uint32_t vertCount, idxCount;
        glm::vec3 minPos, maxPos;
        int bucket;
    };
    std::vector<PrimMeta> primMetas;

    for (int bucket : usedBuckets) {
        auto& p = buckets[bucket];
        PrimMeta meta;
        meta.bucket = bucket;
        meta.vertCount = (uint32_t)(p.positions.size() / 3);
        meta.idxCount = (uint32_t)p.indices.size();

        // Compute min/max for POSITION accessor
        meta.minPos = glm::vec3(std::numeric_limits<float>::max());
        meta.maxPos = glm::vec3(std::numeric_limits<float>::lowest());
        for (uint32_t i = 0; i < meta.vertCount; i++) {
            glm::vec3 v(p.positions[i * 3], p.positions[i * 3 + 1], p.positions[i * 3 + 2]);
            meta.minPos = glm::min(meta.minPos, v);
            meta.maxPos = glm::max(meta.maxPos, v);
        }

        // Positions
        meta.posView = (int)bufferViews.size();
        BufferView bv;
        bv.offset = binBuffer.size();
        bv.length = p.positions.size() * sizeof(float);
        bv.target = 34962;
        bufferViews.push_back(bv);
        size_t oldSize = binBuffer.size();
        binBuffer.resize(oldSize + bv.length);
        std::memcpy(binBuffer.data() + oldSize, p.positions.data(), bv.length);
        padBuffer(binBuffer);

        // Normals
        meta.normView = (int)bufferViews.size();
        bv.offset = binBuffer.size();
        bv.length = p.normals.size() * sizeof(float);
        bv.target = 34962;
        bufferViews.push_back(bv);
        oldSize = binBuffer.size();
        binBuffer.resize(oldSize + bv.length);
        std::memcpy(binBuffer.data() + oldSize, p.normals.data(), bv.length);
        padBuffer(binBuffer);

        // Indices
        meta.idxView = (int)bufferViews.size();
        bv.offset = binBuffer.size();
        bv.length = p.indices.size() * sizeof(uint32_t);
        bv.target = 34963;
        bufferViews.push_back(bv);
        oldSize = binBuffer.size();
        binBuffer.resize(oldSize + bv.length);
        std::memcpy(binBuffer.data() + oldSize, p.indices.data(), bv.length);
        padBuffer(binBuffer);

        primMetas.push_back(meta);
    }

    // Build glTF JSON
    json gltf;
    gltf["asset"] = { {"version", "2.0"}, {"generator", "modelEditor"} };
    gltf["scene"] = 0;
    gltf["scenes"] = json::array({ {{"nodes", json::array({0})}} });
    gltf["nodes"] = json::array({ {{"mesh", 0}} });

    // Materials: one per bucket
    json materials = json::array();
    for (size_t i = 0; i < usedBuckets.size(); i++) {
        int bucket = usedBuckets[i];
        glm::vec3 color;
        std::string name;
        if (bucket == UNPAINTED) {
            color = glm::vec3(0.8f);
            name = "unpainted";
        } else {
            color = palette[bucket];
            name = "color_" + std::to_string(bucket);
        }
        json mat;
        mat["name"] = name;
        mat["pbrMetallicRoughness"] = {
            {"baseColorFactor", json::array({color.r, color.g, color.b, 1.0f})},
            {"metallicFactor", 0.0f},
            {"roughnessFactor", 1.0f}
        };
        mat["doubleSided"] = true;
        mat["extensions"] = { {"KHR_materials_unlit", json::object()} };
        materials.push_back(mat);
    }
    gltf["materials"] = materials;
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

    // Accessors: 3 per primitive (position, normal, indices)
    json accessors = json::array();
    json primitives = json::array();
    for (size_t i = 0; i < primMetas.size(); i++) {
        const auto& meta = primMetas[i];

        int posAcc = (int)accessors.size();
        accessors.push_back({
            {"bufferView", meta.posView},
            {"componentType", 5126}, // FLOAT
            {"count", meta.vertCount},
            {"type", "VEC3"},
            {"min", json::array({meta.minPos.x, meta.minPos.y, meta.minPos.z})},
            {"max", json::array({meta.maxPos.x, meta.maxPos.y, meta.maxPos.z})}
        });

        int normAcc = (int)accessors.size();
        accessors.push_back({
            {"bufferView", meta.normView},
            {"componentType", 5126},
            {"count", meta.vertCount},
            {"type", "VEC3"}
        });

        int idxAcc = (int)accessors.size();
        accessors.push_back({
            {"bufferView", meta.idxView},
            {"componentType", 5125}, // UNSIGNED_INT
            {"count", meta.idxCount},
            {"type", "SCALAR"}
        });

        primitives.push_back({
            {"attributes", {
                {"POSITION", posAcc},
                {"NORMAL", normAcc}
            }},
            {"indices", idxAcc},
            {"material", (int)i}
        });
    }
    gltf["accessors"] = accessors;
    gltf["meshes"] = json::array({ {{"primitives", primitives}} });

    gltf["buffers"] = json::array({ {{"byteLength", binBuffer.size()}} });

    // Serialize JSON
    std::string jsonStr = gltf.dump();
    // Pad JSON to 4 bytes with spaces
    while (jsonStr.size() % 4 != 0) jsonStr.push_back(' ');

    // Write .glb: 12-byte header + JSON chunk + BIN chunk
    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        std::cerr << "[gltf] Failed to open: " << outPath << std::endl;
        return false;
    }

    uint32_t jsonChunkLen = (uint32_t)jsonStr.size();
    uint32_t binChunkLen = (uint32_t)binBuffer.size();
    uint32_t totalLen = 12 + 8 + jsonChunkLen + 8 + binChunkLen;

    // Header: magic "glTF", version 2, total length
    uint32_t magic = 0x46546C67; // "glTF"
    uint32_t version = 2;
    out.write(reinterpret_cast<const char*>(&magic), 4);
    out.write(reinterpret_cast<const char*>(&version), 4);
    out.write(reinterpret_cast<const char*>(&totalLen), 4);

    // JSON chunk
    uint32_t jsonType = 0x4E4F534A; // "JSON"
    out.write(reinterpret_cast<const char*>(&jsonChunkLen), 4);
    out.write(reinterpret_cast<const char*>(&jsonType), 4);
    out.write(jsonStr.data(), jsonChunkLen);

    // BIN chunk
    uint32_t binType = 0x004E4942; // "BIN\0"
    out.write(reinterpret_cast<const char*>(&binChunkLen), 4);
    out.write(reinterpret_cast<const char*>(&binType), 4);
    out.write(reinterpret_cast<const char*>(binBuffer.data()), binChunkLen);

    std::cout << "[gltf] Exported " << primMetas.size() << " primitives, "
              << binBuffer.size() << " bytes to " << outPath << std::endl;
    return true;
}
