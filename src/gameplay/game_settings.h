#pragma once

#include <string>

#include "input_handler.h"

// 解像度プリセット
struct resolution_preset {
    int width;
    int height;
    const char* label;
};

inline constexpr resolution_preset kResolutionPresets[] = {
    {1920, 1080, "1920x1080"},
    {2560, 1440, "2560x1440"},
    {3840, 2160, "3840x2160"},
};
inline constexpr int kResolutionPresetCount = 3;
inline constexpr float kMinLaneWidth = 0.6f;
inline constexpr float kMaxLaneWidth = 10.0f;

// シーン間で共有されるゲーム設定。
// settings_scene で変更され、play_scene / song_select_scene で参照される。
struct game_settings {
    int selected_key_count = 4;
    float camera_angle_degrees = 45.0f;
    float lane_width = 3.5f;
    float note_speed = 0.045f;
    float note_height = 1.0f;
    int global_note_offset_ms = 0;
    float bgm_volume = 1.0f;
    float se_volume = 1.0f;
    int target_fps = 144;
    key_config keys;
    int resolution_index = 0;  // デフォルト 1920x1080
    int windowed_width = 1920;
    int windowed_height = 1080;
    bool fullscreen = false;
    bool dark_mode = true;
};

inline game_settings g_settings;
