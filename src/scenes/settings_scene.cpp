#include "settings_scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <span>

#include "audio_manager.h"
#include "game_settings.h"
#include "key_names.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "settings_io.h"
#include "song_select_scene.h"
#include "title_scene.h"
#include "virtual_screen.h"

namespace {
constexpr int kPageCount = 4;
const char* kPageNames[] = {"Gameplay", "Audio", "Video", "Key Config"};
constexpr Rectangle kSidebarRect = {24.0f, 44.0f, 256.0f, 660.0f};
constexpr Rectangle kContentRect = {300.0f, 44.0f, 956.0f, 660.0f};
constexpr Rectangle kTabRects[kPageCount] = {
    {48.0f, 196.0f, 208.0f, 42.0f},
    {48.0f, 246.0f, 208.0f, 42.0f},
    {48.0f, 296.0f, 208.0f, 42.0f},
    {48.0f, 346.0f, 208.0f, 42.0f},
};
constexpr Rectangle kBackRect = {48.0f, 622.0f, 208.0f, 42.0f};
constexpr Rectangle kGeneralRows[] = {
    {330.0f, 160.0f, 890.0f, 48.0f},
    {330.0f, 220.0f, 890.0f, 48.0f},
    {330.0f, 280.0f, 890.0f, 48.0f},
    {330.0f, 380.0f, 890.0f, 48.0f},
    {330.0f, 440.0f, 890.0f, 48.0f},
    {330.0f, 540.0f, 890.0f, 48.0f},
    {330.0f, 600.0f, 890.0f, 48.0f},
    {330.0f, 660.0f, 890.0f, 48.0f},
};
constexpr Rectangle kKeyModeRect = {330.0f, 170.0f, 890.0f, 64.0f};
constexpr float kSliderLeft = 548.0f;
constexpr float kSliderWidth = 630.0f;
constexpr float kSliderTopOffset = 26.0f;
constexpr float kArrowButtonSize = 34.0f;

// キーコンフィグで使用可能なキーかどうか（UIキーと衝突しないもの）。
bool is_valid_play_key(int key) {
    return key != KEY_ESCAPE && key != KEY_ENTER && key != KEY_UP && key != KEY_DOWN &&
           key != KEY_LEFT && key != KEY_RIGHT && key != KEY_BACKSPACE && key != KEY_NULL;
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

Rectangle slider_track_rect(const Rectangle& row_rect) {
    return {kSliderLeft, row_rect.y + kSliderTopOffset, kSliderWidth, 6.0f};
}

Rectangle arrow_left_rect(const Rectangle& row_rect) {
    return {row_rect.x + row_rect.width - 94.0f, row_rect.y + 15.0f, kArrowButtonSize, kArrowButtonSize};
}

Rectangle arrow_right_rect(const Rectangle& row_rect) {
    return {row_rect.x + row_rect.width - 50.0f, row_rect.y + 15.0f, kArrowButtonSize, kArrowButtonSize};
}

float slider_ratio_from_mouse(const Rectangle& row_rect, Vector2 mouse) {
    const Rectangle track = slider_track_rect(row_rect);
    return clamp01((mouse.x - track.x) / track.width);
}

int fps_option_index(int target_fps) {
    constexpr std::array<int, 4> kFrameRateOptions = {120, 144, 240, 0};
    for (int i = 0; i < static_cast<int>(kFrameRateOptions.size()); ++i) {
        if (kFrameRateOptions[static_cast<size_t>(i)] == target_fps) {
            return i;
        }
    }
    return 1;
}

Color lerp_color(Color from, Color to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return {
        static_cast<unsigned char>(from.r + (to.r - from.r) * clamped),
        static_cast<unsigned char>(from.g + (to.g - from.g) * clamped),
        static_cast<unsigned char>(from.b + (to.b - from.b) * clamped),
        static_cast<unsigned char>(from.a + (to.a - from.a) * clamped),
    };
}

Rectangle inset_rect(Rectangle rect, float amount) {
    return {rect.x + amount, rect.y + amount, rect.width - amount * 2.0f, rect.height - amount * 2.0f};
}
}  // namespace

settings_scene::settings_scene(scene_manager& manager, return_target target) : scene(manager), return_target_(target) {
}

void settings_scene::on_enter() {
    current_page_ = page::gameplay;
    active_slider_ = general_slider::none;
    listening_ = false;
    key_config_slot_ = -1;
    key_config_error_.clear();
    error_timer_ = 0.0f;
}

void settings_scene::update(float dt) {
    error_timer_ = std::max(0.0f, error_timer_ - dt);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();

    // リスニング中はページ切り替え・画面遷移を無効にする
    if (listening_) {
        update_key_config();
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, kBackRect)) {
        save_settings(g_settings);
        if (return_target_ == return_target::song_select) {
            manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        } else {
            manager_.change_scene(std::make_unique<title_scene>(manager_));
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < kPageCount; ++i) {
            if (CheckCollisionPointRec(mouse, kTabRects[i])) {
                current_page_ = static_cast<page>(i);
                key_config_slot_ = -1;
                listening_ = false;
                break;
            }
        }
    }

    switch (current_page_) {
        case page::gameplay:
            update_gameplay();
            break;
        case page::audio:
            update_audio();
            break;
        case page::video:
            update_video();
            break;
        case page::key_config:
            update_key_config();
            break;
    }
}

void settings_scene::update_gameplay() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        active_slider_ = general_slider::none;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, kGeneralRows[0])) {
            active_slider_ = general_slider::note_speed;
        } else if (CheckCollisionPointRec(mouse, kGeneralRows[1])) {
            active_slider_ = general_slider::camera_angle;
        } else if (CheckCollisionPointRec(mouse, kGeneralRows[2])) {
            active_slider_ = general_slider::lane_width;
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (active_slider_ == general_slider::note_speed) {
            const float ratio = slider_ratio_from_mouse(kGeneralRows[0], mouse);
            g_settings.note_speed = 0.020f + ratio * (0.090f - 0.020f);
        } else if (active_slider_ == general_slider::camera_angle) {
            const float ratio = slider_ratio_from_mouse(kGeneralRows[1], mouse);
            g_settings.camera_angle_degrees = 5.0f + ratio * (90.0f - 5.0f);
        } else if (active_slider_ == general_slider::lane_width) {
            const float ratio = slider_ratio_from_mouse(kGeneralRows[2], mouse);
            g_settings.lane_width = 0.6f + ratio * (5.0f - 0.6f);
        }
    }
}

void settings_scene::update_audio() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        active_slider_ = general_slider::none;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, kGeneralRows[0])) {
            active_slider_ = general_slider::bgm_volume;
        } else if (CheckCollisionPointRec(mouse, kGeneralRows[1])) {
            active_slider_ = general_slider::se_volume;
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (active_slider_ == general_slider::bgm_volume) {
            const float ratio = slider_ratio_from_mouse(kGeneralRows[0], mouse);
            g_settings.bgm_volume = ratio;
            audio_manager::instance().set_bgm_volume(g_settings.bgm_volume);
            audio_manager::instance().set_preview_volume(g_settings.bgm_volume);
        } else if (active_slider_ == general_slider::se_volume) {
            const float ratio = slider_ratio_from_mouse(kGeneralRows[1], mouse);
            g_settings.se_volume = ratio;
            audio_manager::instance().set_se_volume(g_settings.se_volume);
        }
    }
}

void settings_scene::update_video() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    constexpr std::array<int, 4> kFrameRateOptions = {120, 144, 240, 0};

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        active_slider_ = general_slider::none;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, kGeneralRows[0])) {
        active_slider_ = general_slider::frame_rate;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && active_slider_ == general_slider::frame_rate) {
        const float ratio = slider_ratio_from_mouse(kGeneralRows[0], mouse);
        const int index = static_cast<int>(std::round(ratio * static_cast<float>(kFrameRateOptions.size() - 1)));
        g_settings.target_fps =
            kFrameRateOptions[static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kFrameRateOptions.size()) - 1))];
    }

    bool resolution_changed = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, arrow_left_rect(kGeneralRows[1]))) {
            g_settings.resolution_index = std::max(0, g_settings.resolution_index - 1);
            resolution_changed = true;
        } else if (CheckCollisionPointRec(mouse, arrow_right_rect(kGeneralRows[1]))) {
            g_settings.resolution_index = std::min(kResolutionPresetCount - 1, g_settings.resolution_index + 1);
            resolution_changed = true;
        }
    }

    if (resolution_changed) {
        const resolution_preset& preset = kResolutionPresets[g_settings.resolution_index];
        SetWindowSize(preset.width, preset.height);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, arrow_left_rect(kGeneralRows[2])) ||
            CheckCollisionPointRec(mouse, arrow_right_rect(kGeneralRows[2]))) {
            g_settings.fullscreen = !g_settings.fullscreen;
            ToggleFullscreen();
        }
    }
}

void settings_scene::update_key_config() {
    const int max_keys = key_config_mode_ == 0 ? 4 : 6;

    if (listening_) {
        // ESC でリスニングキャンセル
        if (IsKeyPressed(KEY_ESCAPE)) {
            listening_ = false;
            return;
        }

        // 押されたキーを検出
        const int pressed = GetKeyPressed();
        if (pressed == KEY_NULL) return;

        if (!is_valid_play_key(pressed)) {
            key_config_error_ = "This key cannot be assigned";
            error_timer_ = 2.0f;
            return;
        }

        // 重複チェック
        const std::span<KeyboardKey> keys = key_config_mode_ == 0
            ? std::span<KeyboardKey>(g_settings.keys.keys_4)
            : std::span<KeyboardKey>(g_settings.keys.keys_6);
        const int count = key_config_mode_ == 0 ? 4 : 6;
        for (int i = 0; i < count; ++i) {
            if (i != key_config_slot_ && keys[static_cast<size_t>(i)] == static_cast<KeyboardKey>(pressed)) {
                key_config_error_ = TextFormat("Key '%s' is already assigned to Lane %d", get_key_name(pressed), i + 1);
                error_timer_ = 2.0f;
                return;
            }
        }

        keys[static_cast<size_t>(key_config_slot_)] = static_cast<KeyboardKey>(pressed);
        listening_ = false;
        key_config_error_.clear();
        return;
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Rectangle mode_left = {kKeyModeRect.x + kKeyModeRect.width - 94.0f, kKeyModeRect.y + 15.0f, kArrowButtonSize, kArrowButtonSize};
        const Rectangle mode_right = {kKeyModeRect.x + kKeyModeRect.width - 50.0f, kKeyModeRect.y + 15.0f, kArrowButtonSize, kArrowButtonSize};
        if (CheckCollisionPointRec(mouse, mode_left) || CheckCollisionPointRec(mouse, mode_right)) {
            key_config_mode_ = 1 - key_config_mode_;
            key_config_slot_ = -1;
            return;
        }

        int y = 258;
        for (int i = 0; i < max_keys; ++i) {
            const Rectangle row_rect = {330.0f, static_cast<float>(y), 560.0f, 48.0f};
            if (CheckCollisionPointRec(mouse, row_rect)) {
                if (key_config_slot_ == i) {
                    listening_ = true;
                    key_config_error_.clear();
                } else {
                    key_config_slot_ = i;
                }
                return;
            }
            y += 62;
        }
    }
}

void settings_scene::draw() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    virtual_screen::begin();
    ClearBackground(RAYWHITE);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, {255, 255, 255, 255}, {241, 243, 246, 255});
    DrawRectangleRec(kSidebarRect, Color{248, 249, 251, 255});
    DrawRectangleRec(kContentRect, Color{248, 249, 251, 255});
    DrawRectangleLinesEx(kSidebarRect, 2.0f, Color{206, 210, 218, 255});
    DrawRectangleLinesEx(kContentRect, 2.0f, Color{206, 210, 218, 255});

    DrawText("SETTINGS", 46, 70, 34, BLACK);
    DrawText("Saved on exit", 48, 112, 20, Color{132, 136, 146, 255});

    for (int i = 0; i < kPageCount; ++i) {
        const bool active = static_cast<int>(current_page_) == i;
        const bool hovered = CheckCollisionPointRec(mouse, kTabRects[i]);
        const bool pressed = hovered && mouse_down;
        const Rectangle draw_rect = pressed ? inset_rect(kTabRects[i], 1.5f) : kTabRects[i];
        const Color fill = active ? lerp_color(Color{223, 228, 234, 255}, Color{210, 216, 224, 255}, hovered ? 1.0f : 0.0f)
                                  : lerp_color(Color{243, 245, 248, 255}, Color{228, 233, 239, 255}, hovered ? 1.0f : 0.0f);
        const Color border = active ? Color{182, 186, 194, 255} : Color{206, 210, 218, 255};
        DrawRectangleRec(draw_rect, fill);
        DrawRectangleLinesEx(draw_rect, 2.0f, border);
        DrawText(kPageNames[i], static_cast<int>(draw_rect.x + 14.0f), static_cast<int>(draw_rect.y + 10.0f), 22,
                 active ? BLACK : DARKGRAY);
    }

    draw_marquee_text("Click tabs to switch pages", 48, 396, 20, Color{132, 136, 146, 255},
                      208.0f, GetTime());
    const bool back_hovered = CheckCollisionPointRec(mouse, kBackRect);
    const bool back_pressed = back_hovered && mouse_down;
    const Rectangle back_draw_rect = back_pressed ? inset_rect(kBackRect, 1.5f) : kBackRect;
    DrawRectangleRec(back_draw_rect, lerp_color(Color{243, 245, 248, 255}, Color{228, 233, 239, 255}, back_hovered ? 1.0f : 0.0f));
    DrawRectangleLinesEx(back_draw_rect, 2.0f, Color{206, 210, 218, 255});
    DrawText("BACK", static_cast<int>(back_draw_rect.x + 72.0f), static_cast<int>(back_draw_rect.y + 10.0f), 22, BLACK);

    const char* page_title = "";
    const char* page_subtitle = "";
    switch (current_page_) {
        case page::gameplay:
            page_title = "Gameplay";
            page_subtitle = "Play feel and lane settings";
            break;
        case page::audio:
            page_title = "Audio";
            page_subtitle = "BGM and sound effect volume";
            break;
        case page::video:
            page_title = "Video";
            page_subtitle = "Display and frame rate settings";
            break;
        case page::key_config:
            page_title = "Key Config";
            page_subtitle = "Per-lane keyboard bindings";
            break;
    }
    DrawText(page_title, 330, 74, 34, BLACK);
    DrawText(page_subtitle, 332, 114, 20, Color{132, 136, 146, 255});

    switch (current_page_) {
        case page::gameplay:
            draw_gameplay();
            break;
        case page::audio:
            draw_audio();
            break;
        case page::video:
            draw_video();
            break;
        case page::key_config:
            draw_key_config();
            break;
    }

    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void settings_scene::draw_gameplay() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const char* labels[] = {"Note Speed", "Camera Angle", "Lane Width"};
    const std::string values[] = {
        TextFormat("%.3f", g_settings.note_speed),
        TextFormat("%.0f deg", g_settings.camera_angle_degrees),
        TextFormat("%.1f", g_settings.lane_width),
    };

    for (int i = 0; i < 3; ++i) {
        DrawRectangleRec(kGeneralRows[i], Color{243, 245, 248, 255});
        DrawRectangleLinesEx(kGeneralRows[i], 2.0f, Color{206, 210, 218, 255});
        DrawText(labels[i], static_cast<int>(kGeneralRows[i].x + 18.0f), static_cast<int>(kGeneralRows[i].y + 12.0f), 22, BLACK);
        const Rectangle track = slider_track_rect(kGeneralRows[i]);
        DrawRectangleRec(track, Color{214, 219, 226, 255});

        float ratio = 0.0f;
        if (i == 0) {
            ratio = (g_settings.note_speed - 0.020f) / (0.090f - 0.020f);
        } else if (i == 1) {
            ratio = (g_settings.camera_angle_degrees - 5.0f) / (90.0f - 5.0f);
        } else {
            ratio = (g_settings.lane_width - 0.6f) / (5.0f - 0.6f);
        }
        ratio = clamp01(ratio);

        DrawRectangle(static_cast<int>(track.x), static_cast<int>(track.y), static_cast<int>(track.width * ratio), static_cast<int>(track.height),
                      Color{132, 136, 146, 255});
        const int knob_x = static_cast<int>(track.x + track.width * ratio);
        DrawRectangle(knob_x - 6, static_cast<int>(track.y - 8.0f), 12, 22, Color{72, 72, 72, 255});

        const int value_width = MeasureText(values[i].c_str(), 22);
        DrawText(values[i].c_str(), static_cast<int>(track.x + track.width - value_width), static_cast<int>(kGeneralRows[i].y + 8.0f), 22,
                 Color{96, 100, 108, 255});
    }
}

void settings_scene::draw_audio() {
    const char* labels[] = {"BGM Volume", "SE Volume"};
    const std::string values[] = {
        TextFormat("%d%%", static_cast<int>(std::round(g_settings.bgm_volume * 100.0f))),
        TextFormat("%d%%", static_cast<int>(std::round(g_settings.se_volume * 100.0f))),
    };

    for (int row = 0; row < 2; ++row) {
        const int i = row;
        DrawRectangleRec(kGeneralRows[i], Color{243, 245, 248, 255});
        DrawRectangleLinesEx(kGeneralRows[i], 2.0f, Color{206, 210, 218, 255});
        DrawText(labels[row], static_cast<int>(kGeneralRows[i].x + 18.0f), static_cast<int>(kGeneralRows[i].y + 12.0f), 22, BLACK);

        const Rectangle track = slider_track_rect(kGeneralRows[i]);
        DrawRectangleRec(track, Color{214, 219, 226, 255});
        const float ratio = row == 0 ? g_settings.bgm_volume : g_settings.se_volume;
        DrawRectangle(static_cast<int>(track.x), static_cast<int>(track.y), static_cast<int>(track.width * ratio), static_cast<int>(track.height),
                      Color{132, 136, 146, 255});
        const int knob_x = static_cast<int>(track.x + track.width * ratio);
        DrawRectangle(knob_x - 6, static_cast<int>(track.y - 8.0f), 12, 22, Color{72, 72, 72, 255});

        const int value_width = MeasureText(values[row].c_str(), 22);
        DrawText(values[row].c_str(), static_cast<int>(track.x + track.width - value_width), static_cast<int>(kGeneralRows[i].y + 8.0f), 22,
                 Color{96, 100, 108, 255});
    }
}

void settings_scene::draw_video() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const std::string fps_label = g_settings.target_fps == 0 ? "Unlimited" : std::to_string(g_settings.target_fps);
    const std::string res_label = kResolutionPresets[g_settings.resolution_index].label;
    const std::string values[] = {fps_label, res_label, g_settings.fullscreen ? "Fullscreen" : "Windowed"};
    const char* labels[] = {"Frame Rate", "Resolution", "Display"};

    {
        const int i = 0;
        DrawRectangleRec(kGeneralRows[i], Color{243, 245, 248, 255});
        DrawRectangleLinesEx(kGeneralRows[i], 2.0f, Color{206, 210, 218, 255});
        DrawText(labels[0], static_cast<int>(kGeneralRows[i].x + 18.0f), static_cast<int>(kGeneralRows[i].y + 12.0f), 22, BLACK);
        const Rectangle track = slider_track_rect(kGeneralRows[i]);
        DrawRectangleRec(track, Color{214, 219, 226, 255});
        const float ratio = static_cast<float>(fps_option_index(g_settings.target_fps)) / 3.0f;
        DrawRectangle(static_cast<int>(track.x), static_cast<int>(track.y), static_cast<int>(track.width * ratio), static_cast<int>(track.height),
                      Color{132, 136, 146, 255});
        const int knob_x = static_cast<int>(track.x + track.width * ratio);
        DrawRectangle(knob_x - 6, static_cast<int>(track.y - 8.0f), 12, 22, Color{72, 72, 72, 255});
        const int value_width = MeasureText(values[0].c_str(), 22);
        DrawText(values[0].c_str(), static_cast<int>(track.x + track.width - value_width), static_cast<int>(kGeneralRows[i].y + 8.0f), 22,
                 Color{96, 100, 108, 255});
    }

    for (int row = 0; row < 2; ++row) {
        const int i = row + 1;
        DrawRectangleRec(kGeneralRows[i], Color{243, 245, 248, 255});
        DrawRectangleLinesEx(kGeneralRows[i], 2.0f, Color{206, 210, 218, 255});
        DrawText(labels[row + 1], static_cast<int>(kGeneralRows[i].x + 18.0f), static_cast<int>(kGeneralRows[i].y + 12.0f), 22, BLACK);
        const Rectangle left_arrow = arrow_left_rect(kGeneralRows[i]);
        const Rectangle right_arrow = arrow_right_rect(kGeneralRows[i]);
        const bool left_hovered = CheckCollisionPointRec(mouse, left_arrow);
        const bool right_hovered = CheckCollisionPointRec(mouse, right_arrow);
        const Rectangle left_draw_rect = (left_hovered && mouse_down) ? inset_rect(left_arrow, 1.5f) : left_arrow;
        const Rectangle right_draw_rect = (right_hovered && mouse_down) ? inset_rect(right_arrow, 1.5f) : right_arrow;
        DrawRectangleRec(left_draw_rect, lerp_color(Color{229, 233, 238, 255}, Color{214, 220, 227, 255}, left_hovered ? 1.0f : 0.0f));
        DrawRectangleRec(right_draw_rect, lerp_color(Color{229, 233, 238, 255}, Color{214, 220, 227, 255}, right_hovered ? 1.0f : 0.0f));
        DrawRectangleLinesEx(left_draw_rect, 2.0f, Color{206, 210, 218, 255});
        DrawRectangleLinesEx(right_draw_rect, 2.0f, Color{206, 210, 218, 255});
        DrawText("<", static_cast<int>(left_draw_rect.x + 11.0f), static_cast<int>(left_draw_rect.y + 5.0f), 24, BLACK);
        DrawText(">", static_cast<int>(right_draw_rect.x + 10.0f), static_cast<int>(right_draw_rect.y + 5.0f), 24, BLACK);

        const int value_width = MeasureText(values[row + 1].c_str(), 24);
        DrawText(values[row + 1].c_str(), static_cast<int>(left_arrow.x - value_width - 16.0f), static_cast<int>(kGeneralRows[i].y + 12.0f), 24,
                 Color{96, 100, 108, 255});
    }
}

void settings_scene::draw_key_config() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    DrawRectangleRec(kKeyModeRect, Color{243, 245, 248, 255});
    DrawRectangleLinesEx(kKeyModeRect, 2.0f, Color{206, 210, 218, 255});
    DrawText("Mode", static_cast<int>(kKeyModeRect.x + 18.0f), static_cast<int>(kKeyModeRect.y + 16.0f), 24, BLACK);
    const Rectangle mode_left = {kKeyModeRect.x + kKeyModeRect.width - 94.0f, kKeyModeRect.y + 15.0f, kArrowButtonSize, kArrowButtonSize};
    const Rectangle mode_right = {kKeyModeRect.x + kKeyModeRect.width - 50.0f, kKeyModeRect.y + 15.0f, kArrowButtonSize, kArrowButtonSize};
    const char* mode_label = key_config_mode_ == 0 ? "4K" : "6K";
    const int mode_label_width = MeasureText(mode_label, 24);
    DrawText(mode_label, static_cast<int>(mode_left.x - mode_label_width - 18.0f),
             static_cast<int>(kKeyModeRect.y + 16.0f), 24, Color{96, 100, 108, 255});
    const bool mode_left_hovered = CheckCollisionPointRec(mouse, mode_left);
    const bool mode_right_hovered = CheckCollisionPointRec(mouse, mode_right);
    const Rectangle mode_left_draw = (mode_left_hovered && mouse_down) ? inset_rect(mode_left, 1.5f) : mode_left;
    const Rectangle mode_right_draw = (mode_right_hovered && mouse_down) ? inset_rect(mode_right, 1.5f) : mode_right;
    DrawRectangleRec(mode_left_draw, lerp_color(Color{229, 233, 238, 255}, Color{214, 220, 227, 255}, mode_left_hovered ? 1.0f : 0.0f));
    DrawRectangleRec(mode_right_draw, lerp_color(Color{229, 233, 238, 255}, Color{214, 220, 227, 255}, mode_right_hovered ? 1.0f : 0.0f));
    DrawRectangleLinesEx(mode_left_draw, 2.0f, Color{206, 210, 218, 255});
    DrawRectangleLinesEx(mode_right_draw, 2.0f, Color{206, 210, 218, 255});
    DrawText("<", static_cast<int>(mode_left_draw.x + 11.0f), static_cast<int>(mode_left_draw.y + 5.0f), 24, BLACK);
    DrawText(">", static_cast<int>(mode_right_draw.x + 10.0f), static_cast<int>(mode_right_draw.y + 5.0f), 24, BLACK);

    int y = 258;

    // キースロット表示
    const std::span<const KeyboardKey> keys = key_config_mode_ == 0
        ? std::span<const KeyboardKey>(g_settings.keys.keys_4)
        : std::span<const KeyboardKey>(g_settings.keys.keys_6);
    const int count = key_config_mode_ == 0 ? 4 : 6;
    for (int i = 0; i < count; ++i) {
        const bool selected = key_config_slot_ == i;
        const bool is_listening = selected && listening_;
        const Rectangle row_rect = {330.0f, static_cast<float>(y), 560.0f, 48.0f};
        const bool hovered = CheckCollisionPointRec(mouse, row_rect);
        const Rectangle draw_rect = (hovered && mouse_down) ? inset_rect(row_rect, 1.5f) : row_rect;
        const Color fill = selected ? lerp_color(Color{223, 228, 234, 255}, Color{210, 216, 224, 255}, hovered ? 1.0f : 0.0f)
                                    : lerp_color(Color{243, 245, 248, 255}, Color{228, 233, 239, 255}, hovered ? 1.0f : 0.0f);
        DrawRectangleRec(draw_rect, fill);
        DrawRectangleLinesEx(draw_rect, 2.0f, selected ? Color{182, 186, 194, 255} : Color{206, 210, 218, 255});
        const char* key_label = is_listening ? "Press a key..." : get_key_name(keys[static_cast<size_t>(i)]);
        DrawText(TextFormat("Lane %d", i + 1), static_cast<int>(draw_rect.x + 18.0f), static_cast<int>(draw_rect.y + 12.0f), 24, BLACK);
        const int key_width = MeasureText(key_label, 24);
        DrawText(key_label, static_cast<int>(draw_rect.x + draw_rect.width - key_width - 18.0f), static_cast<int>(draw_rect.y + 12.0f), 24,
                 is_listening ? Color{220, 38, 38, 255} : Color{96, 100, 108, 255});
        y += 62;
    }

    if (error_timer_ > 0.0f && !key_config_error_.empty()) {
        const unsigned char alpha = static_cast<unsigned char>(std::min(error_timer_ / 0.3f, 1.0f) * 255.0f);
        DrawText(key_config_error_.c_str(), 330, y + 8, 22, Color{220, 38, 38, alpha});
    }
}
