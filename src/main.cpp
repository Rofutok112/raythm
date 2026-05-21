#include <algorithm>

#include "audio_manager.h"
#include "core/window_dialog_support.h"
#include "game_settings.h"
#include "localization/localization.h"
#include "raylib.h"
#include "scene_manager.h"
#include "title_scene.h"
#include "settings_io.h"
#include "theme.h"
#include "ui/ui_font.h"
#include "ui/ui_notice.h"
#include "virtual_screen.h"
#include "platform/windows_app_icon.h"
#include "platform/window_chrome.h"
#include "platform/windows_input_source.h"

int main() {
    initialize_settings_storage(g_settings);
    load_settings(g_settings);
    g_settings.windowed_width = kDefaultWindowedWidth;
    g_settings.windowed_height = kDefaultWindowedHeight;
    localization::set_current_locale(g_settings.ui_locale);
    set_theme(g_settings.dark_mode);
    audio_manager::instance().set_bgm_volume(g_settings.bgm_volume);
    audio_manager::instance().set_se_volume(g_settings.se_volume);
    audio_manager::instance().set_loudness_normalization_enabled(g_settings.loudness_normalization_enabled);

#ifdef _WIN32
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
    const int chrome_height = window_chrome::titlebar_height_px();
#ifdef _WIN32
    InitWindow(g_settings.windowed_width, g_settings.windowed_height + chrome_height, "raythm");
#else
    InitWindow(g_settings.windowed_width, g_settings.windowed_height, "raythm");
#endif
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(g_settings.target_fps);
    SetExitKey(KEY_NULL);
#ifdef _WIN32
    SetWindowMinSize(640, 360 + chrome_height);
#else
    SetWindowMinSize(640, 360);
#endif

    apply_windows_app_icon(GetWindowHandle());
    const bool custom_chrome_enabled = window_chrome::initialize(GetWindowHandle());
    windows_input_source::instance().initialize(GetWindowHandle());
    const int reserved_top = custom_chrome_enabled ? chrome_height : 0;
    virtual_screen::set_top_reserved_pixels(reserved_top);
    window_dialog_support::set_reserved_top_chrome_height(reserved_top);
    if (g_settings.fullscreen) {
        virtual_screen::set_top_reserved_pixels(0);
        window_dialog_support::set_reserved_top_chrome_height(0);
    }
    window_dialog_support::set_fullscreen(g_settings.fullscreen,
                                          g_settings.fullscreen
                                              ? window_dialog_support::current_monitor_width()
                                              : g_settings.windowed_width,
                                          g_settings.fullscreen
                                              ? window_dialog_support::current_monitor_height()
                                              : g_settings.windowed_height);
    if (!g_settings.fullscreen && g_settings.window_maximized) {
        window_chrome::maximize();
    }
    virtual_screen::init();
    ui::set_font_locale_mode(g_settings.ui_locale == localization::locale::japanese
                                 ? ui::font_locale_mode::japanese_ui
                                 : ui::font_locale_mode::automatic);
    ui::initialize_text_font();

    scene_manager manager;
    manager.set_initial_scene(std::unique_ptr<scene>(new title_scene(manager)));
    int applied_target_fps = g_settings.target_fps;
    bool was_window_focused = IsWindowFocused();
    bool was_window_maximized = window_chrome::is_maximized();

    while (!WindowShouldClose() && !manager.exit_requested()) {
        windows_input_source::instance().begin_frame();
        if (applied_target_fps != g_settings.target_fps) {
            SetTargetFPS(g_settings.target_fps);
            applied_target_fps = g_settings.target_fps;
        }
        if (!window_dialog_support::is_fullscreen() && IsWindowResized()) {
            g_settings.window_maximized = window_chrome::is_maximized();
        }
        if (!window_dialog_support::is_fullscreen() && !window_chrome::is_state_transitioning()) {
            const bool window_maximized = window_chrome::is_maximized();
            if (was_window_maximized && !window_maximized) {
                window_dialog_support::apply_windowed_layout(kDefaultWindowedWidth, kDefaultWindowedHeight);
                g_settings.windowed_width = kDefaultWindowedWidth;
                g_settings.windowed_height = kDefaultWindowedHeight;
                g_settings.window_maximized = false;
            } else if (window_maximized) {
                g_settings.window_maximized = true;
            }
            was_window_maximized = window_maximized;
        }
        if (IsKeyPressed(KEY_F11)) {
            g_settings.fullscreen = !g_settings.fullscreen;
            if (g_settings.fullscreen) {
                g_settings.window_maximized = window_chrome::is_maximized();
                virtual_screen::set_top_reserved_pixels(0);
                window_dialog_support::set_reserved_top_chrome_height(0);
            } else {
                virtual_screen::set_top_reserved_pixels(reserved_top);
                window_dialog_support::set_reserved_top_chrome_height(reserved_top);
            }
            window_dialog_support::set_fullscreen(g_settings.fullscreen,
                                                  g_settings.fullscreen
                                                      ? window_dialog_support::current_monitor_width()
                                                      : g_settings.windowed_width,
                                                  g_settings.fullscreen
                                                      ? window_dialog_support::current_monitor_height()
                                                      : g_settings.windowed_height);
            if (!g_settings.fullscreen && g_settings.window_maximized) {
                window_chrome::maximize();
            }
            was_window_maximized = window_chrome::is_maximized();
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
        window_chrome::update(manager);
        ui::tick_global_notices(dt);

        BeginDrawing();
        manager.draw();
        window_chrome::draw();
        ui::draw_global_notices({
            0.0f,
            static_cast<float>(virtual_screen::top_reserved_pixels()),
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight() - virtual_screen::top_reserved_pixels())
        });
        EndDrawing();
        windows_input_source::instance().end_frame();
    }

    manager.shutdown();
    save_settings(g_settings);
    virtual_screen::cleanup();
    ui::shutdown_text_font();
    windows_input_source::instance().shutdown();
    window_chrome::shutdown();
    audio_manager::instance().shutdown();
    CloseWindow();
}
