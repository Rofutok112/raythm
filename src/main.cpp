#include "audio_manager.h"
#include "core/window_dialog_support.h"
#include "game_scenes.h"
#include "game_settings.h"
#include "raylib.h"
#include "scene_manager.h"
#include "settings_io.h"
#include "theme.h"
#include "ui/ui_font.h"
#include "virtual_screen.h"
#include "platform/windows_app_icon.h"
#include "platform/windows_input_source.h"

int main() {
    initialize_settings_storage(g_settings);
    load_settings(g_settings);
    set_theme(g_settings.dark_mode);
    audio_manager::instance().set_bgm_volume(g_settings.bgm_volume);
    audio_manager::instance().set_se_volume(g_settings.se_volume);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(g_settings.windowed_width, g_settings.windowed_height, "raythm");
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(g_settings.target_fps);
    SetExitKey(KEY_NULL);
    SetWindowMinSize(640, 360);

    apply_windows_app_icon(GetWindowHandle());
    windows_input_source::instance().initialize(GetWindowHandle());
    window_dialog_support::set_fullscreen(g_settings.fullscreen,
                                          window_dialog_support::current_monitor_width(),
                                          window_dialog_support::current_monitor_height());
    virtual_screen::init();
    ui::initialize_text_font();

    scene_manager manager;
    manager.set_initial_scene(std::unique_ptr<scene>(new title_scene(manager)));
    int applied_target_fps = g_settings.target_fps;
    bool was_window_focused = IsWindowFocused();

    while (!WindowShouldClose()) {
        if (applied_target_fps != g_settings.target_fps) {
            SetTargetFPS(g_settings.target_fps);
            applied_target_fps = g_settings.target_fps;
        }
        if (!window_dialog_support::is_fullscreen() && IsWindowResized()) {
            g_settings.windowed_width = GetScreenWidth();
            g_settings.windowed_height = GetScreenHeight();
        }
        if (IsKeyPressed(KEY_F11)) {
            g_settings.fullscreen = !g_settings.fullscreen;
            window_dialog_support::set_fullscreen(g_settings.fullscreen,
                                                  window_dialog_support::current_monitor_width(),
                                                  window_dialog_support::current_monitor_height());
            save_settings(g_settings);
        }
        const bool window_focused = IsWindowFocused();
        if (window_dialog_support::is_fullscreen() && was_window_focused && !window_focused) {
            window_dialog_support::minimize_window();
        }
        was_window_focused = window_focused;
        const float dt = GetFrameTime();
        audio_manager::instance().update();
        manager.update(dt);

        BeginDrawing();
        manager.draw();
        EndDrawing();
    }

    save_settings(g_settings);
    virtual_screen::cleanup();
    ui::shutdown_text_font();
    windows_input_source::instance().shutdown();
    audio_manager::instance().shutdown();
    CloseWindow();
}
