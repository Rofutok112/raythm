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
#include "theme.h"
#include "title_scene.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {
constexpr int kPageCount = 4;
const char* kPageNames[] = {"Gameplay", "Audio", "Video", "Key Config"};
constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kSidebarRect = ui::place(kScreenRect, 256.0f, 660.0f,
                                             ui::anchor::top_left, ui::anchor::top_left,
                                             {24.0f, 44.0f});
constexpr Rectangle kContentRect = ui::place(kScreenRect, 956.0f, 660.0f,
                                             ui::anchor::top_left, ui::anchor::top_left,
                                             {300.0f, 44.0f});
constexpr Rectangle kSidebarHeaderRect = ui::place(kSidebarRect, 208.0f, 62.0f,
                                                   ui::anchor::top_left, ui::anchor::top_left,
                                                   {22.0f, 26.0f});
constexpr Rectangle kSidebarHintRect = ui::place(kSidebarRect, 208.0f, 24.0f,
                                                 ui::anchor::top_left, ui::anchor::top_left,
                                                 {24.0f, 352.0f});
constexpr Rectangle kTabArea = ui::place(kSidebarRect, 208.0f, 4.0f * 42.0f + 3.0f * 8.0f,
                                         ui::anchor::top_center, ui::anchor::top_center,
                                         {0.0f, 152.0f});
constexpr Rectangle kBackRect = ui::place(kSidebarRect, 208.0f, 42.0f,
                                          ui::anchor::bottom_center, ui::anchor::bottom_center,
                                          {0.0f, -38.0f});
constexpr Rectangle kContentHeaderRect = ui::place(kContentRect, 560.0f, 60.0f,
                                                   ui::anchor::top_left, ui::anchor::top_left,
                                                   {30.0f, 30.0f});
constexpr Rectangle kGeneralRows[] = {
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 116.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 176.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 236.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 336.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 396.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 496.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 556.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 616.0f}),
};
constexpr Rectangle kKeyModeRect = ui::place(kContentRect, 890.0f, 64.0f,
                                             ui::anchor::top_left, ui::anchor::top_left,
                                             {30.0f, 126.0f});
constexpr float kSliderLeftInset = 218.0f;
constexpr float kSliderRightInset = 42.0f;
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
    return ui::make_slider_layout(row_rect, kSliderLeftInset, kSliderRightInset, 200.0f, 18.0f, kSliderTopOffset).track_rect;
}

Rectangle arrow_left_rect(const Rectangle& row_rect) {
    const Rectangle content = ui::inset(row_rect, ui::edge_insets::symmetric(0.0f, 18.0f));
    const ui::rect_pair columns = ui::split_columns(content, 200.0f);
    const Rectangle button_pair_area = ui::place(columns.second, kArrowButtonSize * 2.0f + 10.0f, kArrowButtonSize,
                                                 ui::anchor::center_right, ui::anchor::center_right);
    return {button_pair_area.x, button_pair_area.y, kArrowButtonSize, kArrowButtonSize};
}

Rectangle arrow_right_rect(const Rectangle& row_rect) {
    const Rectangle left = arrow_left_rect(row_rect);
    return {left.x + kArrowButtonSize + 10.0f, left.y, kArrowButtonSize, kArrowButtonSize};
}

Rectangle key_slot_rect(int index) {
    return ui::place(kContentRect, 560.0f, 48.0f,
                     ui::anchor::top_left, ui::anchor::top_left,
                     {30.0f, 214.0f + static_cast<float>(index) * 62.0f});
}

void build_tab_rects(std::span<Rectangle> out) {
    ui::vstack(kTabArea, 42.0f, 8.0f, out);
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

    // リスニング中はページ切り替え・画面遷移を無効にする
    if (listening_) {
        update_key_config();
        return;
    }

    if (ui::is_clicked(kBackRect)) {
        save_settings(g_settings);
        if (return_target_ == return_target::song_select) {
            manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        } else {
            manager_.change_scene(std::make_unique<title_scene>(manager_));
        }
        return;
    }

    Rectangle tabs[kPageCount];
    build_tab_rects(tabs);
    for (int i = 0; i < kPageCount; ++i) {
        if (ui::is_clicked(tabs[i])) {
            current_page_ = static_cast<page>(i);
            key_config_slot_ = -1;
            listening_ = false;
            break;
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
        if (ui::is_hovered(kGeneralRows[0])) {
            active_slider_ = general_slider::note_speed;
        } else if (ui::is_hovered(kGeneralRows[1])) {
            active_slider_ = general_slider::camera_angle;
        } else if (ui::is_hovered(kGeneralRows[2])) {
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
            g_settings.lane_width = 0.6f + ratio * (10.0f - 0.6f);
        }
    }
}

void settings_scene::update_audio() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        active_slider_ = general_slider::none;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (ui::is_hovered(kGeneralRows[0])) {
            active_slider_ = general_slider::bgm_volume;
        } else if (ui::is_hovered(kGeneralRows[1])) {
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

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ui::is_hovered(kGeneralRows[0])) {
        active_slider_ = general_slider::frame_rate;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && active_slider_ == general_slider::frame_rate) {
        const float ratio = slider_ratio_from_mouse(kGeneralRows[0], mouse);
        const int index = static_cast<int>(std::round(ratio * static_cast<float>(kFrameRateOptions.size() - 1)));
        g_settings.target_fps =
            kFrameRateOptions[static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kFrameRateOptions.size()) - 1))];
    }

    bool resolution_changed = false;

    if (ui::is_clicked(arrow_left_rect(kGeneralRows[1]))) {
        g_settings.resolution_index = std::max(0, g_settings.resolution_index - 1);
        resolution_changed = true;
    } else if (ui::is_clicked(arrow_right_rect(kGeneralRows[1]))) {
        g_settings.resolution_index = std::min(kResolutionPresetCount - 1, g_settings.resolution_index + 1);
        resolution_changed = true;
    }

    if (resolution_changed) {
        const resolution_preset& preset = kResolutionPresets[g_settings.resolution_index];
        SetWindowSize(preset.width, preset.height);
    }

    if (ui::is_clicked(arrow_left_rect(kGeneralRows[2])) ||
        ui::is_clicked(arrow_right_rect(kGeneralRows[2]))) {
        g_settings.fullscreen = !g_settings.fullscreen;
        ToggleFullscreen();
    }

    // テーマ切り替え
    if (ui::is_clicked(arrow_left_rect(kGeneralRows[3])) ||
        ui::is_clicked(arrow_right_rect(kGeneralRows[3]))) {
        g_settings.dark_mode = !g_settings.dark_mode;
        set_theme(g_settings.dark_mode);
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

    const Rectangle mode_left = arrow_left_rect(kKeyModeRect);
    const Rectangle mode_right = arrow_right_rect(kKeyModeRect);
    if (ui::is_clicked(mode_left) || ui::is_clicked(mode_right)) {
        key_config_mode_ = 1 - key_config_mode_;
        key_config_slot_ = -1;
        return;
    }

    for (int i = 0; i < max_keys; ++i) {
        const Rectangle row_rect = key_slot_rect(i);
        if (ui::is_clicked(row_rect)) {
            if (key_config_slot_ == i) {
                listening_ = true;
                key_config_error_.clear();
            } else {
                key_config_slot_ = i;
            }
            return;
        }
    }
}

void settings_scene::draw() {
    const auto& t = *g_theme;
    virtual_screen::begin();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);
    ui::draw_panel(kSidebarRect);
    ui::draw_panel(kContentRect);

    ui::draw_header_block(kSidebarHeaderRect, "SETTINGS", "Saved on exit");

    Rectangle tabs[kPageCount];
    build_tab_rects(tabs);
    for (int i = 0; i < kPageCount; ++i) {
        if (static_cast<int>(current_page_) == i) {
            ui::draw_button_colored(tabs[i], kPageNames[i], 22,
                                    t.row_selected, t.row_active, t.text);
        } else {
            ui::draw_button_colored(tabs[i], kPageNames[i], 22,
                                    t.row, t.row_hover, t.text_secondary);
        }
    }

    draw_marquee_text("Click tabs to switch pages", static_cast<int>(kSidebarHintRect.x), static_cast<int>(kSidebarHintRect.y),
                      20, t.text_muted, kSidebarHintRect.width, GetTime());
    ui::draw_button(kBackRect, "BACK", 22);

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
    ui::draw_header_block(kContentHeaderRect, page_title, page_subtitle);

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
    const char* labels[] = {"Note Speed", "Camera Angle", "Lane Width"};
    const std::string values[] = {
        TextFormat("%.3f", g_settings.note_speed),
        TextFormat("%.0f deg", g_settings.camera_angle_degrees),
        TextFormat("%.1f", g_settings.lane_width),
    };

    for (int i = 0; i < 3; ++i) {
        float ratio = 0.0f;
        if (i == 0) {
            ratio = (g_settings.note_speed - 0.020f) / (0.090f - 0.020f);
        } else if (i == 1) {
            ratio = (g_settings.camera_angle_degrees - 5.0f) / (90.0f - 5.0f);
        } else {
            ratio = (g_settings.lane_width - 0.6f) / (10.0f - 0.6f);
        }
        ui::draw_slider_relative(kGeneralRows[i], labels[i], values[i].c_str(), clamp01(ratio),
                                 kSliderLeftInset, kSliderRightInset, 22, kSliderTopOffset);
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
        const float ratio = row == 0 ? g_settings.bgm_volume : g_settings.se_volume;
        ui::draw_slider_relative(kGeneralRows[i], labels[row], values[row].c_str(), ratio,
                                 kSliderLeftInset, kSliderRightInset, 22, kSliderTopOffset);
    }
}

void settings_scene::draw_video() {
    const std::string fps_label = g_settings.target_fps == 0 ? "Unlimited" : std::to_string(g_settings.target_fps);
    ui::draw_slider_relative(kGeneralRows[0], "Frame Rate", fps_label.c_str(),
                             static_cast<float>(fps_option_index(g_settings.target_fps)) / 3.0f,
                             kSliderLeftInset, kSliderRightInset, 22, kSliderTopOffset);
    ui::draw_value_selector(kGeneralRows[1], "Resolution", kResolutionPresets[g_settings.resolution_index].label);
    ui::draw_value_selector(kGeneralRows[2], "Display", g_settings.fullscreen ? "Fullscreen" : "Windowed");
    ui::draw_value_selector(kGeneralRows[3], "Theme", g_settings.dark_mode ? "Dark" : "Light");
}

void settings_scene::draw_key_config() {
    const auto& t = *g_theme;
    ui::draw_value_selector(kKeyModeRect, "Mode", key_config_mode_ == 0 ? "4K" : "6K");

    // キースロット表示
    const std::span<const KeyboardKey> keys = key_config_mode_ == 0
        ? std::span<const KeyboardKey>(g_settings.keys.keys_4)
        : std::span<const KeyboardKey>(g_settings.keys.keys_6);
    const int count = key_config_mode_ == 0 ? 4 : 6;
    for (int i = 0; i < count; ++i) {
        const bool selected = key_config_slot_ == i;
        const bool is_listening = selected && listening_;
        const Rectangle row_rect = key_slot_rect(i);
        const ui::row_state row_state = ui::draw_selectable_row(row_rect, selected);
        const char* key_label = is_listening ? "Press a key..." : get_key_name(keys[static_cast<size_t>(i)]);
        const ui::rect_pair columns = ui::split_columns(ui::inset(row_state.visual, 18.0f), 160.0f);
        ui::draw_text_in_rect(TextFormat("Lane %d", i + 1), 24, columns.first, t.text, ui::text_align::left);
        ui::draw_text_in_rect(key_label, 24, columns.second, is_listening ? t.error : t.text_dim, ui::text_align::right);
    }

    if (error_timer_ > 0.0f && !key_config_error_.empty()) {
        const unsigned char alpha = static_cast<unsigned char>(std::min(error_timer_ / 0.3f, 1.0f) * 255.0f);
        ui::draw_text_in_rect(key_config_error_.c_str(), 22,
                              ui::place(kContentRect, 560.0f, 28.0f,
                                        ui::anchor::top_left, ui::anchor::top_left,
                                        {30.0f, 214.0f + static_cast<float>(count) * 62.0f + 8.0f}),
                              with_alpha(t.error, alpha), ui::text_align::left);
    }
}
