#include "settings_scene.h"

#include <algorithm>
#include <array>
#include <memory>

#include "game_settings.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "title_scene.h"

settings_scene::settings_scene(scene_manager& manager) : scene(manager) {
}

// 左右でカメラ角度、上下でノート速度、Q/Eでレーン幅を調整する。
void settings_scene::update(float dt) {
    (void)dt;

    constexpr std::array<int, 4> kFrameRateOptions = {120, 144, 240, 0};
    const bool previous_frame_rate = IsKeyPressed(KEY_Z);
    const bool next_frame_rate = IsKeyPressed(KEY_X);

    if (IsKeyPressed(KEY_LEFT)) {
        g_settings.camera_angle_degrees = std::max(5.0f, g_settings.camera_angle_degrees - 5.0f);
    } else if (IsKeyPressed(KEY_RIGHT)) {
        g_settings.camera_angle_degrees = std::min(90.0f, g_settings.camera_angle_degrees + 5.0f);
    }

    if (IsKeyPressed(KEY_UP)) {
        g_settings.note_speed = std::min(0.090f, g_settings.note_speed + 0.005f);
    } else if (IsKeyPressed(KEY_DOWN)) {
        g_settings.note_speed = std::max(0.020f, g_settings.note_speed - 0.005f);
    }

    if (IsKeyPressed(KEY_Q)) {
        g_settings.lane_width = std::max(0.6f, g_settings.lane_width - 0.2f);
    } else if (IsKeyPressed(KEY_E)) {
        g_settings.lane_width = std::min(5.0f, g_settings.lane_width + 0.2f);
    }

    if (previous_frame_rate || next_frame_rate) {
        size_t current_index = 0;
        for (size_t i = 0; i < kFrameRateOptions.size(); ++i) {
            if (kFrameRateOptions[i] == g_settings.target_fps) {
                current_index = i;
                break;
            }
        }

        if (previous_frame_rate) {
            current_index = (current_index + kFrameRateOptions.size() - 1) % kFrameRateOptions.size();
        } else {
            current_index = (current_index + 1) % kFrameRateOptions.size();
        }

        g_settings.target_fps = kFrameRateOptions[current_index];
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
        manager_.change_scene(std::make_unique<title_scene>(manager_));
    }
}

// 現在の設定値と操作案内を表示する。
void settings_scene::draw() {
    const char* frame_rate_label = g_settings.target_fps == 0 ? "Unlimited" : TextFormat("%d", g_settings.target_fps);

    draw_scene_frame("Settings", "ENTER or ESC: Back to Title", {124, 58, 237, 255});
    DrawText("Audio Offset: 0 ms", 130, 300, 36, BLACK);
    DrawText(TextFormat("Note Speed: %.3f", g_settings.note_speed), 130, 360, 36, BLACK);
    DrawText(TextFormat("Camera Angle: %.0f deg", g_settings.camera_angle_degrees), 130, 420, 36, BLACK);
    DrawText(TextFormat("Lane Width: %.1f", g_settings.lane_width), 130, 480, 36, BLACK);
    DrawText(TextFormat("Frame Rate: %s", frame_rate_label), 130, 540, 36, BLACK);
    DrawText("LEFT/RIGHT: Camera Angle", 130, 586, 24, GRAY);
    DrawText("UP/DOWN: Note Speed    Q/E: Lane Width", 130, 620, 24, GRAY);
    DrawText("Z/X: Frame Rate", 130, 654, 24, GRAY);
}
