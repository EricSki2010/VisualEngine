#pragma once

// Wedge/ramp with separate vertices per face for correct normals
// 5 faces: bottom, back, slope, left triangle, right triangle
// Centered at origin, fits in 1x1x1

static float wedgeVertices[] = {
    // Bottom face (4 verts)
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  // 0
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  // 1
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  // 2
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  // 3

    // Back face (4 verts)
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  // 4
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  // 5
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  // 6
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  // 7

    // Slope face (4 verts)
    -0.5f,  0.5f, -0.5f,  0.0f, 0.0f,  // 8
     0.5f,  0.5f, -0.5f,  1.0f, 0.0f,  // 9
     0.5f, -0.5f,  0.5f,  1.0f, 1.0f,  // 10
    -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  // 11

    // Left triangle (3 verts)
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  // 12
    -0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  // 13
    -0.5f,  0.5f, -0.5f,  0.5f, 1.0f,  // 14

    // Right triangle (3 verts)
     0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  // 15
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  // 16
     0.5f,  0.5f, -0.5f,  0.5f, 1.0f,  // 17
};

// Indices with cull state per triangle
// Format: v0, v1, v2, state (0=never cull, 1=partial wall, 2=solid wall)
static unsigned int wedgeIndices[] = {
    // Bottom (solid wall)
    0, 2, 1, 2,    2, 0, 3, 2,
    // Back (solid wall)
    4, 6, 5, 2,    6, 4, 7, 2,
    // Slope (never cull - angled)
    8, 10, 9, 0,   10, 8, 11, 0,
    // Left (partial - on wall but not full)
    12, 14, 13, 1,
    // Right (partial - on wall but not full)
    15, 17, 16, 1,
};

static const int wedgeIndexCount = 24; // actual triangle indices (excluding states)
