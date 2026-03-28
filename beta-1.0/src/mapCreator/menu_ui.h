#pragma once
#include "raylib.h"

static bool drawMenuButton(const char* text, int x, int y, int w, int h, bool enabled = true) {
    Vector2 mouse = GetMousePosition();
    bool hovered = enabled && mouse.x >= x && mouse.x < x + w && mouse.y >= y && mouse.y < y + h;
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    Color bg, border;
    if (!enabled) {
        bg = { 25, 25, 30, 200 };
        border = { 60, 60, 60, 200 };
    } else {
        bg = hovered ? Color{ 60, 60, 80, 230 } : Color{ 35, 35, 45, 230 };
        border = hovered ? WHITE : Color{ 100, 100, 100, 200 };
    }

    DrawRectangle(x, y, w, h, bg);
    DrawRectangleLines(x, y, w, h, border);

    int fontSize = 24;
    int textW = MeasureText(text, fontSize);
    Color textCol = enabled ? WHITE : Color{ 80, 80, 80, 255 };
    DrawText(text, x + (w - textW) / 2, y + (h - fontSize) / 2, fontSize, textCol);

    return clicked;
}
