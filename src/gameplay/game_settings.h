#pragma once

#include "input_handler.h"

// 解像度プリセット
struct resolution_preset {
    int width;
    int height;
    const char* label;
};

inline constexpr resolution_preset kResolutionPresets[] = {
    {1280, 720, "1280x720"},
    {1920, 1080, "1920x1080"},
    {2560, 1440, "2560x1440"},
};
inline constexpr int kResolutionPresetCount = 3;

// シーン間で共有されるゲーム設定。
// settings_scene で変更され、play_scene / song_select_scene で参照される。
struct game_settings {
    int selected_key_count = 4;
    float camera_angle_degrees = 45.0f;
    float lane_width = 3.5f;
    float note_speed = 0.045f;
    float bgm_volume = 1.0f;
    float se_volume = 1.0f;
    int target_fps = 144;
    key_config keys;
    int resolution_index = 1;  // デフォルト 1920x1080
    bool fullscreen = false;
    bool dark_mode = false;
};

inline game_settings g_settings;
