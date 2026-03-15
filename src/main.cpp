#include "raylib.h"
#include "audio.h"
#include "ui/button.h"
#include "ui/ui_render_queue.h"
#include "ui/ui_renderer.h"

int main() {
    InitWindow(1280, 720, "test");
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

    button play_button({ 40.0f, 40.0f, 180.0f, 56.0f }, DARKGRAY);
    ui_render_queue ui_queue;

    while (!WindowShouldClose()) {
        ui_queue.clear();
        play_button.build_render_data(ui_queue);

        BeginDrawing();
            ClearBackground(WHITE);
            BeginMode3D(camera);
                DrawPlane({ 0, 0, 0 }, { 5, 3 }, GRAY);   // 位置, サイズ(幅x奥行), 色
            EndMode3D();

            ui_renderer::render(ui_queue);
            DrawText("Play", 96, 56, 24, RAYWHITE);
            DrawText("Hello", 640, 360, 40, LIGHTGRAY);
        EndDrawing();
    }
}
