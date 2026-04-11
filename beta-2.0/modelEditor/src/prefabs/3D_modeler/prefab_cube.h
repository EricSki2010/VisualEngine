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

// Indices with face direction per triangle
// Format: v0, v1, v2, faceDir
// faceDir: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
static unsigned int cubeIndices[] = {
     0,  1,  2, 4,    2,  3,  0, 4,  // +Z face
     4,  6,  5, 5,    6,  4,  7, 5,  // -Z face
     8,  9, 10, 1,   10, 11,  8, 1,  // -X face
    12, 13, 14, 0,   14, 15, 12, 0,  // +X face
    16, 17, 18, 2,   18, 19, 16, 2,  // +Y face
    20, 21, 22, 3,   22, 23, 20, 3,  // -Y face
};

// Face states: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z (all solid)
static const int cubeFaceStates[6] = {2, 2, 2, 2, 2, 2};

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
