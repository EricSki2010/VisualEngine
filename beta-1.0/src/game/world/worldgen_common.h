#pragma once
#include "engine/core/FastNoiseLite.h"
#include "engine/core/world.h"
#include <cstdint>

// ---- World seed ----
inline int WorldSeed = 58;

// ---- Noise helpers ----
// Offset to avoid Perlin symmetry artifacts near (0,0)
const float NOISE_OFFSET = 10000.0f;

// Sample noise and normalize to [0, 1]
inline float sampleNoise(FastNoiseLite& noise, float worldX, float worldY) {
    return (noise.GetNoise(worldX + NOISE_OFFSET, worldY + NOISE_OFFSET) + 1.0f) / 2.0f;
}

// Chunk size and key packing come from engine/core/world.h
// Use CHUNK_SIZE, tileKey(), unpackTileKey() from there.

// ---- Flow directions (cardinal) ----
enum FlowDir { FLOW_NONE = -1, FLOW_N = 0, FLOW_E = 1, FLOW_S = 2, FLOW_W = 3 };

// Cardinal offsets: N, E, S, W
const int FLOW_DX[] = { 0, 1, 0, -1 };
const int FLOW_DY[] = { -1, 0, 1, 0 };

// Opposite direction: N<->S, E<->W
inline FlowDir oppositeDir(FlowDir d) {
    if (d == FLOW_NONE) return FLOW_NONE;
    return (FlowDir)((d + 2) % 4);
}
