#include "settings/settings_pages.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "localization/localization.h"
#include "raylib.h"
#include "settings/settings_layout.h"
#include "ui_draw.h"

namespace {
using localization::text_key;

const char* ltr(text_key key) {
    return localization::tr(key);
}

constexpr std::array<int, 4> kFrameRateOptions = {120, 144, 240, 360};
constexpr float kMinNoteSpeed = 0.010f;
constexpr float kMaxNoteSpeed = 0.200f;
constexpr float kMinNoteHeight = 0.5f;
constexpr float kMaxNoteHeight = 2.0f;
constexpr float kNoteSpeedDisplayScale = 10.0f;
constexpr int kNoActiveSlider = -1;

enum class slider_display_format { integer_percent, one_decimal, two_decimal, degrees, note_speed };

struct slider_row_definition {
    localization::text_key label;
    float game_settings::* value;
    float min_value;
    float max_value;
    slider_display_format display_format;
};

constexpr std::array<slider_row_definition, 5> kGameplaySliders = {{
    {text_key::note_speed, &game_settings::note_speed, kMinNoteSpeed, kMaxNoteSpeed, slider_display_format::note_speed},
    {text_key::camera_angle, &game_settings::camera_angle_degrees, 5.0f, 90.0f, slider_display_format::degrees},
    {text_key::lane_width, &game_settings::lane_width, kMinLaneWidth, kMaxLaneWidth, slider_display_format::one_decimal},
    {text_key::note_height, &game_settings::note_height, kMinNoteHeight, kMaxNoteHeight, slider_display_format::two_decimal},
    {text_key::lane_cover,
     &game_settings::lane_fog_hidden_percent,
     kMinLaneFogHiddenPercent,
     kMaxLaneFogHiddenPercent,
     slider_display_format::integer_percent},
}};

constexpr std::array<slider_row_definition, 3> kAudioSliders = {{
    {text_key::bgm_volume, &game_settings::bgm_volume, 0.0f, 1.0f, slider_display_format::integer_percent},
    {text_key::se_volume, &game_settings::se_volume, 0.0f, 1.0f, slider_display_format::integer_percent},
    {text_key::hitsound_pan, &game_settings::hitsound_pan_strength, 0.0f, 1.0f, slider_display_format::integer_percent},
}};

std::string format_offset_label(int offset_ms) {
    return (offset_ms > 0 ? "+" : "") + std::to_string(offset_ms) + " ms";
}

std::string format_slider_value(float value, const slider_row_definition& slider) {
    switch (slider.display_format) {
        case slider_display_format::integer_percent:
            return TextFormat("%d%%", static_cast<int>(std::round(value * 100.0f / slider.max_value)));
        case slider_display_format::one_decimal:
            return TextFormat("%.1f", value);
        case slider_display_format::two_decimal:
            return TextFormat("%.2f", value);
        case slider_display_format::degrees:
            return TextFormat("%.0f deg", value);
        case slider_display_format::note_speed:
            return TextFormat("%.1f", value * kNoteSpeedDisplayScale);
    }
    return {};
}

float slider_ratio(float value, const slider_row_definition& slider) {
    return settings::clamp01((value - slider.min_value) / (slider.max_value - slider.min_value));
}

void apply_slider_value(game_settings& settings, const slider_row_definition& slider, const Rectangle& row) {
    const float ratio = settings::slider_ratio_from_mouse(row);
    settings.*(slider.value) = slider.min_value + ratio * (slider.max_value - slider.min_value);
}

template <std::size_t SliderCount, std::size_t RowCount>
int pressed_slider_index(const std::array<slider_row_definition, SliderCount>& sliders,
                         const std::array<Rectangle, RowCount>& rows) {
    static_assert(RowCount >= SliderCount);
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return kNoActiveSlider;
    }
    for (std::size_t i = 0; i < sliders.size(); ++i) {
        if (ui::is_hovered(rows[i], settings::kLayer)) {
            return static_cast<int>(i);
        }
    }
    return kNoActiveSlider;
}

template <std::size_t SliderCount, std::size_t RowCount>
void draw_slider_rows(const game_settings& settings,
                      const std::array<slider_row_definition, SliderCount>& sliders,
                      const std::array<Rectangle, RowCount>& rows) {
    static_assert(RowCount >= SliderCount);
    for (std::size_t i = 0; i < sliders.size(); ++i) {
        const slider_row_definition& slider = sliders[i];
        const float value = settings.*(slider.value);
        const std::string display_value = format_slider_value(value, slider);
        ui::draw_slider_relative(rows[i], ltr(slider.label), display_value.c_str(),
                                 slider_ratio(value, slider), settings::kSliderLeftInset, settings::kSliderRightInset,
                                 settings::kLayer, 22, settings::kSliderTopOffset);
    }
}

bool selector_clicked(const Rectangle& row) {
    return ui::is_clicked(settings::arrow_left_rect(row), settings::kLayer) ||
           ui::is_clicked(settings::arrow_right_rect(row), settings::kLayer);
}

}  // namespace

settings_gameplay_page::settings_gameplay_page(game_settings& settings) : settings_(settings), preview_(settings) {
}

void settings_gameplay_page::reset_interaction() {
    active_slider_index_ = kNoActiveSlider;
}

void settings_gameplay_page::prepare_frame() {
    preview_.prepare_frame();
}

void settings_gameplay_page::update() {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        active_slider_index_ = kNoActiveSlider;
    }

    const int pressed_index = pressed_slider_index(kGameplaySliders, settings::kGameplayRows);
    if (pressed_index != kNoActiveSlider) {
        active_slider_index_ = pressed_index;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && active_slider_index_ != kNoActiveSlider) {
        const std::size_t index = static_cast<std::size_t>(active_slider_index_);
        apply_slider_value(settings_, kGameplaySliders[index], settings::kGameplayRows[index]);
    }

    if (ui::is_clicked(settings::double_arrow_left_rect(settings::kGameplayRows[5]), settings::kLayer)) {
        settings_.global_note_offset_ms = std::max(-10000, settings_.global_note_offset_ms - 5);
    } else if (ui::is_clicked(settings::single_arrow_left_rect(settings::kGameplayRows[5]), settings::kLayer)) {
        settings_.global_note_offset_ms = std::max(-10000, settings_.global_note_offset_ms - 1);
    } else if (ui::is_clicked(settings::single_arrow_right_rect(settings::kGameplayRows[5]), settings::kLayer)) {
        settings_.global_note_offset_ms = std::min(10000, settings_.global_note_offset_ms + 1);
    } else if (ui::is_clicked(settings::double_arrow_right_rect(settings::kGameplayRows[5]), settings::kLayer)) {
        settings_.global_note_offset_ms = std::min(10000, settings_.global_note_offset_ms + 5);
    }
}

void settings_gameplay_page::draw() const {
    draw_slider_rows(settings_, kGameplaySliders, settings::kGameplayRows);

    const std::string global_offset_label = format_offset_label(settings_.global_note_offset_ms);
    const ui::row_state offset_row = ui::draw_row(settings::kGameplayRows[5], g_theme->row, g_theme->row_hover, g_theme->border);
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

    ui::draw_text_in_rect(ltr(text_key::global_offset), 22, columns.first, g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(global_offset_label.c_str(), 22, value_rect, g_theme->text_dim, ui::text_align::right);
    ui::draw_button(settings::double_arrow_left_rect(settings::kGameplayRows[5]), "<<", 22);
    ui::draw_button(settings::single_arrow_left_rect(settings::kGameplayRows[5]), "<", 22);
    ui::draw_button(settings::single_arrow_right_rect(settings::kGameplayRows[5]), ">", 22);
    ui::draw_button(settings::double_arrow_right_rect(settings::kGameplayRows[5]), ">>", 22);
    preview_.draw(settings::kGameplayPreviewRect);
}

settings_audio_page::settings_audio_page(game_settings& settings, const settings_runtime_applier& runtime_applier)
    : settings_(settings), runtime_applier_(runtime_applier) {
}

void settings_audio_page::reset_interaction() {
    active_slider_index_ = kNoActiveSlider;
}

void settings_audio_page::update() {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        active_slider_index_ = kNoActiveSlider;
    }

    const int pressed_index = pressed_slider_index(kAudioSliders, settings::kGeneralRows);
    if (pressed_index != kNoActiveSlider) {
        active_slider_index_ = pressed_index;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && active_slider_index_ != kNoActiveSlider) {
        const std::size_t index = static_cast<std::size_t>(active_slider_index_);
        apply_slider_value(settings_, kAudioSliders[index], settings::kGeneralRows[index]);
        if (kAudioSliders[index].value == &game_settings::bgm_volume) {
            runtime_applier_.apply_bgm_volume(settings_.bgm_volume);
        } else if (kAudioSliders[index].value == &game_settings::se_volume) {
            runtime_applier_.apply_se_volume(settings_.se_volume);
        }
    }

    if (selector_clicked(settings::kGeneralRows[3])) {
        settings_.loudness_normalization_enabled = !settings_.loudness_normalization_enabled;
        runtime_applier_.apply_loudness_normalization(settings_.loudness_normalization_enabled);
    }
}

void settings_audio_page::draw() const {
    draw_slider_rows(settings_, kAudioSliders, settings::kGeneralRows);

    ui::draw_value_selector(settings::kGeneralRows[3], ltr(text_key::loudness_normalization),
                            settings_.loudness_normalization_enabled ? ltr(text_key::enabled) : ltr(text_key::disabled),
                            settings::kLayer);
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

}

void settings_video_page::draw() const {
    const std::string fps_label = std::to_string(sanitize_target_fps(settings_.target_fps));
    ui::draw_slider_relative(settings::kGeneralRows[0], ltr(text_key::frame_rate), fps_label.c_str(),
                             static_cast<float>(settings::fps_option_index(settings_.target_fps)) / 3.0f,
                             settings::kSliderLeftInset, settings::kSliderRightInset,
                             settings::kLayer, 22, settings::kSliderTopOffset);
}

settings_system_page::settings_system_page(game_settings& settings, const settings_runtime_applier& runtime_applier)
    : settings_(settings), runtime_applier_(runtime_applier) {
}

void settings_system_page::reset_interaction() {
}

void settings_system_page::update() {
    if (selector_clicked(settings::kGeneralRows[0])) {
        settings_.ui_locale = settings_.ui_locale == localization::locale::english
                                  ? localization::locale::japanese
                                  : localization::locale::english;
        runtime_applier_.apply_locale(settings_.ui_locale);
    }

    if (selector_clicked(settings::kGeneralRows[1])) {
        settings_.fullscreen = !settings_.fullscreen;
        runtime_applier_.apply_fullscreen(settings_.fullscreen);
    }

    if (selector_clicked(settings::kGeneralRows[2])) {
        settings_.dark_mode = !settings_.dark_mode;
        runtime_applier_.apply_theme(settings_.dark_mode);
    }
}

void settings_system_page::draw() const {
    ui::draw_value_selector(settings::kGeneralRows[0], ltr(text_key::language),
                            localization::locale_display_name(settings_.ui_locale), settings::kLayer);
    ui::draw_value_selector(settings::kGeneralRows[1], ltr(text_key::display),
                            settings_.fullscreen ? ltr(text_key::fullscreen) : ltr(text_key::windowed),
                            settings::kLayer);
    ui::draw_value_selector(settings::kGeneralRows[2], ltr(text_key::theme),
                            settings_.dark_mode ? ltr(text_key::dark) : ltr(text_key::light),
                            settings::kLayer);
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
