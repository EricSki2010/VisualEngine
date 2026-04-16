#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <string>

// Rasterize a group of 2D triangles into a square PNG buffer.
//
// Inputs:
//   verts:          2D vertices in 0..1 space (x=0 is left, y=0 is top;
//                   caller must pre-normalize so the geometry fills the
//                   unit square, stretching if needed).
//   indices:        triangle index list (triples).
//   triangleColors: one RGB color per triangle (float 0..1). Must have
//                   indices.size() / 3 entries.
//   size: side length in pixels (square output). E.g. 64, 128, 256.
//
// Output: PNG byte buffer ready to write to disk or embed.
// Returns an empty vector on failure.
std::vector<uint8_t> packTrianglesToPNG(
    const std::vector<glm::vec2>& verts,
    const std::vector<uint32_t>& indices,
    const std::vector<glm::vec3>& triangleColors,
    int size = 64);

// Normalize arbitrary 2D verts into the unit square, rasterize them via
// packTrianglesToPNG, and write the result to disk.
//
// Algorithm:
//   1. Find (minX, minY) across all verts — this is the "origin" (top-left
//      in image space).
//   2. Translate every vert by -origin so the bounding box starts at (0,0).
//   3. Scale by 1 / (maxX-minX, maxY-minY) so it fills [0,1] x [0,1].
//   4. Feed the normalized verts + indices + triangleColors into
//      packTrianglesToPNG and write the PNG bytes to `outPath`.
//
// The normalized coordinates are returned as `uvs`, one per input vertex.
// They line up 1:1 with the originals, so you can use them as UVs when
// loading the model back with the saved PNG as its texture.
struct PackAndExportResult {
    std::vector<glm::vec2> uvs; // 0..1, same order as input verts
    size_t pngBytes = 0;        // size of the PNG written to disk
    bool success = false;
};

PackAndExportResult packAndSavePNG(
    const std::vector<glm::vec2>& verts,
    const std::vector<uint32_t>& indices,
    const std::vector<glm::vec3>& triangleColors,
    const std::string& outPath,
    int size = 64);

// Bake a flat voxel face into a tiny grid-aligned pixel buffer.
//
// Assumes input verts are 2D coordinates on an integer voxel grid (each
// voxel cell is 1 unit square). For each triangle, the centroid is floored
// to a cell (cx, cy) and that cell's pixel is painted with the triangle's
// color. Every triangle gets a UV that points to the center of its cell,
// so nearest-neighbor sampling returns the cell color exactly.
//
// Output:
//   pixels: width*height*4 RGBA buffer, one pixel per voxel cell.
//   width/height: cells along x / y.
//   triUVs: one UV per triangle (size = indices.size()/3). All three
//           verts of triangle t share triUVs[t]. Caller duplicates shared
//           verts when emitting the final mesh so each triangle carries
//           its own UV.
//
// The output pixels are ready to be placed into the global atlas. The
// caller rewrites each UV from "cell center within this patch" to
// "cell center within the atlas" after packing.
struct BakedVoxelFace {
    std::vector<uint8_t> pixels; // width * height * 4, RGBA
    int width = 0;
    int height = 0;
    std::vector<glm::vec2> triUVs; // per-triangle, local 0..1 space
    bool success = false;
};

BakedVoxelFace bakeVoxelFace(
    const std::vector<glm::vec2>& verts,
    const std::vector<uint32_t>& indices,
    const std::vector<glm::vec3>& triangleColors);
