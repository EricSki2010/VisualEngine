#pragma once
#include "raylib.h"

// ── Input handling ──────────────────────────────────────────────────
// Returns raw movement direction based on WASD, mapped to the current view.

struct InputDir {
    float x = 0.0f;
    float z = 0.0f;
};

enum ViewMode {
    VIEW_2_5D,      // isometric-style: camera at (-15, +25, -15)
    VIEW_TOPDOWN,   // straight down
    VIEW_3D,        // behind the player
};

// Get WASD input mapped to world directions for the given view mode
inline InputDir getMovementInput(ViewMode mode = VIEW_2_5D) {
    InputDir dir;

    switch (mode) {
        case VIEW_2_5D:
            // Camera looks from (-15, +25, -15), so "forward" on screen is (+X, +Z)
            if (IsKeyDown(KEY_W)) { dir.x += 1.0f; dir.z += 1.0f; }
            if (IsKeyDown(KEY_S)) { dir.x -= 1.0f; dir.z -= 1.0f; }
            if (IsKeyDown(KEY_A)) { dir.x += 1.0f; dir.z -= 1.0f; }
            if (IsKeyDown(KEY_D)) { dir.x -= 1.0f; dir.z += 1.0f; }
            break;

        case VIEW_TOPDOWN:
            if (IsKeyDown(KEY_W)) { dir.z -= 1.0f; }
            if (IsKeyDown(KEY_S)) { dir.z += 1.0f; }
            if (IsKeyDown(KEY_A)) { dir.x -= 1.0f; }
            if (IsKeyDown(KEY_D)) { dir.x += 1.0f; }
            break;

        case VIEW_3D:
            if (IsKeyDown(KEY_W)) { dir.z += 1.0f; }
            if (IsKeyDown(KEY_S)) { dir.z -= 1.0f; }
            if (IsKeyDown(KEY_A)) { dir.x -= 1.0f; }
            if (IsKeyDown(KEY_D)) { dir.x += 1.0f; }
            break;
    }

    return dir;
}

inline bool jumpPressed()  { return IsKeyPressed(KEY_SPACE); }
inline bool jumpHeld()     { return IsKeyDown(KEY_SPACE); }
