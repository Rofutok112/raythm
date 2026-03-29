#include "game_scenes.h"
#include "game_settings.h"
#include "raylib.h"
#include "scene_manager.h"
#include "settings_io.h"
#include "virtual_screen.h"

int main() {
    load_settings(g_settings);

    const resolution_preset& preset = kResolutionPresets[g_settings.resolution_index];
    InitWindow(preset.width, preset.height, "raythm");
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(g_settings.target_fps);
    SetExitKey(KEY_NULL);

    if (g_settings.fullscreen) {
        ToggleFullscreen();
    }

    virtual_screen::init();

    scene_manager manager;
    manager.set_initial_scene(std::unique_ptr<scene>(new title_scene(manager)));
    int applied_target_fps = g_settings.target_fps;

    while (!WindowShouldClose()) {
        if (applied_target_fps != g_settings.target_fps) {
            SetTargetFPS(g_settings.target_fps);
            applied_target_fps = g_settings.target_fps;
        }
        const float dt = GetFrameTime();
        manager.update(dt);

        BeginDrawing();
        manager.draw();
        EndDrawing();
    }

    virtual_screen::cleanup();
    CloseWindow();
}
