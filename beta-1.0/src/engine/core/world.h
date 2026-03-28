#pragma once
#include <unordered_map>
#include <cstdint>
#include <string>
#include <functional>
#include <shared_mutex>
#include <optional>

// ── Tile storage ────────────────────────────────────────────────────
// Stores manually placed blocks. Game layer can combine this with
// procedural generation (noise, rivers, etc.) for the final world.

struct WorldTile {
    int type = 0;       // game defines what types mean
    float height = 0.0f; // Y position of the block center
    unsigned char r = 128, g = 128, b = 128, a = 255;
    std::string blockName;  // registry name (empty = raw RGB block)
    int flowDir = -1;       // flow direction (-1 = none, 0=N, 1=E, 2=S, 3=W)
};

// ── Chunk system ────────────────────────────────────────────────────
// Tiles are grouped into 16x16 chunks for efficient spatial iteration.
// The API (placeTile, getTile, etc.) is unchanged from the outside.

constexpr int CHUNK_SIZE = 16;

// Pack (x, z) into a single 64-bit key
inline int64_t tileKey(int x, int z) {
    return (int64_t)(((uint64_t)(uint32_t)x << 32) | (uint32_t)z);
}

// Unpack a 64-bit key back into (x, z)
inline void unpackTileKey(int64_t key, int& x, int& z) {
    x = (int)(key >> 32);
    z = (int)(key & 0xFFFFFFFF);
}

// Convert world coords to chunk coords
inline int toChunkCoord(int v) {
    return (v >= 0) ? (v / CHUNK_SIZE) : ((v - CHUNK_SIZE + 1) / CHUNK_SIZE);
}

// Convert world coords to local coords within a chunk (always 0..15)
inline int toLocalCoord(int v) {
    int m = v % CHUNK_SIZE;
    return (m >= 0) ? m : m + CHUNK_SIZE;
}

struct Chunk {
    std::unordered_map<int64_t, WorldTile> tiles;
};

struct World {
    std::unordered_map<int64_t, Chunk> chunks;
    mutable std::shared_mutex mtx;  // thread-safe: shared for reads, unique for writes

    void placeTile(int x, int z, int type, float height,
                   unsigned char r = 128, unsigned char g = 128,
                   unsigned char b = 128, unsigned char a = 255,
                   const std::string& blockName = "", int flowDir = -1) {
        std::unique_lock lock(mtx);
        int cx = toChunkCoord(x);
        int cz = toChunkCoord(z);
        int lx = toLocalCoord(x);
        int lz = toLocalCoord(z);
        chunks[tileKey(cx, cz)].tiles[tileKey(lx, lz)] = { type, height, r, g, b, a, blockName, flowDir };
    }

    void removeTile(int x, int z) {
        std::unique_lock lock(mtx);
        int cx = toChunkCoord(x);
        int cz = toChunkCoord(z);
        auto chunkIt = chunks.find(tileKey(cx, cz));
        if (chunkIt == chunks.end()) return;

        int lx = toLocalCoord(x);
        int lz = toLocalCoord(z);
        chunkIt->second.tiles.erase(tileKey(lx, lz));

        // Remove empty chunks to avoid dead entries
        if (chunkIt->second.tiles.empty()) {
            chunks.erase(chunkIt);
        }
    }

    bool hasTile(int x, int z) const {
        std::shared_lock lock(mtx);
        int cx = toChunkCoord(x);
        int cz = toChunkCoord(z);
        auto chunkIt = chunks.find(tileKey(cx, cz));
        if (chunkIt == chunks.end()) return false;

        int lx = toLocalCoord(x);
        int lz = toLocalCoord(z);
        return chunkIt->second.tiles.count(tileKey(lx, lz)) > 0;
    }

    // Returns a copy of the tile (thread-safe — no dangling pointer after lock release).
    // Check .has_value() before using. Old pointer-returning overload is below.
    std::optional<WorldTile> getTile(int x, int z) const {
        std::shared_lock lock(mtx);
        int cx = toChunkCoord(x);
        int cz = toChunkCoord(z);
        auto chunkIt = chunks.find(tileKey(cx, cz));
        if (chunkIt == chunks.end()) return std::nullopt;

        int lx = toLocalCoord(x);
        int lz = toLocalCoord(z);
        auto tileIt = chunkIt->second.tiles.find(tileKey(lx, lz));
        if (tileIt == chunkIt->second.tiles.end()) return std::nullopt;
        return tileIt->second;
    }

    // Iterate all tiles within a radius of (centerX, centerZ).
    // Callback receives world-space (x, z) and the tile reference.
    // Only visits chunks that overlap the view area.
    using TileVisitor = std::function<void(int x, int z, const WorldTile& tile)>;

    void forEachTileInRadius(int centerX, int centerZ, int radius, TileVisitor visitor) const {
        std::shared_lock lock(mtx);
        int minCX = toChunkCoord(centerX - radius);
        int maxCX = toChunkCoord(centerX + radius);
        int minCZ = toChunkCoord(centerZ - radius);
        int maxCZ = toChunkCoord(centerZ + radius);

        for (int chunkZ = minCZ; chunkZ <= maxCZ; chunkZ++) {
            for (int chunkX = minCX; chunkX <= maxCX; chunkX++) {
                auto chunkIt = chunks.find(tileKey(chunkX, chunkZ));
                if (chunkIt == chunks.end()) continue;

                int baseX = chunkX * CHUNK_SIZE;
                int baseZ = chunkZ * CHUNK_SIZE;

                for (auto& [key, tile] : chunkIt->second.tiles) {
                    int lx, lz;
                    unpackTileKey(key, lx, lz);
                    int wx = baseX + lx;
                    int wz = baseZ + lz;

                    if (abs(wx - centerX) <= radius && abs(wz - centerZ) <= radius) {
                        visitor(wx, wz, tile);
                    }
                }
            }
        }
    }

    void clear() {
        std::unique_lock lock(mtx);
        chunks.clear();
    }
};
