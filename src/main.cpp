#include "audio_manager.h"
#include "game_scenes.h"
#include "game_settings.h"
#include "official_content_sync.h"
#include "raylib.h"
#include "scene_manager.h"
#include "settings_io.h"
#include "theme.h"
#include "virtual_screen.h"
#include "platform/windows_app_icon.h"
#include "platform/windows_input_source.h"

int main() {
    initialize_settings_storage(g_settings);
    official_content_sync::synchronize();
    load_settings(g_settings);
    set_theme(g_settings.dark_mode);
    audio_manager::instance().set_bgm_volume(g_settings.bgm_volume);
    audio_manager::instance().set_se_volume(g_settings.se_volume);

    const resolution_preset& preset = kResolutionPresets[g_settings.resolution_index];
    InitWindow(preset.width, preset.height, "raythm");
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(g_settings.target_fps);
    SetExitKey(KEY_NULL);

    if (g_settings.fullscreen) {
        ToggleFullscreen();
    }

    apply_windows_app_icon(GetWindowHandle());
    windows_input_source::instance().initialize(GetWindowHandle());
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
        audio_manager::instance().update();
        manager.update(dt);

        BeginDrawing();
        manager.draw();
        EndDrawing();
    }

    virtual_screen::cleanup();
    windows_input_source::instance().shutdown();
    audio_manager::instance().shutdown();
    CloseWindow();
}
