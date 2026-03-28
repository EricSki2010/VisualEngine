#pragma once
#include "menu_ui.h"
#include "raylib.h"

enum AppState { STATE_MENU, STATE_NAMING, STATE_EDITOR, STATE_LOAD };

struct MenuScreen {
    AppState drawAndUpdate() {
        BeginDrawing();
            ClearBackground({ 15, 15, 20, 255 });

            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();

            const char* title = "Map Creator";
            int titleSize = 48;
            int titleW = MeasureText(title, titleSize);
            DrawText(title, (screenW - titleW) / 2, screenH / 4, titleSize, WHITE);

            int btnW = 300, btnH = 50, btnX = (screenW - btnW) / 2, btnGap = 20;
            int btnStartY = screenH / 2 - btnH;

            if (drawMenuButton("Create New Structure", btnX, btnStartY, btnW, btnH))
                return STATE_NAMING;

            if (drawMenuButton("Load Game", btnX, btnStartY + btnH + btnGap, btnW, btnH))
                return STATE_LOAD;

            if (drawMenuButton("Exit", btnX, btnStartY + 2 * (btnH + btnGap), btnW, btnH))
                return (AppState)-1;

        EndDrawing();
        return STATE_MENU;
    }
};
