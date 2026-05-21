#include "settings/settings_runtime_applier.h"

#include "audio_manager.h"
#include "core/window_dialog_support.h"
#include "game_settings.h"
#include "localization/localization.h"
#include "platform/window_chrome.h"
#include "raylib.h"
#include "theme.h"
#include "ui/ui_font.h"
#include "virtual_screen.h"

void settings_runtime_applier::apply_bgm_volume(float volume) const {
    audio_manager::instance().set_bgm_volume(volume);
    audio_manager::instance().set_preview_volume(volume);
}

void settings_runtime_applier::apply_se_volume(float volume) const {
    audio_manager::instance().set_se_volume(volume);
}

void settings_runtime_applier::apply_loudness_normalization(bool enabled) const {
    audio_manager::instance().set_loudness_normalization_enabled(enabled);
}

void settings_runtime_applier::apply_fullscreen(bool fullscreen) const {
    if (fullscreen) {
        g_settings.window_maximized = window_chrome::is_maximized();
    }
    const int chrome_height = fullscreen ? 0 : window_chrome::titlebar_height_px();
    virtual_screen::set_top_reserved_pixels(chrome_height);
    window_dialog_support::set_reserved_top_chrome_height(chrome_height);
    window_dialog_support::set_fullscreen(fullscreen,
                                          fullscreen
                                              ? window_dialog_support::current_monitor_width()
                                              : g_settings.windowed_width,
                                          fullscreen
                                              ? window_dialog_support::current_monitor_height()
                                              : g_settings.windowed_height);
    if (!fullscreen && g_settings.window_maximized) {
        window_chrome::maximize();
    }
}

void settings_runtime_applier::apply_theme(bool dark_mode) const {
    set_theme(dark_mode);
}

void settings_runtime_applier::apply_locale(localization::locale locale) const {
    localization::set_current_locale(locale);
    ui::set_font_locale_mode(locale == localization::locale::japanese
                                 ? ui::font_locale_mode::japanese_ui
                                 : ui::font_locale_mode::automatic);
}
