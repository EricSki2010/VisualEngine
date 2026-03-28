#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <filesystem>
#include <unordered_map>

#pragma pack(push, 1)

struct ObjectEntry {
    int32_t objectID;       // what object (0 = empty slot)
    float originX, originY, originZ;  // position within the cube
    uint8_t visibleFaces;   // bitmask: 6 bits for top, bottom, left, right, front, back
};

inline constexpr int MAX_OBJECTS = 5;
inline constexpr int MAX_OVERFLOW = 15;

struct Voxel {
    uint8_t objectCount;              // how many objects in this unit
    uint8_t hasOverflow;              // 1 if extra objects are in overflow file
    ObjectEntry objects[MAX_OBJECTS]; // main objects (fseekable)
};

#pragma pack(pop)

struct WorldHeader {
    int32_t width;
    int32_t height;
    int32_t depth;
    int32_t chunkSize;  // 32x32x32
};

inline constexpr int CHUNK_SIZE = 32;
inline const int VOXEL_BYTES = sizeof(Voxel);
inline const int CHUNK_BYTES = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * VOXEL_BYTES;
inline const int HEADER_BYTES = sizeof(WorldHeader);
inline constexpr int SLOT_BYTES = 1;

// Sparse chunk: only stores non-empty voxels in RAM
// Key = x + y*32 + z*32*32
struct ActiveChunk {
    int chunkX, chunkY, chunkZ;
    std::unordered_map<uint16_t, Voxel> voxels;
};

inline uint16_t voxelKey(int x, int y, int z) {
    return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
}

// Call once at startup to set the base path (pass the exe's directory)
void initBasePath(const std::filesystem::path& exePath);
std::filesystem::path getBasePath();

void setup();
void NewScene3D(const char* name, int width, int height, int depth);

// Chunk operations (sparse file format)
ActiveChunk loadChunkSparse(const char* sceneName, int cx, int cy, int cz);
bool saveChunkSparse(const char* sceneName, const ActiveChunk& chunk);
bool addObject(ActiveChunk& chunk, int x, int y, int z, const ObjectEntry& obj);
bool removeObject(ActiveChunk& chunk, int x, int y, int z, int32_t objectID);

// High-level block placement (world coordinates)
bool placeBlock(ActiveChunk& chunk, int x, int y, int z, int32_t blockID, float originX = 0, float originY = 0, float originZ = 0);
bool removeBlock(ActiveChunk& chunk, int x, int y, int z, int32_t blockID);

// Face visibility recalculation — declared here, defined in worldData.cpp
// Include registry.h before calling this
struct BlockDef;
template <typename T> struct Registry;
void recalcFaces(ActiveChunk& chunk, int x, int y, int z, const Registry<BlockDef>& blocks);
void recalcAllFaces(ActiveChunk& chunk, const Registry<BlockDef>& blocks);

#endif // MEMORY_H
