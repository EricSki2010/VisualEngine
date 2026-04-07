#pragma once

// Unit cube: 24 vertices (4 per face), 36 indices
// Each vertex: position (3), uv (2)
// Centered at origin, size 1x1x1

static float cubeVertices[] = {
    // +Z face
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
    // -Z face
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    // -X face
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
    // +X face
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
    // +Y face
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
    // -Y face
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,
};

// Indices with cull state per triangle
// Format: v0, v1, v2, state (0=never cull, 1=partial wall, 2=solid wall)
static unsigned int cubeIndices[] = {
     0,  1,  2, 2,    2,  3,  0, 2,  // +Z (solid)
     4,  6,  5, 2,    6,  4,  7, 2,  // -Z (solid)
     8,  9, 10, 2,   10, 11,  8, 2,  // -X (solid)
    12, 13, 14, 2,   14, 15, 12, 2,  // +X (solid)
    16, 17, 18, 2,   18, 19, 16, 2,  // +Y (solid)
    20, 21, 22, 2,   22, 23, 20, 2,  // -Y (solid)
};

static const int cubeIndexCount = 36;

// Plain indices for non-culled uses (ghost block, etc.)
static unsigned int cubePlainIndices[] = {
     0,  1,  2,   2,  3,  0,
     4,  6,  5,   6,  4,  7,
     8,  9, 10,  10, 11,  8,
    12, 13, 14,  14, 15, 12,
    16, 17, 18,  18, 19, 16,
    20, 21, 22,  22, 23, 20,
};
