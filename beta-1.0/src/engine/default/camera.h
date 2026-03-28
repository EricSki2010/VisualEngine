#pragma once
#include "raylib.h"
#include "engine/default/input.h"

// ── Camera / view system ────────────────────────────────────────────
// Wraps Camera3D with view mode presets and target following.

struct GameCamera {
    Camera3D cam = {};
    ViewMode mode = VIEW_2_5D;
    Vector3 offset = { -15.0f, 25.0f, -15.0f };

    void init(ViewMode startMode = VIEW_2_5D) {
        cam.up = { 0.0f, 1.0f, 0.0f };
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        setView(startMode);
    }

    void setView(ViewMode newMode) {
        mode = newMode;
        switch (mode) {
            case VIEW_2_5D:
                offset = { -15.0f, 25.0f, -15.0f };
                break;
            case VIEW_TOPDOWN:
                offset = { 0.0f, 35.0f, -0.1f };
                break;
            case VIEW_3D:
                offset = { 0.0f, 10.0f, -15.0f };
                break;
        }
    }

    // Follow a target position (e.g. player feet + some Y offset)
    void follow(Vector3 target) {
        cam.target   = { target.x, target.y + 1.0f, target.z };
        cam.position = { target.x + offset.x, target.y + offset.y, target.z + offset.z };
    }
};
