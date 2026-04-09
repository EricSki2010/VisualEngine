#pragma once

#include <string>

// Exports the current 3D model scene (colliders + palette) as a .glb file.
// Meshes are grouped by paint color (one primitive per color) plus one
// primitive for unpainted geometry. Vertices are transformed into world
// space (rotation + position baked in). Normals are recomputed from the
// final triangle winding.
//
// Returns true on success, false on error (e.g. file write failed).
bool exportModelToGlb(const std::string& outPath);
