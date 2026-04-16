#pragma once

#include <string>

// Exports the current 3D model scene (colliders + palette) as a .glb file.
// Single primitive, single unlit material, per-vertex colors via COLOR_0.
// Vertices are transformed into world space (rotation + position baked in).
// Normals are recomputed from the final triangle winding. Each triangle's
// three vertices carry the triangle's palette color (or the default grey
// for unpainted faces); shared positions between differently-colored
// triangles are duplicated so each triangle keeps its own color.
//
// Returns true on success, false on error (e.g. file write failed).
bool exportModelToGlb(const std::string& outPath);
