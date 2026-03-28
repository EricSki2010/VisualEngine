#pragma once
#include "placed_blocks.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <filesystem>

// Returns the AbstractModels/ folder path
static std::string getAbstractModelsDir() {
    namespace fs = std::filesystem;
    fs::path dir = fs::path("AbstractModels");
    fs::create_directories(dir);
    return dir.string();
}

// Returns the assets/entities/blockModels/ folder path
static std::string getBlockModelsDir() {
    namespace fs = std::filesystem;
    fs::path assetsDir = fs::path("assets") / "entities" / "blockModels";
    fs::create_directories(assetsDir);
    return assetsDir.string();
}

// Returns the assets/entities/blocks/ folder path
static std::string getBlocksJsonDir() {
    namespace fs = std::filesystem;
    fs::path blocksDir = fs::path("assets") / "entities" / "blocks";
    fs::create_directories(blocksDir);
    return blocksDir.string();
}

// Creates a block JSON file for the exported model
static bool createBlockJson(const std::string& name, const std::string& glbRelPath) {
    std::string jsonPath = getBlocksJsonDir() + "/" + name + ".json";
    FILE* f = fopen(jsonPath.c_str(), "w");
    if (!f) return false;
    fprintf(f, "{\n");
    fprintf(f, "    \"name\": \"%s\",\n", name.c_str());
    fprintf(f, "    \"formatType\": \"glb\",\n");
    fprintf(f, "    \"modelFile\": \"%s\",\n", glbRelPath.c_str());
    fprintf(f, "    \"collisionType\": \"solid\",\n");
    fprintf(f, "    \"color\": [130, 130, 130, 255]\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

// ── GLB Mesh Data ───────────────────────────────────────────────────
// Intermediate representation built from PlacedBlocks, then written as GLB.

struct GLBMeshData {
    struct Vert { float x, y, z, r, g, b, a; };
    std::vector<Vert> verts;
    std::vector<uint32_t> idxs;
    float minP[3], maxP[3]; // bounding box (post-normalization)

    bool empty() const { return verts.empty(); }

    void addQuad(float x, float y, float z, float s, int face,
                 float cr, float cg, float cb) {
        uint32_t base = (uint32_t)verts.size();

        switch (face) {
            case 0: // +X
                verts.push_back({x+s,y-s,z-s, cr,cg,cb,1});
                verts.push_back({x+s,y+s,z-s, cr,cg,cb,1});
                verts.push_back({x+s,y+s,z+s, cr,cg,cb,1});
                verts.push_back({x+s,y-s,z+s, cr,cg,cb,1});
                break;
            case 1: // -X
                verts.push_back({x-s,y-s,z+s, cr,cg,cb,1});
                verts.push_back({x-s,y+s,z+s, cr,cg,cb,1});
                verts.push_back({x-s,y+s,z-s, cr,cg,cb,1});
                verts.push_back({x-s,y-s,z-s, cr,cg,cb,1});
                break;
            case 2: // +Y
                verts.push_back({x-s,y+s,z-s, cr,cg,cb,1});
                verts.push_back({x-s,y+s,z+s, cr,cg,cb,1});
                verts.push_back({x+s,y+s,z+s, cr,cg,cb,1});
                verts.push_back({x+s,y+s,z-s, cr,cg,cb,1});
                break;
            case 3: // -Y
                verts.push_back({x-s,y-s,z+s, cr,cg,cb,1});
                verts.push_back({x-s,y-s,z-s, cr,cg,cb,1});
                verts.push_back({x+s,y-s,z-s, cr,cg,cb,1});
                verts.push_back({x+s,y-s,z+s, cr,cg,cb,1});
                break;
            case 4: // +Z
                verts.push_back({x-s,y-s,z+s, cr,cg,cb,1});
                verts.push_back({x+s,y-s,z+s, cr,cg,cb,1});
                verts.push_back({x+s,y+s,z+s, cr,cg,cb,1});
                verts.push_back({x-s,y+s,z+s, cr,cg,cb,1});
                break;
            case 5: // -Z
                verts.push_back({x+s,y-s,z-s, cr,cg,cb,1});
                verts.push_back({x-s,y-s,z-s, cr,cg,cb,1});
                verts.push_back({x-s,y+s,z-s, cr,cg,cb,1});
                verts.push_back({x+s,y+s,z-s, cr,cg,cb,1});
                break;
        }

        idxs.push_back(base+0); idxs.push_back(base+1); idxs.push_back(base+2);
        idxs.push_back(base+0); idxs.push_back(base+2); idxs.push_back(base+3);
    }

    // modelBoundsExtent: if > 0, use this as the max extent for normalization
    // instead of computing from vertices. This preserves proportions relative
    // to the original model dimensions even when blocks are removed.
    float modelBoundsExtent = 0.0f;

    void normalize(float originX = 0.0f, float originY = 0.0f, float originZ = 0.0f,
                    bool hasOrigin = false) {
        if (verts.empty()) return;
        // Compute raw bounding box
        float rawMin[3] = {1e9f,1e9f,1e9f}, rawMax[3] = {-1e9f,-1e9f,-1e9f};
        for (auto& v : verts) {
            if (v.x < rawMin[0]) rawMin[0] = v.x; if (v.x > rawMax[0]) rawMax[0] = v.x;
            if (v.y < rawMin[1]) rawMin[1] = v.y; if (v.y > rawMax[1]) rawMax[1] = v.y;
            if (v.z < rawMin[2]) rawMin[2] = v.z; if (v.z > rawMax[2]) rawMax[2] = v.z;
        }

        // Fit in a 1x1x1 cube, using original model bounds if available
        float maxExtent;
        if (modelBoundsExtent > 0.0f) {
            maxExtent = modelBoundsExtent;
        } else {
            float extX = rawMax[0] - rawMin[0];
            float extY = rawMax[1] - rawMin[1];
            float extZ = rawMax[2] - rawMin[2];
            maxExtent = extX;
            if (extY > maxExtent) maxExtent = extY;
            if (extZ > maxExtent) maxExtent = extZ;
        }
        if (maxExtent < 0.001f) maxExtent = 1.0f;

        float scale = 1.0f / maxExtent;

        // If an origin was set, use it as the anchor point.
        // Otherwise fall back to bounding box center X/Z, base Y=0.
        float anchorX, anchorY, anchorZ;
        if (hasOrigin) {
            anchorX = originX;
            anchorY = originY;
            anchorZ = originZ;
        } else {
            anchorX = (rawMin[0] + rawMax[0]) * 0.5f;
            anchorY = rawMin[1];
            anchorZ = (rawMin[2] + rawMax[2]) * 0.5f;
        }

        for (auto& v : verts) {
            v.x = (v.x - anchorX) * scale;
            v.y = (v.y - anchorY) * scale;
            v.z = (v.z - anchorZ) * scale;
        }

        // Recompute bounds
        for (int i = 0; i < 3; i++) { minP[i] = 1e9f; maxP[i] = -1e9f; }
        for (auto& v : verts) {
            if (v.x < minP[0]) minP[0] = v.x; if (v.x > maxP[0]) maxP[0] = v.x;
            if (v.y < minP[1]) minP[1] = v.y; if (v.y > maxP[1]) maxP[1] = v.y;
            if (v.z < minP[2]) minP[2] = v.z; if (v.z > maxP[2]) maxP[2] = v.z;
        }
    }
};

// ── Stage 1: Build mesh from PlacedBlocks ───────────────────────────

static GLBMeshData buildExportMesh(PlacedBlocks& blocks) {
    GLBMeshData mesh;

    const auto& allPositions = blocks.allWorldPositions();
    for (auto& w : allPositions) {
        Vec3i ph = blocks.toOffset(w);
        Color fc[6];
        const std::string& blockName = blocks.getBlockName(ph);
        if (!blockName.empty()) {
            PlacedBlocks::decodeFaceColors(blockName, fc);
        } else {
            for (int i = 0; i < 6; i++) fc[i] = GRAY;
        }

        float x = (float)ph.x, y = (float)ph.y, z = (float)ph.z;

        for (int face = 0; face < 6; face++) {
            Vec3i nb = {ph.x+PlacedBlocks::DIRS6[face].x, ph.y+PlacedBlocks::DIRS6[face].y, ph.z+PlacedBlocks::DIRS6[face].z};
            if (blocks.hasBlockAt(nb)) continue;

            float cr = fc[face].r/255.0f, cg = fc[face].g/255.0f, cb = fc[face].b/255.0f;
            mesh.addQuad(x, y, z, 0.5f, face, cr, cg, cb);
        }
    }

    if (!mesh.empty()) {
        // Use original model dimensions for normalization so removing blocks
        // doesn't stretch the remaining geometry to fill a 1x1x1 cube
        if (blocks.isModelMode()) {
            int minX, maxX, minY, maxY, minZ, maxZ;
            blocks.getModelBoundsMinMax(minX, maxX, minY, maxY, minZ, maxZ);
            float extX = (float)(maxX - minX + 1);
            float extY = (float)(maxY - minY + 1);
            float extZ = (float)(maxZ - minZ + 1);
            float maxExt = extX;
            if (extY > maxExt) maxExt = extY;
            if (extZ > maxExt) maxExt = extZ;
            mesh.modelBoundsExtent = maxExt;
        }

        bool hasOrigin = (blocks.modelOriginPos.x != 0.0f ||
                          blocks.modelOriginPos.y != 0.0f ||
                          blocks.modelOriginPos.z != 0.0f);
        mesh.normalize(blocks.modelOriginPos.x, blocks.modelOriginPos.y,
                       blocks.modelOriginPos.z, hasOrigin);
    }
    return mesh;
}

// ── Stage 2: Write GLB binary file ──────────────────────────────────

static bool writeGLBFile(const std::string& filepath, const GLBMeshData& mesh) {
    if (mesh.empty()) return false;

    uint32_t vc = (uint32_t)mesh.verts.size();
    uint32_t ic = (uint32_t)mesh.idxs.size();
    uint32_t posBytes = vc * 12;
    uint32_t colBytes = vc * 16;
    uint32_t idxBytes = ic * 4;
    uint32_t binSize = posBytes + colBytes + idxBytes;
    uint32_t binPad = (4 - (binSize % 4)) % 4;

    // Build JSON
    std::string j = "{";
    j += "\"asset\":{\"version\":\"2.0\",\"generator\":\"MapCreator\"},";
    j += "\"scene\":0,";
    j += "\"scenes\":[{\"nodes\":[0]}],";
    j += "\"nodes\":[{\"mesh\":0}],";
    j += "\"materials\":[{\"doubleSided\":true,\"pbrMetallicRoughness\":{\"metallicFactor\":0.0,\"roughnessFactor\":1.0}}],";
    j += "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"COLOR_0\":1},\"indices\":2,\"material\":0}]}],";
    j += "\"accessors\":[";

    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"bufferView\":0,\"componentType\":5126,\"count\":%u,\"type\":\"VEC3\","
        "\"min\":[%.6f,%.6f,%.6f],\"max\":[%.6f,%.6f,%.6f]},",
        vc, mesh.minP[0], mesh.minP[1], mesh.minP[2], mesh.maxP[0], mesh.maxP[1], mesh.maxP[2]);
    j += buf;
    snprintf(buf, sizeof(buf),
        "{\"bufferView\":1,\"componentType\":5126,\"count\":%u,\"type\":\"VEC4\"},", vc);
    j += buf;
    snprintf(buf, sizeof(buf),
        "{\"bufferView\":2,\"componentType\":5125,\"count\":%u,\"type\":\"SCALAR\"}", ic);
    j += buf;

    j += "],\"bufferViews\":[";
    snprintf(buf, sizeof(buf),
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":%u,\"target\":34962},", posBytes);
    j += buf;
    snprintf(buf, sizeof(buf),
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34962},", posBytes, colBytes);
    j += buf;
    snprintf(buf, sizeof(buf),
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34963}", posBytes+colBytes, idxBytes);
    j += buf;

    snprintf(buf, sizeof(buf), "],\"buffers\":[{\"byteLength\":%u}]}", binSize);
    j += buf;

    // Pad JSON to 4 bytes
    while (j.size() % 4 != 0) j += ' ';

    uint32_t jsonLen = (uint32_t)j.size();
    uint32_t totalSize = 12 + 8 + jsonLen + 8 + binSize + binPad;

    FILE* f = fopen(filepath.c_str(), "wb");
    if (!f) return false;

    // GLB header
    uint32_t magic = 0x46546C67;
    uint32_t version = 2;
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&totalSize, 4, 1, f);

    // JSON chunk
    uint32_t jsonType = 0x4E4F534A;
    fwrite(&jsonLen, 4, 1, f);
    fwrite(&jsonType, 4, 1, f);
    fwrite(j.c_str(), 1, jsonLen, f);

    // Binary chunk
    uint32_t binChunkSize = binSize + binPad;
    uint32_t binType = 0x004E4942;
    fwrite(&binChunkSize, 4, 1, f);
    fwrite(&binType, 4, 1, f);

    // Positions
    for (auto& v : mesh.verts) { float p[3] = {v.x, v.y, v.z}; fwrite(p, 12, 1, f); }
    // Colors
    for (auto& v : mesh.verts) { float c[4] = {v.r, v.g, v.b, v.a}; fwrite(c, 16, 1, f); }
    // Indices
    fwrite(mesh.idxs.data(), 4, ic, f);
    // Padding
    for (uint32_t i = 0; i < binPad; i++) { uint8_t z = 0; fwrite(&z, 1, 1, f); }

    fclose(f);
    return true;
}

// ── Public API (unchanged signature) ────────────────────────────────

static bool exportModelAsGLB(const std::string& filepath, PlacedBlocks& blocks) {
    GLBMeshData mesh = buildExportMesh(blocks);
    return writeGLBFile(filepath, mesh);
}
