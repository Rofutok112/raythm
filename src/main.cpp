#include "game_scenes.h"
#include "raylib.h"
#include "scene_manager.h"

int main() {
    InitWindow(1280, 720, "raythm");
    SetTargetFPS(120);

    scene_manager manager;
    manager.set_initial_scene(std::unique_ptr<scene>(new title_scene(manager)));

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        manager.update(dt);

        BeginDrawing();
        manager.draw();
        EndDrawing();
    }

    CloseWindow();
}
