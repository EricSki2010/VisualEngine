#pragma once
#include <cstdint>

// Deterministic per-tile RNG — same coords + channel always gives the same result
// Uses a hash-based approach, no state to store

inline uint32_t tileHash(int x, int z, int channel = 0) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u + (uint32_t)channel * 1274126177u;
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);
    return h;
}

// Returns a float in [0.0, 1.0)
inline float tileRandFloat(int x, int z, int channel = 0) {
    return (float)(tileHash(x, z, channel) & 0x00FFFFFFu) / (float)0x01000000u;
}

// Returns an int in [0, max)
inline int tileRandInt(int x, int z, int max, int channel = 0) {
    if (max <= 0) return 0;
    return (int)(tileHash(x, z, channel) % (uint32_t)max);
}

// Returns true with the given probability [0.0, 1.0]
inline bool tileRandChance(int x, int z, float probability, int channel = 0) {
    return tileRandFloat(x, z, channel) < probability;
}
