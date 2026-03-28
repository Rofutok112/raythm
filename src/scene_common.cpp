#include "scene_common.h"

void draw_scene_frame(const char* title, const char* subtitle, Color accent) {
    ClearBackground({18, 24, 38, 255});

    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, {28, 36, 54, 255}, {12, 16, 26, 255});
    DrawRectangleRounded({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, {245, 247, 250, 235});
    DrawRectangleRoundedLinesEx({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, 3.0f, accent);

    DrawText(title, 130, 130, 44, accent);
    DrawText(subtitle, 130, 190, 24, DARKGRAY);
}
