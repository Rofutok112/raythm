#include "settings/settings_runtime_applier.h"

#include "audio_manager.h"
#include "game_settings.h"
#include "raylib.h"
#include "theme.h"

void settings_runtime_applier::apply_bgm_volume(float volume) const {
    audio_manager::instance().set_bgm_volume(volume);
    audio_manager::instance().set_preview_volume(volume);
}

void settings_runtime_applier::apply_se_volume(float volume) const {
    audio_manager::instance().set_se_volume(volume);
}

void settings_runtime_applier::apply_resolution(int resolution_index) const {
    const resolution_preset& preset = kResolutionPresets[resolution_index];
    SetWindowSize(preset.width, preset.height);
}

void settings_runtime_applier::toggle_fullscreen() const {
    ToggleFullscreen();
}

void settings_runtime_applier::apply_theme(bool dark_mode) const {
    set_theme(dark_mode);
}
