#include "raylib.h"
#include "audio.h"

int main() {
    InitWindow(1280, 720, "raythm");
    SetTargetFPS(120);

    Camera3D camera = {};
    camera.position = { 0.0f, 5.0f, 10.0f };
    camera.target   = { 0.0f, 0.0f, 0.0f };
    camera.up       = { 0.0f, 1.0f, 0.0f };
    camera.fovy     = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    audio audio;
    audio.load("../assets/audio/music.mp3");
    audio.play();

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(WHITE);
            BeginMode3D(camera);
                DrawPlane({ 0, 0, 0 }, { 5, 3 }, GRAY);   // 位置, サイズ(幅x奥行), 色
            EndMode3D();

            DrawText("Hello", 640, 360, 40, LIGHTGRAY);
        EndDrawing();
    }
}