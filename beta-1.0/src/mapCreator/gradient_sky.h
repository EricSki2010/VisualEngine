#pragma once
#include "raylib.h"
#include <cmath>

struct GradientSky {
    Color topColor    = { 5, 5, 10, 255 };
    Color bottomColor = { 120, 120, 130, 255 };

    void draw(float pitch) {
        int w = GetScreenWidth() + 100;
        int h = GetScreenHeight() + 100;

        float pitchNorm = (pitch + 89.0f) / 178.0f;
        int center = (int)(h * (1.0f - pitchNorm));

        int strips = 64;
        for (int i = 0; i < strips; i++) {
            int y0 = i * h / strips;
            int y1 = (i + 1) * h / strips;

            float t = (float)i / (float)(strips - 1);

            float shifted = t + (pitchNorm - 0.5f) * 1.5f;
            if (shifted < 0.0f) shifted = 0.0f;
            if (shifted > 1.0f) shifted = 1.0f;

            unsigned char r = (unsigned char)(topColor.r + shifted * (bottomColor.r - topColor.r));
            unsigned char g = (unsigned char)(topColor.g + shifted * (bottomColor.g - topColor.g));
            unsigned char b = (unsigned char)(topColor.b + shifted * (bottomColor.b - topColor.b));

            DrawRectangle(-50, y0 - 50, w, y1 - y0, { r, g, b, 255 });
        }
    }
};
