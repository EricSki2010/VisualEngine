#pragma once
#include "raylib.h"
#include <cmath>

struct EditorCamera {
    Camera3D cam = {};
    Vector3 origin = { 0.0f, 0.0f, 0.0f };
    float yaw   = -45.0f;
    float pitch  = 35.0f;
    float distance = 30.0f;
    float panSpeed = 20.0f;
    float orbitSpeed = 0.30f;
    float zoomSpeed = 3.0f;
    float minDist = 2.0f;
    float maxDist = 200.0f;
    float minPitch = -89.0f;
    float maxPitch = 89.0f;

    void init() {
        cam.up = { 0.0f, 1.0f, 0.0f };
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        updatePosition();
    }

    void update(float dt) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            yaw   -= delta.x * orbitSpeed;
            pitch += delta.y * orbitSpeed;
            if (pitch < minPitch) pitch = minPitch;
            if (pitch > maxPitch) pitch = maxPitch;
        }

        float yawRad = yaw * DEG2RAD;
        float forwardX = sinf(yawRad);
        float forwardZ = cosf(yawRad);
        float rightX = cosf(yawRad);
        float rightZ = -sinf(yawRad);

        float moveX = 0.0f, moveZ = 0.0f;
        if (IsKeyDown(KEY_W)) { moveX -= forwardX; moveZ -= forwardZ; }
        if (IsKeyDown(KEY_S)) { moveX += forwardX; moveZ += forwardZ; }
        if (IsKeyDown(KEY_D)) { moveX += rightX;   moveZ += rightZ; }
        if (IsKeyDown(KEY_A)) { moveX -= rightX;   moveZ -= rightZ; }

        float len = sqrtf(moveX * moveX + moveZ * moveZ);
        if (len > 0.0f) {
            moveX /= len;
            moveZ /= len;
            origin.x += moveX * panSpeed * dt;
            origin.z += moveZ * panSpeed * dt;
        }

        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) {
            distance += zoomSpeed * 10.0f * dt;
            if (distance > maxDist) distance = maxDist;
        }
        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD)) {
            distance -= zoomSpeed * 10.0f * dt;
            if (distance < minDist) distance = minDist;
        }

        if (IsKeyDown(KEY_LEFT_SHIFT)) origin.y -= panSpeed * dt;
        if (IsKeyDown(KEY_SPACE))      origin.y += panSpeed * dt;

        updatePosition();
    }

    void updatePosition() {
        float yawRad = yaw * DEG2RAD;
        float pitchRad = pitch * DEG2RAD;

        cam.position = {
            origin.x + distance * cosf(pitchRad) * sinf(yawRad),
            origin.y + distance * sinf(pitchRad),
            origin.z + distance * cosf(pitchRad) * cosf(yawRad)
        };
        cam.target = origin;
    }

    void setOrigin(float x, float y, float z) {
        origin = { x, y, z };
        updatePosition();
    }
};
