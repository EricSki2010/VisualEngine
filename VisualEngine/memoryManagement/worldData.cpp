#include "memory.h"
#include "registry.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <string>

static std::filesystem::path getSavesPath() {
    return getBasePath() / "saves" / "worldData";
}

static long long getSlotOffset(int x, int y, int z, const WorldHeader& header) {
    return HEADER_BYTES + (x + y * header.width + z * header.width * header.height);
}

static std::filesystem::path getChunkPath(const char* sceneName, int x, int y, int z) {
    std::string fileName = "chunk_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z) + ".bin";
    return getSavesPath() / (std::string(sceneName) + "_chunks") / fileName;
}

// Mark a chunk slot in the index as occupied or empty
static bool markIndex(const char* sceneName, int x, int y, int z, char flag) {
    std::filesystem::path indexPath = getSavesPath() / (std::string(sceneName) + ".bin");

    std::fstream index(indexPath, std::ios::binary | std::ios::in | std::ios::out);
    if (!index) {
        std::cerr << "Failed to open scene index: " << indexPath << std::endl;
        return false;
    }

    WorldHeader header;
    index.read(reinterpret_cast<char*>(&header), HEADER_BYTES);

    if (x < 0 || x >= header.width || y < 0 || y >= header.height || z < 0 || z >= header.depth) {
        std::cerr << "Chunk coordinates out of bounds" << std::endl;
        return false;
    }

    long long offset = getSlotOffset(x, y, z, header);
    index.seekp(offset);
    index.write(&flag, 1);
    return true;
}

// Check if a chunk exists in the index
static bool chunkExists(const char* sceneName, int x, int y, int z) {
    std::filesystem::path indexPath = getSavesPath() / (std::string(sceneName) + ".bin");

    std::ifstream index(indexPath, std::ios::binary);
    if (!index) return false;

    WorldHeader header;
    index.read(reinterpret_cast<char*>(&header), HEADER_BYTES);

    if (x < 0 || x >= header.width || y < 0 || y >= header.height || z < 0 || z >= header.depth)
        return false;

    long long offset = getSlotOffset(x, y, z, header);
    index.seekg(offset);
    char flag = 0;
    index.read(&flag, 1);
    return flag != 0;
}

// Load chunk from disk (sparse format: count + key/voxel pairs)
ActiveChunk loadChunkSparse(const char* sceneName, int cx, int cy, int cz) {
    ActiveChunk active;
    active.chunkX = cx;
    active.chunkY = cy;
    active.chunkZ = cz;

    if (!chunkExists(sceneName, cx, cy, cz)) return active;

    std::filesystem::path chunkPath = getChunkPath(sceneName, cx, cy, cz);
    std::ifstream file(chunkPath, std::ios::binary);
    if (!file) return active;

    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    const uint32_t maxVoxels = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    if (!file || count > maxVoxels) return active;

    for (uint32_t i = 0; i < count; i++) {
        uint16_t key = 0;
        Voxel voxel;
        file.read(reinterpret_cast<char*>(&key), sizeof(key));
        file.read(reinterpret_cast<char*>(&voxel), sizeof(Voxel));
        active.voxels[key] = voxel;
    }

    return active;
}

// Save chunk to disk (sparse format: only non-empty voxels)
bool saveChunkSparse(const char* sceneName, const ActiveChunk& chunk) {
    std::filesystem::path chunkPath = getChunkPath(sceneName, chunk.chunkX, chunk.chunkY, chunk.chunkZ);

    // If chunk is empty, remove the file and clear the index
    if (chunk.voxels.empty()) {
        std::filesystem::remove(chunkPath);
        markIndex(sceneName, chunk.chunkX, chunk.chunkY, chunk.chunkZ, 0);
        return true;
    }

    std::ofstream file(chunkPath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to create chunk file: " << chunkPath << std::endl;
        return false;
    }

    uint32_t count = chunk.voxels.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [key, voxel] : chunk.voxels) {
        file.write(reinterpret_cast<const char*>(&key), sizeof(key));
        file.write(reinterpret_cast<const char*>(&voxel), sizeof(Voxel));
    }

    file.close();
    if (!file) {
        std::cerr << "Error writing chunk file" << std::endl;
        return false;
    }

    markIndex(sceneName, chunk.chunkX, chunk.chunkY, chunk.chunkZ, 1);
    return true;
}

// Add an object to a voxel in an active chunk
bool addObject(ActiveChunk& chunk, int x, int y, int z, const ObjectEntry& obj) {
    uint16_t key = voxelKey(x, y, z);
    auto [it, inserted] = chunk.voxels.try_emplace(key, Voxel{});
    Voxel& voxel = it->second;

    if (voxel.objectCount >= MAX_OBJECTS) {
        std::cerr << "Voxel full (max " << MAX_OBJECTS << " objects)" << std::endl;
        return false;
    }

    voxel.objects[voxel.objectCount] = obj;
    voxel.objectCount++;
    return true;
}

// Remove an object by ID from a voxel in an active chunk
bool removeObject(ActiveChunk& chunk, int x, int y, int z, int32_t objectID) {
    uint16_t key = voxelKey(x, y, z);
    auto it = chunk.voxels.find(key);
    if (it == chunk.voxels.end()) return false;

    Voxel& voxel = it->second;
    for (int i = 0; i < voxel.objectCount; i++) {
        if (voxel.objects[i].objectID == objectID) {
            for (int j = i; j < voxel.objectCount - 1; j++) {
                voxel.objects[j] = voxel.objects[j + 1];
            }
            voxel.objectCount--;
            std::memset(&voxel.objects[voxel.objectCount], 0, sizeof(ObjectEntry));

            if (voxel.objectCount == 0) {
                chunk.voxels.erase(it);
            }
            return true;
        }
    }
    return false;
}

bool placeBlock(ActiveChunk& chunk, int x, int y, int z, int32_t blockID, float originX, float originY, float originZ) {
    ObjectEntry obj;
    obj.objectID = blockID;
    obj.originX = originX;
    obj.originY = originY;
    obj.originZ = originZ;
    obj.visibleFaces = FACE_ALL;
    return addObject(chunk, x, y, z, obj);
}

bool removeBlock(ActiveChunk& chunk, int x, int y, int z, int32_t blockID) {
    return removeObject(chunk, x, y, z, blockID);
}

static const int dirs[6][3] = {
    {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
};
static const uint8_t faceBit[6]     = { FACE_POS_X, FACE_NEG_X, FACE_POS_Y, FACE_NEG_Y, FACE_POS_Z, FACE_NEG_Z };
static const uint8_t oppositeBit[6] = { FACE_NEG_X, FACE_POS_X, FACE_NEG_Y, FACE_POS_Y, FACE_NEG_Z, FACE_POS_Z };

void recalcAllFaces(ActiveChunk& chunk, const BlockRegistry& blocks) {
    // Build a flat lookup: blockID -> solidFaces, to avoid repeated hash lookups
    std::unordered_map<int32_t, uint8_t> solidCache;
    for (const auto& [key, voxel] : chunk.voxels) {
        for (int i = 0; i < voxel.objectCount; i++) {
            int32_t id = voxel.objects[i].objectID;
            if (solidCache.find(id) == solidCache.end()) {
                const BlockDef* def = blocks.getByID(id);
                solidCache[id] = def ? def->solidFaces : 0;
            }
        }
    }

    for (auto& [key, voxel] : chunk.voxels) {
        int x = key % CHUNK_SIZE;
        int y = (key / CHUNK_SIZE) % CHUNK_SIZE;
        int z = key / (CHUNK_SIZE * CHUNK_SIZE);

        for (int i = 0; i < voxel.objectCount; i++) {
            uint8_t visible = FACE_ALL;

            for (int d = 0; d < 6; d++) {
                int nx = x + dirs[d][0];
                int ny = y + dirs[d][1];
                int nz = z + dirs[d][2];

                if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_SIZE || nz < 0 || nz >= CHUNK_SIZE)
                    continue;

                auto nit = chunk.voxels.find(voxelKey(nx, ny, nz));
                if (nit == chunk.voxels.end()) continue;

                for (int j = 0; j < nit->second.objectCount; j++) {
                    uint8_t sf = solidCache[nit->second.objects[j].objectID];
                    if (sf & oppositeBit[d]) {
                        visible &= ~faceBit[d];
                        break;
                    }
                }
            }

            voxel.objects[i].visibleFaces = visible;
        }
    }
}

void recalcFaces(ActiveChunk& chunk, int x, int y, int z, const BlockRegistry& blocks) {
    uint16_t key = voxelKey(x, y, z);
    auto it = chunk.voxels.find(key);
    if (it == chunk.voxels.end()) return;

    Voxel& voxel = it->second;

    for (int i = 0; i < voxel.objectCount; i++) {
        uint8_t visible = 0x3F;

        for (int d = 0; d < 6; d++) {
            int nx = x + dirs[d][0];
            int ny = y + dirs[d][1];
            int nz = z + dirs[d][2];

            if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_SIZE || nz < 0 || nz >= CHUNK_SIZE)
                continue;

            auto nit = chunk.voxels.find(voxelKey(nx, ny, nz));
            if (nit == chunk.voxels.end()) continue;

            // Check if any object in the neighbor has the opposite face solid
            for (int j = 0; j < nit->second.objectCount; j++) {
                const BlockDef* ndef = blocks.getByID(nit->second.objects[j].objectID);
                if (ndef && (ndef->solidFaces & oppositeBit[d])) {
                    visible &= ~faceBit[d];
                    break;
                }
            }
        }

        voxel.objects[i].visibleFaces = visible;
    }
}
