#include "raylib.h"

int main() {
    InitWindow(1280, 720, "raythm");
    SetTargetFPS(120);

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(WHITE);
            DrawText("Hello", 640, 360, 40, LIGHTGRAY);
        EndDrawing();
    }
}