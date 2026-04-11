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

// Indices with face direction per triangle
// Format: v0, v1, v2, faceDir
// faceDir: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z, 0xFFFFFFFF=none
static unsigned int wedgeIndices[] = {
    // Bottom (-Y face)
    0, 2, 1, 3,    2, 0, 3, 3,
    // Back (-Z face)
    4, 6, 5, 5,    6, 4, 7, 5,
    // Slope (no face)
    8, 10, 9, 0xFFFFFFFF,   10, 8, 11, 0xFFFFFFFF,
    // Left (-X face, partial)
    12, 14, 13, 1,
    // Right (+X face, partial)
    15, 17, 16, 0,
};

// Face states: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
// Bottom(-Y)=solid, Back(-Z)=solid, Left(-X)/Right(+X)=partial
static const int wedgeFaceStates[6] = {1, 1, 0, 2, 0, 2};

static const int wedgeIndexCount = 24; // actual triangle indices (excluding faceDir)
