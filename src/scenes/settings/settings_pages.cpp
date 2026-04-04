#include "settings/settings_pages.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "raylib.h"
#include "settings/settings_layout.h"
#include "ui_draw.h"

namespace {

constexpr std::array<int, 4> kFrameRateOptions = {120, 144, 240, 0};
constexpr float kMinNoteSpeed = 0.010f;
constexpr float kMaxNoteSpeed = 0.200f;
constexpr float kMinNoteHeight = 0.5f;
constexpr float kMaxNoteHeight = 2.0f;
constexpr float kNoteSpeedDisplayScale = 10.0f;

std::string format_offset_label(int offset_ms) {
    return (offset_ms > 0 ? "+" : "") + std::to_string(offset_ms) + " ms";
}

}  // namespace

settings_gameplay_page::settings_gameplay_page(game_settings& settings) : settings_(settings) {
}

void settings_gameplay_page::reset_interaction() {
    active_slider_ = slider::none;
}

void settings_gameplay_page::update() {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        active_slider_ = slider::none;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (ui::is_hovered(settings::kGeneralRows[0], settings::kLayer)) {
            active_slider_ = slider::note_speed;
        } else if (ui::is_hovered(settings::kGeneralRows[1], settings::kLayer)) {
            active_slider_ = slider::camera_angle;
        } else if (ui::is_hovered(settings::kGeneralRows[2], settings::kLayer)) {
            active_slider_ = slider::lane_width;
        } else if (ui::is_hovered(settings::kGeneralRows[3], settings::kLayer)) {
            active_slider_ = slider::note_height;
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (active_slider_ == slider::note_speed) {
            const float ratio = settings::slider_ratio_from_mouse(settings::kGeneralRows[0]);
            settings_.note_speed = kMinNoteSpeed + ratio * (kMaxNoteSpeed - kMinNoteSpeed);
        } else if (active_slider_ == slider::camera_angle) {
            const float ratio = settings::slider_ratio_from_mouse(settings::kGeneralRows[1]);
            settings_.camera_angle_degrees = 5.0f + ratio * (90.0f - 5.0f);
        } else if (active_slider_ == slider::lane_width) {
            const float ratio = settings::slider_ratio_from_mouse(settings::kGeneralRows[2]);
            settings_.lane_width = 0.6f + ratio * (10.0f - 0.6f);
        } else if (active_slider_ == slider::note_height) {
            const float ratio = settings::slider_ratio_from_mouse(settings::kGeneralRows[3]);
            settings_.note_height = kMinNoteHeight + ratio * (kMaxNoteHeight - kMinNoteHeight);
        }
    }

    if (ui::is_clicked(settings::double_arrow_left_rect(settings::kGeneralRows[4]), settings::kLayer)) {
        settings_.global_note_offset_ms = std::max(-10000, settings_.global_note_offset_ms - 5);
    } else if (ui::is_clicked(settings::single_arrow_left_rect(settings::kGeneralRows[4]), settings::kLayer)) {
        settings_.global_note_offset_ms = std::max(-10000, settings_.global_note_offset_ms - 1);
    } else if (ui::is_clicked(settings::single_arrow_right_rect(settings::kGeneralRows[4]), settings::kLayer)) {
        settings_.global_note_offset_ms = std::min(10000, settings_.global_note_offset_ms + 1);
    } else if (ui::is_clicked(settings::double_arrow_right_rect(settings::kGeneralRows[4]), settings::kLayer)) {
        settings_.global_note_offset_ms = std::min(10000, settings_.global_note_offset_ms + 5);
    }
}

void settings_gameplay_page::draw() const {
    const char* labels[] = {"Note Speed", "Camera Angle", "Lane Width", "Note Height"};
    const std::string values[] = {
        TextFormat("%.1f", settings_.note_speed * kNoteSpeedDisplayScale),
        TextFormat("%.0f deg", settings_.camera_angle_degrees),
        TextFormat("%.1f", settings_.lane_width),
        TextFormat("%.2f", settings_.note_height),
    };

    for (int i = 0; i < 4; ++i) {
        float ratio = 0.0f;
        if (i == 0) {
            ratio = (settings_.note_speed - kMinNoteSpeed) / (kMaxNoteSpeed - kMinNoteSpeed);
        } else if (i == 1) {
            ratio = (settings_.camera_angle_degrees - 5.0f) / (90.0f - 5.0f);
        } else if (i == 2) {
            ratio = (settings_.lane_width - 0.6f) / (10.0f - 0.6f);
        } else {
            ratio = (settings_.note_height - kMinNoteHeight) / (kMaxNoteHeight - kMinNoteHeight);
        }
        ui::draw_slider_relative(settings::kGeneralRows[static_cast<std::size_t>(i)], labels[i], values[i].c_str(),
                                 settings::clamp01(ratio), settings::kSliderLeftInset, settings::kSliderRightInset,
                                 settings::kLayer, 22, settings::kSliderTopOffset);
    }

    const std::string global_offset_label = format_offset_label(settings_.global_note_offset_ms);
    const ui::row_state offset_row = ui::draw_row(settings::kGeneralRows[4], g_theme->row, g_theme->row_hover, g_theme->border);
    const Rectangle content = ui::inset(offset_row.visual, ui::edge_insets::symmetric(0.0f, 18.0f));
    const ui::rect_pair columns = ui::split_columns(content, 200.0f);
    const Rectangle button_group = ui::place(columns.second, settings::kArrowButtonSize * 4.0f + 30.0f,
                                             settings::kArrowButtonSize,
                                             ui::anchor::center_right, ui::anchor::center_right);
    const Rectangle value_rect = {
        columns.second.x,
        columns.second.y,
        button_group.x - columns.second.x - 16.0f,
        columns.second.height
    };

    ui::draw_text_in_rect("Global Offset", 22, columns.first, g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(global_offset_label.c_str(), 22, value_rect, g_theme->text_dim, ui::text_align::right);
    ui::draw_button(settings::double_arrow_left_rect(settings::kGeneralRows[4]), "<<", 22);
    ui::draw_button(settings::single_arrow_left_rect(settings::kGeneralRows[4]), "<", 22);
    ui::draw_button(settings::single_arrow_right_rect(settings::kGeneralRows[4]), ">", 22);
    ui::draw_button(settings::double_arrow_right_rect(settings::kGeneralRows[4]), ">>", 22);
}

settings_audio_page::settings_audio_page(game_settings& settings, const settings_runtime_applier& runtime_applier)
    : settings_(settings), runtime_applier_(runtime_applier) {
}

void settings_audio_page::reset_interaction() {
    active_slider_ = slider::none;
}

void settings_audio_page::update() {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        active_slider_ = slider::none;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (ui::is_hovered(settings::kGeneralRows[0], settings::kLayer)) {
            active_slider_ = slider::bgm_volume;
        } else if (ui::is_hovered(settings::kGeneralRows[1], settings::kLayer)) {
            active_slider_ = slider::se_volume;
        }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (active_slider_ == slider::bgm_volume) {
            settings_.bgm_volume = settings::slider_ratio_from_mouse(settings::kGeneralRows[0]);
            runtime_applier_.apply_bgm_volume(settings_.bgm_volume);
        } else if (active_slider_ == slider::se_volume) {
            settings_.se_volume = settings::slider_ratio_from_mouse(settings::kGeneralRows[1]);
            runtime_applier_.apply_se_volume(settings_.se_volume);
        }
    }
}

void settings_audio_page::draw() const {
    const char* labels[] = {"BGM Volume", "SE Volume"};
    const std::string values[] = {
        TextFormat("%d%%", static_cast<int>(std::round(settings_.bgm_volume * 100.0f))),
        TextFormat("%d%%", static_cast<int>(std::round(settings_.se_volume * 100.0f))),
    };

    for (int row = 0; row < 2; ++row) {
        const float ratio = row == 0 ? settings_.bgm_volume : settings_.se_volume;
        ui::draw_slider_relative(settings::kGeneralRows[static_cast<std::size_t>(row)], labels[row], values[row].c_str(), ratio,
                                 settings::kSliderLeftInset, settings::kSliderRightInset,
                                 settings::kLayer, 22, settings::kSliderTopOffset);
    }
}

settings_video_page::settings_video_page(game_settings& settings, const settings_runtime_applier& runtime_applier)
    : settings_(settings), runtime_applier_(runtime_applier) {
}

void settings_video_page::reset_interaction() {
    dragging_frame_rate_ = false;
}

void settings_video_page::update() {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        dragging_frame_rate_ = false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ui::is_hovered(settings::kGeneralRows[0], settings::kLayer)) {
        dragging_frame_rate_ = true;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && dragging_frame_rate_) {
        const float ratio = settings::slider_ratio_from_mouse(settings::kGeneralRows[0]);
        const int index = static_cast<int>(std::round(ratio * static_cast<float>(kFrameRateOptions.size() - 1)));
        settings_.target_fps =
            kFrameRateOptions[static_cast<std::size_t>(std::clamp(index, 0, static_cast<int>(kFrameRateOptions.size()) - 1))];
    }

    bool resolution_changed = false;

    if (ui::is_clicked(settings::arrow_left_rect(settings::kGeneralRows[1]), settings::kLayer)) {
        settings_.resolution_index = std::max(0, settings_.resolution_index - 1);
        resolution_changed = true;
    } else if (ui::is_clicked(settings::arrow_right_rect(settings::kGeneralRows[1]), settings::kLayer)) {
        settings_.resolution_index = std::min(kResolutionPresetCount - 1, settings_.resolution_index + 1);
        resolution_changed = true;
    }

    if (resolution_changed) {
        runtime_applier_.apply_resolution(settings_.resolution_index);
    }

    if (ui::is_clicked(settings::arrow_left_rect(settings::kGeneralRows[2]), settings::kLayer) ||
        ui::is_clicked(settings::arrow_right_rect(settings::kGeneralRows[2]), settings::kLayer)) {
        settings_.fullscreen = !settings_.fullscreen;
        runtime_applier_.toggle_fullscreen();
    }

    if (ui::is_clicked(settings::arrow_left_rect(settings::kGeneralRows[3]), settings::kLayer) ||
        ui::is_clicked(settings::arrow_right_rect(settings::kGeneralRows[3]), settings::kLayer)) {
        settings_.dark_mode = !settings_.dark_mode;
        runtime_applier_.apply_theme(settings_.dark_mode);
    }
}

void settings_video_page::draw() const {
    const std::string fps_label = settings_.target_fps == 0 ? "Unlimited" : std::to_string(settings_.target_fps);
    ui::draw_slider_relative(settings::kGeneralRows[0], "Frame Rate", fps_label.c_str(),
                             static_cast<float>(settings::fps_option_index(settings_.target_fps)) / 3.0f,
                             settings::kSliderLeftInset, settings::kSliderRightInset,
                             settings::kLayer, 22, settings::kSliderTopOffset);
    ui::draw_value_selector(settings::kGeneralRows[1], "Resolution", kResolutionPresets[settings_.resolution_index].label, settings::kLayer);
    ui::draw_value_selector(settings::kGeneralRows[2], "Display", settings_.fullscreen ? "Fullscreen" : "Windowed", settings::kLayer);
    ui::draw_value_selector(settings::kGeneralRows[3], "Theme", settings_.dark_mode ? "Dark" : "Light", settings::kLayer);
}

settings_key_config_page::settings_key_config_page(game_settings& settings) : settings_(settings) {
}

void settings_key_config_page::reset() {
    state_.reset();
}

void settings_key_config_page::clear_selection() {
    state_.clear_selection();
}

void settings_key_config_page::tick(float dt) {
    state_.tick(dt);
}

void settings_key_config_page::update() {
    state_.update(settings_);
}

void settings_key_config_page::draw() const {
    state_.draw(settings_);
}

bool settings_key_config_page::blocks_navigation() const {
    return state_.blocks_navigation();
}
