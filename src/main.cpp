#include "audio_manager.h"
#include "core/window_dialog_support.h"
#include "game_scenes.h"
#include "game_settings.h"
#include "raylib.h"
#include "scene_manager.h"
#include "settings_io.h"
#include "theme.h"
#include "virtual_screen.h"
#include "platform/windows_app_icon.h"
#include "platform/windows_input_source.h"

int main() {
    initialize_settings_storage(g_settings);
    load_settings(g_settings);
    set_theme(g_settings.dark_mode);
    audio_manager::instance().set_bgm_volume(g_settings.bgm_volume);
    audio_manager::instance().set_se_volume(g_settings.se_volume);

    const resolution_preset& preset = kResolutionPresets[g_settings.resolution_index];
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(preset.width, preset.height, "raythm");
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(g_settings.target_fps);
    SetExitKey(KEY_NULL);
    SetWindowMinSize(640, 360);

    apply_windows_app_icon(GetWindowHandle());
    windows_input_source::instance().initialize(GetWindowHandle());
    window_dialog_support::set_fullscreen(g_settings.fullscreen, preset.width, preset.height);
    virtual_screen::init();

    scene_manager manager;
    manager.set_initial_scene(std::unique_ptr<scene>(new title_scene(manager)));
    int applied_target_fps = g_settings.target_fps;

    while (!WindowShouldClose()) {
        const resolution_preset& current_preset = kResolutionPresets[g_settings.resolution_index];
        if (applied_target_fps != g_settings.target_fps) {
            SetTargetFPS(g_settings.target_fps);
            applied_target_fps = g_settings.target_fps;
        }
        if (IsKeyPressed(KEY_F11)) {
            g_settings.fullscreen = !g_settings.fullscreen;
            window_dialog_support::set_fullscreen(g_settings.fullscreen, current_preset.width, current_preset.height);
            save_settings(g_settings);
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
