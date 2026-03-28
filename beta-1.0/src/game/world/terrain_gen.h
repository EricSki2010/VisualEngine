#pragma once

// ---- Tile / biome types ----
enum TileType {
    TILE_DEEP_WATER = 0,
    TILE_SHALLOW_WATER,
    TILE_SAND,
    TILE_DESERT,
    TILE_DRY_GRASS,
    TILE_GRASS,
    TILE_SWAMP,
    TILE_DRY_HILLS,
    TILE_FOREST,
    TILE_DENSE_FOREST,
    TILE_BARE_STONE,
    TILE_MOSSY_STONE,
    TILE_MOUNTAIN_PEAK,
    TILE_SNOWY_PEAK,
    TILE_COUNT
};

inline const char* const TILE_NAMES[TILE_COUNT] = {
    "Deep Water",
    "Shallow Water",
    "Sand",
    "Desert",
    "Dry Grass",
    "Grass",
    "Swamp",
    "Dry Hills",
    "Forest",
    "Dense Forest",
    "Bare Stone",
    "Mossy Stone",
    "Mountain Peak",
    "Snowy Peak",
};

// ---- Elevation thresholds ----
const float DEEP_WATER_THRESHOLD = 0.25f;
const float WATER_THRESHOLD = 0.35f;
const float SAND_THRESHOLD = 0.37f;

// ---- Biome classification ----
// Given noise values in [0,1], returns which biome/tile type this location is.
inline TileType getBiome(float elevation, float moisture, float vegetation) {
    if (elevation < 0.25f) return TILE_DEEP_WATER;
    if (elevation < 0.35f) return TILE_SHALLOW_WATER;
    if (elevation < 0.37f) return TILE_SAND;

    if (elevation < 0.65f) {
        if (moisture < 0.30f) return TILE_DESERT;
        if (vegetation > 0.60f && vegetation < 0.80f) return TILE_FOREST;
        if (vegetation > 0.80f) return TILE_DENSE_FOREST;
        if (moisture < 0.50f) return TILE_DRY_GRASS;
        if (moisture < 0.70f) return TILE_GRASS;
        return TILE_SWAMP;
    }

    if (elevation < 0.75f) {
        if ((moisture < 0.50f && vegetation < 0.40f) || moisture < 0.30f) return TILE_DRY_HILLS;
        if (vegetation < 0.70f) return TILE_FOREST;
        return TILE_DENSE_FOREST;
    }

    if (elevation < 0.85f) {
        if (moisture < 0.40f) return TILE_BARE_STONE;
        return TILE_MOSSY_STONE;
    }

    if (moisture > 0.50f) return TILE_SNOWY_PEAK;
    return TILE_MOUNTAIN_PEAK;
}
