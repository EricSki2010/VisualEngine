#pragma once
#include "placed_blocks.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <filesystem>

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

static bool exportModelAsGLB(const std::string& filepath, PlacedBlocks& blocks) {
    struct Vert { float x, y, z, r, g, b, a; };
    std::vector<Vert> verts;
    std::vector<uint32_t> idxs;

    Vec3i faceDirs[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

    // Get all surface positions (works for both model and structure mode)
    auto allPositions = blocks.allWorldPositions();
    for (auto& w : allPositions) {
        Vec3i ph = blocks.toOffset(w);
        Color fc[6];
        auto it = blocks.placed.find(ph);
        if (it != blocks.placed.end()) {
            PlacedBlocks::decodeFaceColors(it->second, fc);
        } else {
            for (int i = 0; i < 6; i++) fc[i] = GRAY;
        }

        float x = (float)ph.x, y = (float)ph.y, z = (float)ph.z;
        float s = 0.5f;

        for (int face = 0; face < 6; face++) {
            Vec3i nb = {ph.x+faceDirs[face].x, ph.y+faceDirs[face].y, ph.z+faceDirs[face].z};
            if (blocks.hasBlockAt(nb)) continue;

            float cr = fc[face].r/255.0f, cg = fc[face].g/255.0f, cb = fc[face].b/255.0f;
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
    }

    if (verts.empty()) return false;

    // Compute raw bounding box
    float rawMin[3] = {1e9f,1e9f,1e9f}, rawMax[3] = {-1e9f,-1e9f,-1e9f};
    for (auto& v : verts) {
        if (v.x < rawMin[0]) rawMin[0] = v.x; if (v.x > rawMax[0]) rawMax[0] = v.x;
        if (v.y < rawMin[1]) rawMin[1] = v.y; if (v.y > rawMax[1]) rawMax[1] = v.y;
        if (v.z < rawMin[2]) rawMin[2] = v.z; if (v.z > rawMax[2]) rawMax[2] = v.z;
    }

    // Normalize to fit in a 1x1x1 cube, centered on X/Z, base at Y=0
    float extX = rawMax[0] - rawMin[0];
    float extY = rawMax[1] - rawMin[1];
    float extZ = rawMax[2] - rawMin[2];
    float maxExtent = extX;
    if (extY > maxExtent) maxExtent = extY;
    if (extZ > maxExtent) maxExtent = extZ;
    if (maxExtent < 0.001f) maxExtent = 1.0f;

    float scale = 1.0f / maxExtent;
    float centerX = (rawMin[0] + rawMax[0]) * 0.5f;
    float centerZ = (rawMin[2] + rawMax[2]) * 0.5f;
    float baseY = rawMin[1];

    for (auto& v : verts) {
        v.x = (v.x - centerX) * scale;
        v.y = (v.y - baseY) * scale;
        v.z = (v.z - centerZ) * scale;
    }

    // Recompute bounds after normalization
    float minP[3] = {1e9f,1e9f,1e9f}, maxP[3] = {-1e9f,-1e9f,-1e9f};
    for (auto& v : verts) {
        if (v.x < minP[0]) minP[0] = v.x; if (v.x > maxP[0]) maxP[0] = v.x;
        if (v.y < minP[1]) minP[1] = v.y; if (v.y > maxP[1]) maxP[1] = v.y;
        if (v.z < minP[2]) minP[2] = v.z; if (v.z > maxP[2]) maxP[2] = v.z;
    }

    uint32_t vc = (uint32_t)verts.size();
    uint32_t ic = (uint32_t)idxs.size();
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
        vc, minP[0], minP[1], minP[2], maxP[0], maxP[1], maxP[2]);
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

    // Pad JSON to 4 bytes with spaces
    while (j.size() % 4 != 0) j += ' ';

    uint32_t jsonLen = (uint32_t)j.size();
    uint32_t totalSize = 12 + 8 + jsonLen + 8 + binSize + binPad;

    // Write file
    FILE* f = fopen(filepath.c_str(), "wb");
    if (!f) return false;

    // GLB header
    uint32_t magic = 0x46546C67; // "glTF"
    uint32_t version = 2;
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&totalSize, 4, 1, f);

    // JSON chunk
    uint32_t jsonType = 0x4E4F534A; // "JSON"
    fwrite(&jsonLen, 4, 1, f);
    fwrite(&jsonType, 4, 1, f);
    fwrite(j.c_str(), 1, jsonLen, f);

    // Binary chunk
    uint32_t binChunkSize = binSize + binPad;
    uint32_t binType = 0x004E4942; // "BIN\0"
    fwrite(&binChunkSize, 4, 1, f);
    fwrite(&binType, 4, 1, f);

    // Positions
    for (auto& v : verts) { float p[3] = {v.x, v.y, v.z}; fwrite(p, 12, 1, f); }
    // Colors
    for (auto& v : verts) { float c[4] = {v.r, v.g, v.b, v.a}; fwrite(c, 16, 1, f); }
    // Indices
    fwrite(idxs.data(), 4, ic, f);
    // Padding
    for (uint32_t i = 0; i < binPad; i++) { uint8_t z = 0; fwrite(&z, 1, 1, f); }

    fclose(f);
    return true;
}
