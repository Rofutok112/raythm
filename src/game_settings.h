#pragma once

// シーン間で共有されるゲーム設定。
// settings_scene で変更され、play_scene / song_select_scene で参照される。
struct game_settings {
    int selected_key_count = 4;
    float camera_angle_degrees = 45.0f;
    float lane_width = 3.5f;
    float note_speed = 0.045f;
    int target_fps = 144;
};

inline game_settings g_settings;
