#include "settings/settings_pages.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
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

enum class slider_display_format { integer_percent, one_decimal, two_decimal, degrees, note_speed };

struct slider_row_definition {
    localization::text_key label;
    float game_settings::* value;
    float min_value;
    float max_value;
    slider_display_format display_format;
};

enum class system_selector_kind {
    language,
    fullscreen,
    theme,
};

struct system_selector_definition {
    localization::text_key label;
    std::size_t row_index = 0;
    system_selector_kind kind = system_selector_kind::language;
};

struct bool_selector_definition {
    localization::text_key label;
    std::size_t row_index = 0;
    bool game_settings::* value = nullptr;
};

struct offset_stepper_button {
    Rectangle rect{};
    const char* label = "";
    int delta = 0;
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

constexpr std::array<system_selector_definition, 3> kSystemSelectors = {{
    {text_key::language, 0, system_selector_kind::language},
    {text_key::display, 1, system_selector_kind::fullscreen},
    {text_key::theme, 2, system_selector_kind::theme},
}};

constexpr bool_selector_definition kLoudnessSelector{
    text_key::loudness_normalization,
    3,
    &game_settings::loudness_normalization_enabled,
};

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

ui::slider_options slider_options() {
    return {
        .layer = settings::kLayer,
        .font_size = 22,
        .track_top_offset = settings::kSliderTopOffset,
    };
}

void draw_settings_slider_row(Rectangle row,
                              localization::text_key label,
                              const char* display_value,
                              float ratio) {
    ui::slider_relative(row, ltr(label), display_value,
                        ratio, settings::kSliderLeftInset, settings::kSliderRightInset,
                        slider_options());
}

void draw_settings_selector_row(Rectangle row, localization::text_key label, const char* value) {
    ui::value_selector(row, ltr(label), value, settings::value_selector_options());
}

const char* system_selector_value(const game_settings& settings, system_selector_kind kind) {
    switch (kind) {
        case system_selector_kind::language:
            return localization::locale_display_name(settings.ui_locale);
        case system_selector_kind::fullscreen:
            return settings.fullscreen ? ltr(text_key::fullscreen) : ltr(text_key::windowed);
        case system_selector_kind::theme:
            return settings.dark_mode ? ltr(text_key::dark) : ltr(text_key::light);
    }
    return "";
}

void apply_system_selector(settings_page_update_result& result,
                           const game_settings& settings,
                           system_selector_kind kind) {
    switch (kind) {
        case system_selector_kind::language:
            result.locale = settings.ui_locale == localization::locale::english
                ? localization::locale::japanese
                : localization::locale::english;
            break;
        case system_selector_kind::fullscreen:
            result.bool_changes.push_back({
                &game_settings::fullscreen,
                !settings.fullscreen,
            });
            break;
        case system_selector_kind::theme:
            result.bool_changes.push_back({
                &game_settings::dark_mode,
                !settings.dark_mode,
            });
            break;
    }
}

float slider_value_from_mouse(const slider_row_definition& slider, const Rectangle& row) {
    const float ratio = settings::slider_ratio_from_mouse(row);
    return slider.min_value + ratio * (slider.max_value - slider.min_value);
}

template <std::size_t SliderCount, std::size_t RowCount>
ui::indexed_drag_result update_slider_drag(const std::array<slider_row_definition, SliderCount>& sliders,
                                           const std::array<Rectangle, RowCount>& rows,
                                           ui::indexed_drag_state& drag_state) {
    static_assert(RowCount >= SliderCount);
    return ui::update_indexed_drag(std::span<const Rectangle>(rows.data(), sliders.size()),
                                   drag_state,
                                   settings::kLayer);
}

template <std::size_t SliderCount, std::size_t RowCount>
void append_slider_drag_change(settings_page_update_result& result,
                               const std::array<slider_row_definition, SliderCount>& sliders,
                               const std::array<Rectangle, RowCount>& rows,
                               ui::indexed_drag_state& drag_state) {
    const ui::indexed_drag_result slider_drag = update_slider_drag(sliders, rows, drag_state);
    if (!slider_drag.dragging) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(slider_drag.active_index);
    result.float_changes.push_back({
        sliders[index].value,
        slider_value_from_mouse(sliders[index], rows[index]),
    });
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
        draw_settings_slider_row(rows[i], slider.label, display_value.c_str(), slider_ratio(value, slider));
    }
}

bool selector_clicked(const Rectangle& row) {
    return ui::is_clicked(settings::arrow_left_rect(row), settings::kLayer) ||
           ui::is_clicked(settings::arrow_right_rect(row), settings::kLayer);
}

const char* bool_selector_value(const game_settings& settings, const bool_selector_definition& selector) {
    return settings.*(selector.value) ? ltr(text_key::enabled) : ltr(text_key::disabled);
}

void append_bool_selector_change(settings_page_update_result& result,
                                 const game_settings& settings,
                                 const bool_selector_definition& selector) {
    if (selector.value == nullptr || !selector_clicked(settings::kGeneralRows[selector.row_index])) {
        return;
    }
    result.bool_changes.push_back({
        selector.value,
        !(settings.*(selector.value)),
    });
}

void draw_bool_selector_row(const game_settings& settings, const bool_selector_definition& selector) {
    draw_settings_selector_row(settings::kGeneralRows[selector.row_index],
                               selector.label,
                               bool_selector_value(settings, selector));
}

std::array<offset_stepper_button, 4> offset_stepper_buttons_for(const settings::offset_stepper_layout& layout) {
    const std::array<Rectangle, 4> buttons = settings::offset_stepper_button_rects(layout);
    std::array<offset_stepper_button, 4> result{};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        result[i] = {
            .rect = buttons[i],
            .label = settings::kOffsetStepperLabels[i],
            .delta = settings::kOffsetStepperDeltas[i],
        };
    }
    return result;
}

ui::button_state draw_offset_stepper_button(const offset_stepper_button& button) {
    return ui::button(button.rect, button.label, {
        .layer = settings::kLayer,
        .font_size = 22,
    });
}

int offset_stepper_delta(Rectangle row) {
    const settings::offset_stepper_layout layout = settings::global_offset_stepper_layout(row);
    for (const offset_stepper_button& button : offset_stepper_buttons_for(layout)) {
        if (ui::is_clicked(button.rect, settings::kLayer)) {
            return button.delta;
        }
    }
    return 0;
}

void draw_settings_stepper_row(Rectangle row,
                               localization::text_key label,
                               const char* display_value,
                               const settings::offset_stepper_layout& layout) {
    ui::row(row, {
        .layer = settings::kLayer,
        .border_width = 2.0f,
        .bg = g_theme->row,
        .bg_hover = g_theme->row_hover,
        .border_color = g_theme->border,
        .custom_colors = true,
    });
    ui::draw_text_in_rect(ltr(label), 22, layout.label_rect, g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(display_value, 22, layout.value_rect, g_theme->text_dim, ui::text_align::right);
    for (const offset_stepper_button& button : offset_stepper_buttons_for(layout)) {
        draw_offset_stepper_button(button);
    }
}

}  // namespace

settings_gameplay_page::settings_gameplay_page(game_settings& settings) : settings_(settings), preview_(settings) {
}

void settings_gameplay_page::reset_interaction() {
    ui::reset_indexed_drag(slider_drag_);
}

void settings_gameplay_page::prepare_frame() {
    preview_.prepare_frame();
}

settings_page_update_result settings_gameplay_page::update() {
    settings_page_update_result result;
    append_slider_drag_change(result, kGameplaySliders, settings::kGameplayRows, slider_drag_);

    if (const int delta = offset_stepper_delta(settings::kGameplayRows[5]); delta != 0) {
        result.int_changes.push_back({
            &game_settings::global_note_offset_ms,
            std::clamp(settings_.global_note_offset_ms + delta, -10000, 10000),
        });
    }
    return result;
}

void settings_gameplay_page::draw() const {
    draw_slider_rows(settings_, kGameplaySliders, settings::kGameplayRows);

    const std::string global_offset_label = format_offset_label(settings_.global_note_offset_ms);
    const settings::offset_stepper_layout offset_layout =
        settings::global_offset_stepper_layout(settings::kGameplayRows[5]);
    draw_settings_stepper_row(settings::kGameplayRows[5], text_key::global_offset,
                              global_offset_label.c_str(), offset_layout);
    preview_.draw(settings::kGameplayPreviewRect);
}

settings_audio_page::settings_audio_page(game_settings& settings)
    : settings_(settings) {
}

void settings_audio_page::reset_interaction() {
    ui::reset_indexed_drag(slider_drag_);
}

settings_page_update_result settings_audio_page::update() {
    settings_page_update_result result;
    append_slider_drag_change(result, kAudioSliders, settings::kGeneralRows, slider_drag_);
    append_bool_selector_change(result, settings_, kLoudnessSelector);
    return result;
}

void settings_audio_page::draw() const {
    draw_slider_rows(settings_, kAudioSliders, settings::kGeneralRows);
    draw_bool_selector_row(settings_, kLoudnessSelector);
}

settings_video_page::settings_video_page(game_settings& settings)
    : settings_(settings) {
}

void settings_video_page::reset_interaction() {
    ui::reset_indexed_drag(frame_rate_drag_);
}

settings_page_update_result settings_video_page::update() {
    settings_page_update_result result;
    const ui::indexed_drag_result frame_rate_drag =
        ui::update_indexed_drag(std::span<const Rectangle>(&settings::kGeneralRows[0], 1),
                                frame_rate_drag_,
                                settings::kLayer);
    if (frame_rate_drag.dragging) {
        const float ratio = settings::slider_ratio_from_mouse(settings::kGeneralRows[0]);
        const int index = static_cast<int>(std::round(ratio * static_cast<float>(kFrameRateOptions.size() - 1)));
        result.int_changes.push_back({
            &game_settings::target_fps,
            kFrameRateOptions[static_cast<std::size_t>(std::clamp(index, 0, static_cast<int>(kFrameRateOptions.size()) - 1))],
        });
    }

    return result;
}

void settings_video_page::draw() const {
    const std::string fps_label = std::to_string(sanitize_target_fps(settings_.target_fps));
    draw_settings_slider_row(settings::kGeneralRows[0], text_key::frame_rate, fps_label.c_str(),
                             static_cast<float>(settings::fps_option_index(settings_.target_fps)) / 3.0f);
}

settings_system_page::settings_system_page(game_settings& settings)
    : settings_(settings) {
}

void settings_system_page::reset_interaction() {
}

settings_page_update_result settings_system_page::update() {
    settings_page_update_result result;
    for (const system_selector_definition& selector : kSystemSelectors) {
        if (selector_clicked(settings::kGeneralRows[selector.row_index])) {
            apply_system_selector(result, settings_, selector.kind);
        }
    }
    return result;
}

void settings_system_page::draw() const {
    for (const system_selector_definition& selector : kSystemSelectors) {
        draw_settings_selector_row(settings::kGeneralRows[selector.row_index],
                                   selector.label,
                                   system_selector_value(settings_, selector.kind));
    }
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
