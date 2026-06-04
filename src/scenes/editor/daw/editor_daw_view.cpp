#include "editor/daw/editor_daw_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "editor/editor_timeline_types.h"
#include "editor/view/editor_layout.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_text_input.h"
#include "ui/icons/raythm_icons.h"
#include "ui_layout.h"
#include "ui_tooltip.h"
#include "virtual_screen.h"

namespace {
namespace layout = editor::layout;

constexpr float kPanelInset = 14.0f;
constexpr float kAutomationWidth = 380.0f;
constexpr std::size_t kAutomationSideGuideCount = 4;
std::array<ui::text_input_state, kAutomationSideGuideCount> gAutomationGuideInputs;

bool accepts_metadata_character(int codepoint, const std::string&) {
    return codepoint >= 32 && codepoint <= 126;
}

bool accepts_float_character(int codepoint, const std::string& value) {
    if (codepoint >= '0' && codepoint <= '9') {
        return true;
    }
    return codepoint == '.' && value.find('.') == std::string::npos;
}

bool accepts_int_character(int codepoint, const std::string&) {
    return codepoint >= '0' && codepoint <= '9';
}

const char* key_count_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

const char* palette_label(note_type type) {
    switch (type) {
        case note_type::tap:
            return "TAP";
        case note_type::hold:
            return "LONG";
        case note_type::release:
            return "RELEASE";
        case note_type::stay:
            return "STAY";
        case note_type::decorative_hold:
            return "DECO";
    }
    return "TAP";
}

Color palette_tone(note_type type) {
    const auto& t = *g_theme;
    switch (type) {
        case note_type::tap:
            return t.error;
        case note_type::hold:
            return t.success;
        case note_type::release:
            return t.slow;
        case note_type::stay:
            return t.fast;
        case note_type::decorative_hold:
            return t.accent;
    }
    return t.accent;
}

const char* timing_event_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Meter";
}

const char* scroll_curve_label(scroll_automation_curve curve) {
    switch (curve) {
        case scroll_automation_curve::hold:
            return "Hold";
        case scroll_automation_curve::linear:
            return "Linear";
        case scroll_automation_curve::ease_in:
            return "Ease In";
        case scroll_automation_curve::ease_out:
            return "Ease Out";
        case scroll_automation_curve::ease_in_out:
            return "Ease In/Out";
    }
    return "Hold";
}

scroll_automation_curve next_scroll_curve(scroll_automation_curve curve) {
    switch (curve) {
        case scroll_automation_curve::hold:
            return scroll_automation_curve::linear;
        case scroll_automation_curve::linear:
            return scroll_automation_curve::ease_in;
        case scroll_automation_curve::ease_in:
            return scroll_automation_curve::ease_out;
        case scroll_automation_curve::ease_out:
            return scroll_automation_curve::ease_in_out;
        case scroll_automation_curve::ease_in_out:
            return scroll_automation_curve::hold;
    }
    return scroll_automation_curve::hold;
}

Color panel_tint(Color base, Color tone, float amount) {
    return with_alpha(lerp_color(base, tone, amount), base.a);
}

Rectangle inset_rect(Rectangle rect, float value) {
    return ui::inset(rect, ui::edge_insets::uniform(value));
}

Rectangle row(Rectangle rect, float y, float height) {
    return {rect.x, rect.y + y, rect.width, height};
}

Rectangle centered_icon_rect(Rectangle rect, float inset) {
    return {rect.x + inset, rect.y + inset, rect.width - inset * 2.0f, rect.height - inset * 2.0f};
}

std::vector<float> automation_snap_candidates(const std::array<float, 5>& guides,
                                              std::optional<float> pinned_multiplier = std::nullopt) {
    std::vector<float> candidates(guides.begin(), guides.end());
    if (pinned_multiplier.has_value() &&
        std::isfinite(*pinned_multiplier) &&
        *pinned_multiplier >= 0.0f) {
        candidates.push_back(*pinned_multiplier);
    }
    return candidates;
}

float snap_automation_multiplier(const std::vector<float>& candidates, float multiplier, bool free_drag) {
    multiplier = std::max(0.0f, multiplier);
    if (free_drag) {
        return std::round(multiplier * 100.0f) / 100.0f;
    }
    if (candidates.empty()) {
        return multiplier;
    }
    float best = candidates[0];
    float best_distance = std::fabs(multiplier - best);
    for (const float candidate : candidates) {
        const float distance = std::fabs(multiplier - candidate);
        if (distance < best_distance) {
            best = candidate;
            best_distance = distance;
        }
    }
    return best;
}

void normalize_automation_guides(scroll_automation_guides& guides) {
    for (float& guide : guides.values) {
        if (!std::isfinite(guide)) {
            guide = 1.0f;
        }
        guide = std::max(0.0f, guide);
    }
}

void reset_automation_guide_input(ui::text_input_state& input, float value) {
    input.value = TextFormat("%.1f", value);
    input.cursor = input.value.size();
    input.has_selection = false;
}

bool is_unity_automation_guide(float value) {
    return std::fabs(value - 1.0f) < 0.0001f;
}

bool automation_guides_are_ordered(const scroll_automation_guides& guides) {
    return guides.values[0] <= guides.values[1] &&
           guides.values[1] <= 1.0f &&
           1.0f <= guides.values[2] &&
           guides.values[2] <= guides.values[3];
}

std::array<float, 5> automation_guides(scroll_automation_guides& guides) {
    normalize_automation_guides(guides);
    return {
        guides.values[0],
        guides.values[1],
        1.0f,
        guides.values[2],
        guides.values[3],
    };
}

float automation_guide_t(std::size_t guide_index) {
    return static_cast<float>(guide_index) / 4.0f;
}

float automation_multiplier_to_t(const std::array<float, 5>& guides, float multiplier) {
    for (std::size_t index = 0; index + 1 < guides.size(); ++index) {
        const float from = guides[index];
        const float to = guides[index + 1];
        const float low = std::min(from, to);
        const float high = std::max(from, to);
        if (multiplier < low || multiplier > high) {
            continue;
        }
        if (std::fabs(to - from) < 0.0001f) {
            return automation_guide_t(index);
        }
        const float segment_t = std::clamp((multiplier - from) / (to - from), 0.0f, 1.0f);
        return (static_cast<float>(index) + segment_t) / 4.0f;
    }

    std::size_t nearest_index = 0;
    float nearest_distance = std::fabs(multiplier - guides[0]);
    for (std::size_t index = 1; index < guides.size(); ++index) {
        const float distance = std::fabs(multiplier - guides[index]);
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest_index = index;
        }
    }
    return automation_guide_t(nearest_index);
}

float automation_multiplier_at_t(const std::array<float, 5>& guides, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float scaled = t * 4.0f;
    const std::size_t index = std::min<std::size_t>(3, static_cast<std::size_t>(std::floor(scaled)));
    const float segment_t = std::clamp(scaled - static_cast<float>(index), 0.0f, 1.0f);
    return guides[index] + (guides[index + 1] - guides[index]) * segment_t;
}

bool draw_automation_guide_input(std::size_t index, Rectangle rect, scroll_automation_guides& guides) {
    if (index >= guides.values.size()) {
        return false;
    }
    normalize_automation_guides(guides);
    ui::text_input_state& input = gAutomationGuideInputs[index];
    const bool hovered = CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), rect);
    if (!input.active) {
        reset_automation_guide_input(input, guides.values[index]);
    }
    const ui::text_input_result result = ui::draw_text_input(
        rect, input, "", "x", nullptr, ui::draw_layer::base, 12, 8,
        accepts_float_character, 0.0f, false, true, true);
    if (result.submitted || result.deactivated) {
        const scroll_automation_guides previous_guides = guides;
        try {
            const float parsed_value = std::max(0.0f, std::stof(input.value));
            if (!is_unity_automation_guide(parsed_value)) {
                guides.values[index] = parsed_value;
            }
        } catch (...) {
        }
        normalize_automation_guides(guides);
        if (is_unity_automation_guide(guides.values[index]) ||
            !automation_guides_are_ordered(guides)) {
            guides = previous_guides;
        }
        reset_automation_guide_input(input, guides.values[index]);
    }
    return hovered || input.active || result.clicked;
}

Vector2 rect_center(Rectangle rect) {
    return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
}

ui::button_state draw_icon_button(Rectangle rect,
                                  void (*draw_icon)(Rectangle, Color, float),
                                  bool active,
                                  Color active_color) {
    const auto& t = *g_theme;
    const ui::button_state button = ui::draw_button_colored(
        rect, "", 16,
        active ? panel_tint(t.row_selected, active_color, 0.14f) : t.row,
        active ? panel_tint(t.row_active, active_color, 0.18f) : t.row_hover,
        active ? t.text : t.text_secondary,
        active ? 2.2f : 1.2f);
    draw_icon(centered_icon_rect(rect, 9.0f), active ? active_color : t.text_secondary, 3.2f);
    return button;
}

void draw_micro_label(Rectangle rect, const char* label, Color color) {
    ui::draw_text_in_rect(label, 12, rect, color, ui::text_align::left);
}

void draw_badge(Rectangle rect, const char* label, Color border, Color text) {
    ui::draw_rect_f(rect, with_alpha(border, 28));
    ui::draw_rect_lines(rect, 1.0f, with_alpha(border, 190));
    ui::draw_text_in_rect(label, 13, rect, text);
}

ui::button_state draw_layer_button(Rectangle rect,
                                   const char* label,
                                   int font_size,
                                   ui::draw_layer layer,
                                   Color bg,
                                   Color bg_hover,
                                   Color text_color,
                                   float border_width = 1.5f) {
    const bool hovered = ui::is_hovered(rect, layer);
    const bool pressed = ui::is_pressed(rect, layer);
    const bool clicked = ui::is_clicked(rect, layer);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    ui::draw_rect_f(visual, lerp_color(bg, bg_hover, hovered ? 1.0f : 0.0f));
    ui::draw_rect_lines(visual, border_width, g_theme->border_light);
    ui::draw_text_in_rect(label, font_size, visual, text_color);
    return {hovered, pressed, clicked};
}

ui::row_state draw_layer_row(Rectangle rect,
                             bool selected,
                             ui::draw_layer layer,
                             Color selected_tone) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(rect, layer);
    const bool pressed = ui::is_pressed(rect, layer);
    const bool clicked = ui::is_clicked(rect, layer);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    ui::draw_rect_f(visual, selected
        ? panel_tint(t.row_selected, selected_tone, 0.16f)
        : lerp_color(t.row, t.row_hover, hovered ? 1.0f : 0.0f));
    ui::draw_rect_lines(visual, selected ? 2.0f : 1.0f, selected ? selected_tone : t.border_light);
    return {hovered, pressed, clicked, visual};
}

void set_active_timing_input(editor_timing_panel_state& state, editor_timing_input_field field) {
    state.active_input_field = field;
    state.inputs.bpm_value.active = field == editor_timing_input_field::bpm_value;
    state.inputs.meter_numerator.active = field == editor_timing_input_field::meter_numerator;
    state.inputs.meter_denominator.active = field == editor_timing_input_field::meter_denominator;
    state.inputs.scroll_duration.active = field == editor_timing_input_field::scroll_duration;
    state.inputs.scroll_multiplier.active = field == editor_timing_input_field::scroll_multiplier;
    state.inputs.bpm_bar.active = false;
    state.inputs.meter_bar.active = false;
    state.inputs.scroll_start_bar.active = false;
}

void draw_palette_icon(Rectangle rect, note_type type, Color color) {
    switch (type) {
        case note_type::tap:
            raythm_icons::draw_note_tap(rect, color, 3.0f);
            break;
        case note_type::hold:
            raythm_icons::draw_note_long(rect, color, 3.2f);
            break;
        case note_type::release:
            raythm_icons::draw_note_release(rect, color, 3.0f);
            break;
        case note_type::stay:
            raythm_icons::draw_note_stay(rect, color, 3.0f);
            break;
        case note_type::decorative_hold:
            raythm_icons::draw_note_long(rect, color, 2.6f);
            break;
    }
}

void draw_palette_pad(Rectangle rect,
                      note_type type,
                      const editor_note_palette_selection& selection,
                      editor_left_panel_view_result& result) {
    const auto& t = *g_theme;
    const bool selected = selection.type == type;
    const Color tone = palette_tone(type);
    const ui::row_state state = ui::draw_row(
        rect,
        selected ? panel_tint(t.row_selected, tone, 0.18f) : t.row,
        selected ? panel_tint(t.row_active, tone, 0.2f) : t.row_hover,
        selected ? tone : t.border_light,
        selected ? 2.0f : 1.0f);
    const Rectangle icon_rect = {state.visual.x + 10.0f, state.visual.y + 7.0f, 36.0f, state.visual.height - 14.0f};
    draw_palette_icon(icon_rect, type, selected ? tone : t.text_secondary);
    ui::draw_text_in_rect(palette_label(type), 15,
                          {state.visual.x + 58.0f, state.visual.y,
                           state.visual.width - 72.0f, state.visual.height},
                          selected ? t.text : t.text_secondary, ui::text_align::left);
    if (state.clicked) {
        result.selected_note_type = type;
    }
}

bool draw_ray_toggle(Rectangle rect, bool enabled) {
    const auto& t = *g_theme;
    const Color ray_tone = {176, 112, 255, 255};
    const ui::row_state state = ui::draw_row(
        rect,
        enabled ? panel_tint(t.row_selected, ray_tone, 0.2f) : t.row,
        enabled ? panel_tint(t.row_active, ray_tone, 0.24f) : t.row_hover,
        enabled ? ray_tone : t.border_light,
        enabled ? 2.0f : 1.0f);
    const Rectangle label_rect = {state.visual.x + 12.0f, state.visual.y, state.visual.width - 96.0f,
                                  state.visual.height};
    ui::draw_text_in_rect("Ray", 15, label_rect, enabled ? t.text : t.text_secondary, ui::text_align::left);
    const Rectangle track = {state.visual.x + state.visual.width - 78.0f,
                             state.visual.y + state.visual.height * 0.5f - 10.0f,
                             54.0f, 20.0f};
    ui::draw_rect_f(track, enabled ? with_alpha(ray_tone, 165) : with_alpha(t.text_muted, 70));
    ui::draw_rect_lines(track, 1.0f, enabled ? ray_tone : t.border_light);
    const float knob_x = enabled ? track.x + track.width - 18.0f : track.x + 2.0f;
    ui::draw_rect_f({knob_x, track.y + 2.0f, 16.0f, 16.0f}, enabled ? t.text : t.text_secondary);
    ui::draw_text_in_rect(enabled ? "ON" : "OFF", 11,
                          {track.x - 32.0f, track.y, 28.0f, track.height},
                          enabled ? ray_tone : t.text_muted, ui::text_align::right);
    return state.clicked;
}

editor_timeline_note make_timeline_note(const note_data& note, size_t source_index) {
    editor_timeline_note_type type = editor_timeline_note_type::tap;
    switch (note.type) {
        case note_type::tap:
            type = editor_timeline_note_type::tap;
            break;
        case note_type::hold:
            type = editor_timeline_note_type::hold;
            break;
        case note_type::release:
            type = editor_timeline_note_type::release;
            break;
        case note_type::stay:
            type = editor_timeline_note_type::stay;
            break;
        case note_type::decorative_hold:
            type = editor_timeline_note_type::decorative_hold;
            break;
    }
    return {type, note.tick, note.lane, note.end_tick, note.is_ray, note_lane_width(note), source_index};
}

bool note_intersects_tick_range(const note_data& note, int min_tick, int max_tick) {
    const int start_tick = note.tick;
    const int end_tick = note_has_duration(note) ? std::max(note.tick, note.end_tick) : note.tick;
    return end_tick >= min_tick && start_tick <= max_tick;
}

struct timeline_note_cache {
    const editor_state* state = nullptr;
    size_t generation = static_cast<size_t>(-1);
    std::vector<editor_timeline_note> notes;
};

const std::vector<editor_timeline_note>* cached_minimap_notes(const editor_state& state) {
    static timeline_note_cache cache;
    const size_t generation = state.revision_generation();
    if (cache.state != &state || cache.generation != generation) {
        cache.state = &state;
        cache.generation = generation;
        cache.notes.clear();
        cache.notes.reserve(state.data().notes.size());
        for (size_t index = 0; index < state.data().notes.size(); ++index) {
            cache.notes.push_back(make_timeline_note(state.data().notes[index], index));
        }
    }
    return &cache.notes;
}

editor_timeline_view_model make_timeline_model(const editor_timeline_presenter_model& model) {
    const editor_timeline_metrics metrics = editor_timeline_viewport::metrics(model.viewport);
    const float visible_tick_span = editor_timeline_viewport::visible_tick_span(model.viewport);
    const int min_tick = static_cast<int>(std::floor(model.viewport.viewport.bottom_tick - visible_tick_span * 0.1f));
    const int max_tick = static_cast<int>(std::ceil(model.viewport.viewport.bottom_tick + visible_tick_span));

    std::vector<editor_timeline_note> notes;
    const std::vector<size_t> visible_note_indices =
        model.state.note_indices_in_tick_range(min_tick, max_tick);
    notes.reserve(visible_note_indices.size());
    for (const size_t index : visible_note_indices) {
        const note_data& note = model.state.data().notes[index];
        if (note_intersects_tick_range(note, min_tick, max_tick)) {
            notes.push_back(make_timeline_note(note, index));
        }
    }

    std::vector<editor_timeline_scroll_automation_point> scroll_automation;
    scroll_automation.reserve(model.state.data().scroll_automation.size());
    for (const scroll_automation_point& point : model.state.data().scroll_automation) {
        scroll_automation.push_back({point.tick, point.multiplier, point.curve_to_next});
    }

    std::vector<editor_timeline_note> preview_notes;
    preview_notes.reserve(model.preview_notes.size());
    for (size_t index = 0; index < model.preview_notes.size(); ++index) {
        preview_notes.push_back(make_timeline_note(model.preview_notes[index], index));
    }

    return {
        metrics,
        model.meter_map.visible_grid_lines(min_tick, max_tick),
        std::move(scroll_automation),
        std::move(notes),
        cached_minimap_notes(model.state),
        model.state.revision_generation(),
        model.selected_note_indices,
        model.selected_scroll_event_index,
        model.audio_loaded ? std::optional<int>(model.playback_tick) : std::nullopt,
        model.waveform_summary,
        &model.state.engine(),
        model.waveform_visible,
        model.waveform_offset_ms,
        std::move(preview_notes),
        model.preview_note_indices,
        model.preview_has_overlap,
        model.selection_rect,
        min_tick,
        max_tick,
        editor_timeline_viewport::snap_interval(model.viewport),
        editor_timeline_viewport::content_height_pixels(model.viewport),
        editor_timeline_viewport::scroll_offset_pixels(model.viewport)
    };
}

void draw_waveform(const editor_timeline_view_model& model, Rectangle content) {
    const auto& t = *g_theme;
    if (!model.waveform_visible || model.waveform_summary == nullptr || model.timing_engine == nullptr) {
        return;
    }

    struct waveform_row_cache {
        const audio_waveform_summary* summary = nullptr;
        int offset_ms = 0;
        int min_tick = 0;
        int max_tick = 0;
        float bottom_tick = 0.0f;
        float ticks_per_pixel = 0.0f;
        int row_count = 0;
        float content_y = 0.0f;
        std::vector<float> rows;
    };
    static waveform_row_cache cache;
    const int row_count = std::max(1, static_cast<int>(std::ceil(content.height)));
    if (cache.summary != model.waveform_summary ||
        cache.offset_ms != model.waveform_offset_ms ||
        cache.min_tick != model.min_tick ||
        cache.max_tick != model.max_tick ||
        std::fabs(cache.bottom_tick - model.metrics.bottom_tick) > 0.01f ||
        std::fabs(cache.ticks_per_pixel - model.metrics.ticks_per_pixel) > 0.001f ||
        cache.row_count != row_count ||
        std::fabs(cache.content_y - content.y) > 0.01f) {
        cache.summary = model.waveform_summary;
        cache.offset_ms = model.waveform_offset_ms;
        cache.min_tick = model.min_tick;
        cache.max_tick = model.max_tick;
        cache.bottom_tick = model.metrics.bottom_tick;
        cache.ticks_per_pixel = model.metrics.ticks_per_pixel;
        cache.row_count = row_count;
        cache.content_y = content.y;
        cache.rows.assign(static_cast<size_t>(row_count), 0.0f);
        for (const audio_waveform_peak& peak : model.waveform_summary->peaks) {
            const double shifted_ms = peak.seconds * 1000.0 + static_cast<double>(model.waveform_offset_ms);
            const int tick = model.timing_engine->ms_to_tick(shifted_ms);
            if (tick < model.min_tick || tick > model.max_tick) {
                continue;
            }
            const int row_index = static_cast<int>(std::floor(model.metrics.tick_to_y(tick) - content.y));
            if (row_index >= 0 && row_index < row_count) {
                cache.rows[static_cast<size_t>(row_index)] =
                    std::max(cache.rows[static_cast<size_t>(row_index)], std::clamp(peak.amplitude, 0.0f, 1.0f));
            }
        }
    }

    const float base_x = content.x + content.width - 4.0f;
    const float max_width = std::max(1.0f, content.width - 10.0f);
    for (int i = 0; i < row_count; ++i) {
        const float amplitude = cache.rows[static_cast<size_t>(i)];
        if (amplitude <= 0.001f) {
            continue;
        }
        const float y = content.y + static_cast<float>(i);
        const float width = max_width * amplitude;
        ui::draw_line_f(base_x - width, y, base_x, y, with_alpha(t.accent, 70));
    }
}

Color editor_play_note_color(editor_timeline_note_type type, bool is_ray, Color base) {
    if (is_ray) {
        switch (type) {
            case editor_timeline_note_type::hold:
                return lerp_color(base, {142, 92, 236, 255}, 0.72f);
            case editor_timeline_note_type::decorative_hold:
                return lerp_color(base, {172, 132, 255, 255}, 0.58f);
            case editor_timeline_note_type::release:
                return lerp_color(base, {198, 116, 255, 255}, 0.76f);
            case editor_timeline_note_type::stay:
                return lerp_color(base, WHITE, 0.90f);
            case editor_timeline_note_type::tap:
                return lerp_color(base, {180, 132, 255, 255}, 0.68f);
        }
    }
    switch (type) {
        case editor_timeline_note_type::release:
            return lerp_color(base, {255, 105, 148, 255}, 0.42f);
        case editor_timeline_note_type::stay:
            return lerp_color(base, g_theme->judge_perfect, 0.25f);
        case editor_timeline_note_type::tap:
            return lerp_color(base, WHITE, 0.42f);
        case editor_timeline_note_type::hold:
            return lerp_color(base, WHITE, 0.96f);
        case editor_timeline_note_type::decorative_hold:
            return lerp_color(base, {86, 220, 232, 255}, 0.55f);
    }
    return base;
}

Color editor_tap_gradient_color(Color base, float t) {
    const float edge_factor = std::pow(std::fabs(t - 0.5f) * 2.0f, 1.25f);
    const float highlight = 0.26f + (1.0f - edge_factor) * 0.26f;
    const unsigned char alpha = static_cast<unsigned char>(166.0f + edge_factor * 42.0f);
    return with_alpha(lerp_color(base, WHITE, highlight), alpha);
}

Color editor_hold_gradient_color(Color base, float t) {
    const float edge_factor = std::pow(std::fabs(t - 0.5f) * 2.0f, 1.55f);
    const unsigned char alpha = static_cast<unsigned char>(84.0f + edge_factor * 110.0f);
    return with_alpha(lerp_color(base, WHITE, edge_factor * 0.10f), alpha);
}

Color editor_decorative_hold_gradient_color(Color base, float t) {
    const float center_factor = 1.0f - std::pow(std::fabs(t - 0.5f) * 2.0f, 1.35f);
    const unsigned char alpha = static_cast<unsigned char>(48.0f + center_factor * 86.0f);
    return with_alpha(lerp_color(base, WHITE, 0.14f + center_factor * 0.20f), alpha);
}

Color editor_stay_gradient_color(Color base, float t) {
    const float center_factor = 1.0f - std::pow(std::fabs(t - 0.5f) * 2.0f, 1.35f);
    const float highlight = 0.16f + center_factor * 0.52f;
    const unsigned char alpha = static_cast<unsigned char>(208.0f + center_factor * 24.0f);
    return with_alpha(lerp_color(base, WHITE, highlight), alpha);
}

void draw_horizontal_strip_gradient(Rectangle rect, int steps, Color (*color_at)(Color, float), Color base) {
    const float step_width = rect.width / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(steps);
        const Rectangle strip = {
            rect.x + step_width * static_cast<float>(i),
            rect.y,
            i == steps - 1 ? rect.width - step_width * static_cast<float>(i) : step_width + 0.75f,
            rect.height
        };
        ui::draw_rect_f(strip, color_at(base, t));
    }
}

void draw_editor_tap_slab(Rectangle rect, Color fill, bool release_style, bool ray_style, bool selected) {
    const Color tap_base = ray_style
                               ? lerp_color(fill, {180, 132, 255, 255}, 0.72f)
                               : release_style
                               ? lerp_color(fill, {255, 118, 156, 255}, 0.38f)
                               : lerp_color(fill, WHITE, 0.56f);
    const Color edge = lerp_color(tap_base, WHITE, 0.30f);
    const float rim_height = std::max(2.0f, rect.height * 0.16f);
    const float side_width = std::max(2.0f, std::min(rect.width * 0.055f, rect.height * 0.18f));
    const Color frame_left = with_alpha(lerp_color(edge, WHITE, 0.12f), 232);
    const Color frame_right = with_alpha(lerp_color(edge, BLACK, 0.08f), 218);
    const Color frame_near = with_alpha(lerp_color(edge, WHITE, 0.18f), 238);
    const Color frame_far = with_alpha(lerp_color(edge, BLACK, 0.12f), 210);

    draw_horizontal_strip_gradient(rect, 16, editor_tap_gradient_color, tap_base);
    DrawRectangleGradientH(static_cast<int>(rect.x), static_cast<int>(rect.y),
                           static_cast<int>(rect.width), static_cast<int>(rim_height),
                           frame_left, frame_right);
    DrawRectangleGradientH(static_cast<int>(rect.x), static_cast<int>(rect.y + rect.height - rim_height),
                           static_cast<int>(rect.width), static_cast<int>(rim_height),
                           frame_left, frame_right);
    DrawRectangleGradientV(static_cast<int>(rect.x), static_cast<int>(rect.y),
                           static_cast<int>(side_width), static_cast<int>(rect.height),
                           frame_near, frame_far);
    DrawRectangleGradientV(static_cast<int>(rect.x + rect.width - side_width), static_cast<int>(rect.y),
                           static_cast<int>(side_width), static_cast<int>(rect.height),
                           frame_near, frame_far);
    if (selected) {
        ui::draw_rect_lines(ui::inset(rect, -2.0f), 2.0f, g_theme->accent);
    }
}

void draw_editor_hold_body(Rectangle rect, Color fill, bool ray_style, bool selected) {
    const Color hold_base = ray_style
                                ? lerp_color(fill, g_theme->accent, 0.68f)
                                : lerp_color(fill, WHITE, 0.94f);
    const Color edge = lerp_color(hold_base, WHITE, 0.24f);
    const float cap_height = std::min(18.0f, std::max(4.0f, rect.height * 0.16f));
    const float cap_overhang = std::min(rect.width * 0.035f, rect.width * 0.015f);
    const Rectangle body = {
        rect.x + cap_overhang,
        rect.y,
        std::max(1.0f, rect.width - cap_overhang * 2.0f),
        rect.height
    };
    const Color cap_left = with_alpha(lerp_color(edge, WHITE, 0.16f), 230);
    const Color cap_right = with_alpha(lerp_color(edge, BLACK, 0.10f), 214);

    draw_horizontal_strip_gradient(body, 24, editor_hold_gradient_color, hold_base);
    DrawRectangleGradientH(static_cast<int>(rect.x), static_cast<int>(rect.y),
                           static_cast<int>(rect.width), static_cast<int>(cap_height),
                           cap_left, cap_right);
    DrawRectangleGradientH(static_cast<int>(rect.x), static_cast<int>(rect.y + rect.height - cap_height),
                           static_cast<int>(rect.width), static_cast<int>(cap_height),
                           cap_left, cap_right);
    if (selected) {
        ui::draw_rect_lines(ui::inset(rect, -2.0f), 2.0f, g_theme->accent);
    }
}

void draw_editor_decorative_hold_body(Rectangle rect, Color fill, bool ray_style, bool selected) {
    const Color decor_base = ray_style
                                 ? lerp_color(fill, {194, 156, 255, 255}, 0.62f)
                                 : lerp_color(fill, {86, 220, 232, 255}, 0.64f);
    draw_horizontal_strip_gradient(rect, 18, editor_decorative_hold_gradient_color, decor_base);

    const Color rail = with_alpha(lerp_color(decor_base, WHITE, 0.30f), 150);
    const float rail_width = std::clamp(rect.width * 0.035f, 1.5f, 4.0f);
    DrawRectangleGradientV(static_cast<int>(rect.x), static_cast<int>(rect.y),
                           static_cast<int>(rail_width), static_cast<int>(rect.height),
                           rail, with_alpha(rail, 72));
    DrawRectangleGradientV(static_cast<int>(rect.x + rect.width - rail_width), static_cast<int>(rect.y),
                           static_cast<int>(rail_width), static_cast<int>(rect.height),
                           rail, with_alpha(rail, 72));
    if (selected) {
        ui::draw_rect_lines(ui::inset(rect, -2.0f), 2.0f, g_theme->accent);
    }
}

void draw_editor_stay_dot(Rectangle rect, Color fill, bool ray_style, bool selected) {
    const Color stay_base = ray_style
                                ? lerp_color(WHITE, {224, 214, 255, 255}, 0.14f)
                                : lerp_color({70, 236, 224, 255}, fill, 0.14f);
    const Color end_edge = with_alpha(lerp_color(stay_base, WHITE, 0.34f), 226);
    const Color end_inner = with_alpha(lerp_color(stay_base, WHITE, 0.12f), 178);
    const Vector2 center = rect_center(rect);
    const Rectangle bar = rect;
    const float cap_width = std::clamp(bar.width * 0.055f, 2.0f, 5.0f);
    const float cap_height = bar.height * 1.55f;
    const Rectangle left_cap = {bar.x, center.y - cap_height * 0.5f, cap_width, cap_height};
    const Rectangle right_cap = {bar.x + bar.width - cap_width, center.y - cap_height * 0.5f,
                                 cap_width, cap_height};

    draw_horizontal_strip_gradient(bar, 18, editor_stay_gradient_color, stay_base);
    DrawRectangleGradientV(static_cast<int>(left_cap.x), static_cast<int>(left_cap.y),
                           static_cast<int>(left_cap.width), static_cast<int>(left_cap.height),
                           end_inner, end_edge);
    DrawRectangleGradientV(static_cast<int>(right_cap.x), static_cast<int>(right_cap.y),
                           static_cast<int>(right_cap.width), static_cast<int>(right_cap.height),
                           end_inner, end_edge);
    if (selected) {
        ui::draw_rect_lines(ui::inset(bar, -2.0f), 2.0f, g_theme->accent);
    }
}

void draw_editor_release_chevron(Rectangle note_rect, Color marker, Color contour) {
    const float width = note_rect.width * 0.88f;
    const float height = std::max(9.0f, note_rect.height * 0.78f);
    const float lift = note_rect.height * 0.34f + 2.0f;
    const Vector2 center = {note_rect.x + note_rect.width * 0.5f,
                            note_rect.y + note_rect.height * 0.5f - lift - height * 0.5f};
    const Vector2 left_outer_bottom = {center.x - width * 0.50f, center.y + height * 0.26f};
    const Vector2 left_outer_top = {center.x - width * 0.50f, center.y + height * 0.02f};
    const Vector2 center_top = {center.x, center.y - height * 0.36f};
    const Vector2 center_bottom = {center.x, center.y - height * 0.08f};
    const Vector2 right_outer_top = {center.x + width * 0.50f, center.y + height * 0.02f};
    const Vector2 right_outer_bottom = {center.x + width * 0.50f, center.y + height * 0.26f};

    DrawTriangle(left_outer_bottom, left_outer_top, center_top, marker);
    DrawTriangle(left_outer_bottom, center_top, center_bottom, marker);
    DrawTriangle(center_bottom, center_top, right_outer_top, marker);
    DrawTriangle(center_bottom, right_outer_top, right_outer_bottom, marker);

    const float line_width = std::clamp(height * 0.10f, 1.6f, 2.8f);
    DrawLineEx(left_outer_bottom, left_outer_top, line_width, contour);
    DrawLineEx(left_outer_top, center_top, line_width, contour);
    DrawLineEx(center_top, right_outer_top, line_width, contour);
    DrawLineEx(right_outer_top, right_outer_bottom, line_width, contour);
    DrawLineEx(right_outer_bottom, center_bottom, line_width, contour);
    DrawLineEx(center_bottom, left_outer_bottom, line_width, contour);
}

void draw_simple_note_block(const editor_timeline_note& note,
                            const editor_timeline_note_geometry& geometry,
                            bool selected,
                            bool preview,
                            bool overlap) {
    const auto& t = *g_theme;
    const Color fill = overlap ? t.error : editor_play_note_color(note.type, note.is_ray, t.note_color);
    const Color color = preview ? with_alpha(fill, 160) : with_alpha(fill, note.is_ray ? 235 : 205);
    const Rectangle rect = geometry.visual.has_body ? geometry.visual.body_rect : geometry.visual.head_rect;
    ui::draw_rect_f(rect, color);
    if (selected || preview) {
        ui::draw_rect_lines(ui::inset(rect, selected ? -1.5f : 0.0f), selected ? 2.0f : 1.0f,
                            overlap ? t.error : (selected ? t.accent : t.success));
    }
}

void draw_note_block(const editor_timeline_note& note,
                     const editor_timeline_note_geometry& geometry,
                     bool selected,
                     bool preview,
                     bool overlap,
                     bool simplified = false) {
    if (simplified && !selected) {
        draw_simple_note_block(note, geometry, selected, preview, overlap);
        return;
    }

    const auto& t = *g_theme;
    const Color fill = overlap ? t.error : editor_play_note_color(note.type, note.is_ray, t.note_color);
    const Color draw_fill = preview ? with_alpha(fill, 170) : fill;

    if (geometry.visual.has_body) {
        if (note.type == editor_timeline_note_type::decorative_hold) {
            draw_editor_decorative_hold_body(geometry.visual.visual_body_rect, draw_fill, note.is_ray, selected);
            return;
        }
        draw_editor_hold_body(geometry.visual.body_rect, draw_fill, note.is_ray, selected);
        return;
    }

    if (note.type == editor_timeline_note_type::stay) {
        draw_editor_stay_dot(geometry.visual.head_rect, draw_fill, note.is_ray, selected);
        return;
    }

    draw_editor_tap_slab(geometry.visual.head_rect, draw_fill, note.type == editor_timeline_note_type::release,
                         note.is_ray, selected);
    if (note.type == editor_timeline_note_type::release) {
        const Color release_seed = note.is_ray ? Color{190, 112, 255, 255} : Color{255, 90, 132, 255};
        const Color release_base = lerp_color(release_seed, draw_fill, note.is_ray ? 0.24f : 0.16f);
        const Color marker = with_alpha(lerp_color(release_base, WHITE, 0.28f), 255);
        const Color contour = with_alpha(lerp_color(release_base, BLACK, 0.16f), 255);
        draw_editor_release_chevron(geometry.visual.head_rect, marker, contour);
    }
}

float minimap_y_for_tick(const editor_timeline_view_model& model, Rectangle minimap, float tick) {
    const float full_tick_span = std::max(1.0f, model.content_height_pixels * model.metrics.ticks_per_pixel);
    const float max_bottom_tick = model.metrics.bottom_tick +
        model.scroll_offset_pixels * model.metrics.ticks_per_pixel;
    const float min_bottom_tick = max_bottom_tick -
        std::max(0.0f, model.content_height_pixels - model.metrics.content_rect().height) *
            model.metrics.ticks_per_pixel;
    const float ratio = std::clamp((tick - min_bottom_tick) / full_tick_span, 0.0f, 1.0f);
    return minimap.y + minimap.height - ratio * minimap.height;
}

struct minimap_shape_cache {
    const std::vector<editor_timeline_note>* notes = nullptr;
    size_t generation = static_cast<size_t>(-1);
    int key_count = 0;
    int width = 0;
    int height = 0;
    float x = 0.0f;
    float y = 0.0f;
    float full_tick_span = 0.0f;
    float min_bottom_tick = 0.0f;
    float max_bottom_tick = 0.0f;
    Color note_color = {};
    std::vector<std::pair<Rectangle, Color>> bodies;
    std::vector<std::pair<Rectangle, Color>> markers;
};

float minimap_y_for_cached_tick(const minimap_shape_cache& cache, float tick) {
    const float ratio = std::clamp((tick - cache.min_bottom_tick) / std::max(1.0f, cache.full_tick_span), 0.0f, 1.0f);
    return cache.y + static_cast<float>(cache.height) - ratio * static_cast<float>(cache.height);
}

minimap_shape_cache& cached_minimap_shapes(const editor_timeline_view_model& model,
                                           Rectangle inner,
                                           const ui_theme& t) {
    static minimap_shape_cache cache;
    const int width = std::max(1, static_cast<int>(std::ceil(inner.width)));
    const int height = std::max(1, static_cast<int>(std::ceil(inner.height)));
    const float full_tick_span = std::max(1.0f, model.content_height_pixels * model.metrics.ticks_per_pixel);
    const float max_bottom_tick = model.metrics.bottom_tick +
        model.scroll_offset_pixels * model.metrics.ticks_per_pixel;
    const float min_bottom_tick = max_bottom_tick -
        std::max(0.0f, model.content_height_pixels - model.metrics.content_rect().height) *
            model.metrics.ticks_per_pixel;

    if (cache.notes == model.minimap_notes &&
        cache.generation == model.minimap_generation &&
        cache.key_count == model.metrics.key_count &&
        cache.width == width &&
        cache.height == height &&
        std::fabs(cache.x - inner.x) < 0.01f &&
        std::fabs(cache.y - inner.y) < 0.01f &&
        std::fabs(cache.full_tick_span - full_tick_span) < 0.01f &&
        std::fabs(cache.min_bottom_tick - min_bottom_tick) < 0.01f &&
        std::fabs(cache.max_bottom_tick - max_bottom_tick) < 0.01f &&
        cache.note_color.r == t.note_color.r &&
        cache.note_color.g == t.note_color.g &&
        cache.note_color.b == t.note_color.b &&
        cache.note_color.a == t.note_color.a) {
        return cache;
    }

    cache.notes = model.minimap_notes;
    cache.generation = model.minimap_generation;
    cache.key_count = model.metrics.key_count;
    cache.width = width;
    cache.height = height;
    cache.x = inner.x;
    cache.y = inner.y;
    cache.full_tick_span = full_tick_span;
    cache.min_bottom_tick = min_bottom_tick;
    cache.max_bottom_tick = max_bottom_tick;
    cache.note_color = t.note_color;
    cache.bodies.clear();
    cache.markers.clear();

    const std::vector<editor_timeline_note> empty_notes;
    const std::vector<editor_timeline_note>& minimap_notes =
        model.minimap_notes != nullptr ? *model.minimap_notes : empty_notes;
    std::vector<unsigned char> marker_occupancy(static_cast<size_t>(width * height), 0);
    const float lane_width = inner.width / static_cast<float>(std::max(1, model.metrics.key_count));
    cache.bodies.reserve(std::min<std::size_t>(minimap_notes.size(), 4096));
    cache.markers.reserve(static_cast<size_t>(width * height / 4));
    for (const editor_timeline_note& note : minimap_notes) {
        if (note.lane < 0 || note.lane >= model.metrics.key_count) {
            continue;
        }
        const float y = minimap_y_for_cached_tick(cache, static_cast<float>(note.tick));
        const float x = inner.x + lane_width * static_cast<float>(note.lane);
        const float note_width = lane_width * static_cast<float>(std::max(1, note.lane_width));
        const Color color = editor_play_note_color(note.type, note.is_ray, t.note_color);
        if ((note.type == editor_timeline_note_type::hold ||
             note.type == editor_timeline_note_type::decorative_hold) &&
            note.end_tick > note.tick) {
            const float end_y = minimap_y_for_cached_tick(cache, static_cast<float>(note.end_tick));
            cache.bodies.push_back({
                {x + note_width * 0.35f, std::min(y, end_y), std::max(2.0f, note_width * 0.3f),
                 std::max(2.0f, std::fabs(end_y - y))},
                note.type == editor_timeline_note_type::decorative_hold
                    ? with_alpha(color, note.is_ray ? 120 : 82)
                    : with_alpha(color, note.is_ray ? 170 : 125)
            });
        }

        const int occupancy_x = std::clamp(static_cast<int>(std::floor(x + note_width * 0.5f - inner.x)), 0, width - 1);
        const int occupancy_y = std::clamp(static_cast<int>(std::floor(y - inner.y)), 0, height - 1);
        unsigned char& occupied = marker_occupancy[static_cast<size_t>(occupancy_y * width + occupancy_x)];
        if (occupied != 0) {
            continue;
        }
        occupied = 1;
        cache.markers.push_back({
            {x + 1.0f, y - 1.5f, std::max(2.0f, note_width - 2.0f), 3.0f},
            with_alpha(color, note.is_ray ? 235 : 185)
        });
    }
    return cache;
}

void draw_chart_minimap(const editor_timeline_view_model& model, Rectangle minimap) {
    const auto& t = *g_theme;
    ui::draw_rect_f(minimap, with_alpha(t.section, 235));
    ui::draw_rect_lines(minimap, 1.0f, t.border_light);

    const Rectangle inner = ui::inset(minimap, 4.0f);
    {
        ui::scoped_clip_rect clip_scope(inner);
        const minimap_shape_cache& cache = cached_minimap_shapes(model, inner, t);
        for (const auto& body : cache.bodies) {
            ui::draw_rect_f(body.first, body.second);
        }
        for (const auto& marker : cache.markers) {
            ui::draw_rect_f(marker.first, marker.second);
        }

    }

    const float visible_start_y = minimap_y_for_tick(model, inner, model.metrics.bottom_tick);
    const float visible_end_y = minimap_y_for_tick(
        model,
        inner,
        model.metrics.bottom_tick + model.metrics.visible_tick_span());
    const float box_top = std::clamp(std::min(visible_start_y, visible_end_y), inner.y, inner.y + inner.height);
    const float box_bottom = std::clamp(std::max(visible_start_y, visible_end_y), inner.y, inner.y + inner.height);
    const Rectangle clipped_box = {
        inner.x,
        box_top,
        inner.width,
        std::max(1.0f, box_bottom - box_top)
    };
    ui::draw_rect_f(clipped_box, with_alpha(t.accent, 36));
    ui::draw_rect_lines(clipped_box, 2.0f, with_alpha(t.accent, 220));
}

}  // namespace

namespace editor::daw {

editor_left_panel_view_result draw_left_panel(const editor_left_panel_view_model& model) {
    const auto& t = *g_theme;
    editor_left_panel_view_result result;
    const Rectangle panel = layout::kLeftPanelRect;
    const Rectangle content = {panel.x + kPanelInset, panel.y + kPanelInset,
                               panel.width - kPanelInset * 2.0f, panel.height - kPanelInset * 2.0f};
    const char* status_label = model.is_dirty ? "MODIFIED" : (model.has_file ? "SAVED" : "UNSAVED");

    ui::draw_rect_f(panel, panel_tint(t.panel, t.bg_alt, 0.18f));
    ui::draw_rect_lines(panel, 1.5f, t.border);

    ui::draw_text_in_rect("CHART", 15, row(content, 0.0f, 20.0f), t.text_muted, ui::text_align::left);
    draw_marquee_text(model.song_title, content.x, content.y + 26.0f, 24, t.text, content.width, model.now);
    draw_badge({content.x, content.y + 62.0f, 95.0f, 24.0f}, status_label,
               model.is_dirty ? t.slow : t.success, model.is_dirty ? t.slow : t.success);
    draw_difficulty_level_badge(model.level,
                                {content.x + 103.0f, content.y + 62.0f, 76.0f, 24.0f},
                                13, 255);

    const Rectangle palette = {content.x, content.y + 112.0f, content.width, 384.0f};
    ui::draw_section(palette);
    ui::draw_text_in_rect("Tool", 22,
                          {palette.x + 12.0f, palette.y + 10.0f, palette.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("Place notes and toggle ray notes.",
                          13,
                          {palette.x + 12.0f, palette.y + 36.0f, palette.width - 24.0f, 18.0f},
                          t.text_muted, ui::text_align::left);
    const float gap = 8.0f;
    const float pad_height = 44.0f;
    const float pad_width = palette.width - 24.0f;
    const float note_row_y = palette.y + 62.0f;
    draw_palette_pad({palette.x + 12.0f, note_row_y, pad_width, pad_height},
                     note_type::tap, model.note_palette, result);
    draw_palette_pad({palette.x + 12.0f, note_row_y + (pad_height + gap), pad_width, pad_height},
                     note_type::hold, model.note_palette, result);
    draw_palette_pad({palette.x + 12.0f, note_row_y + (pad_height + gap) * 2.0f, pad_width, pad_height},
                     note_type::release, model.note_palette, result);
    draw_palette_pad({palette.x + 12.0f, note_row_y + (pad_height + gap) * 3.0f, pad_width, pad_height},
                     note_type::stay, model.note_palette, result);
    draw_palette_pad({palette.x + 12.0f, note_row_y + (pad_height + gap) * 4.0f, pad_width, pad_height},
                     note_type::decorative_hold, model.note_palette, result);

    result.ray_toggled = draw_ray_toggle(
        {palette.x + 12.0f, note_row_y + (pad_height + gap) * 5.0f + 8.0f,
         palette.width - 24.0f, pad_height},
        model.note_palette.is_ray);

    if (model.load_error != nullptr) {
        ui::draw_text_in_rect(model.load_error->c_str(), 16,
                              {content.x, content.y + content.height - 58.0f, content.width, 52.0f},
                              t.error, ui::text_align::left);
    }

    return result;
}

editor_right_panel_view_result draw_right_panel(const editor_right_panel_view_model& model,
                                                editor_timing_panel_state& timing_state) {
    const auto& t = *g_theme;
    editor_right_panel_view_result result;
    const Rectangle panel = layout::kRightPanelRect;
    const Rectangle content = inset_rect(panel, kPanelInset);

    const std::vector<scroll_automation_point> empty_points;
    const std::vector<scroll_automation_point>& points =
        model.scroll_automation != nullptr ? *model.scroll_automation : empty_points;
    scroll_automation_guides editable_guides =
        model.scroll_guides != nullptr ? *model.scroll_guides : scroll_automation_guides{};
    const std::array<float, 5> guides = automation_guides(editable_guides);
    std::vector<std::pair<size_t, scroll_automation_point>> sorted_points;
    sorted_points.reserve(points.size());
    int max_tick = 1920;
    for (size_t index = 0; index < points.size(); ++index) {
        sorted_points.push_back({index, points[index]});
        max_tick = std::max(max_tick, points[index].tick + 480);
    }
    std::stable_sort(sorted_points.begin(), sorted_points.end(), [](const auto& left, const auto& right) {
        return left.second.tick < right.second.tick;
    });

    ui::draw_rect_f(panel, panel_tint(t.panel, t.bg_alt, 0.14f));
    ui::draw_rect_lines(panel, 1.5f, t.border);
    ui::draw_text_in_rect("SCROLL", 15, {content.x, content.y, content.width, 20.0f},
                          t.text_muted, ui::text_align::left);

    const Rectangle graph_box = {content.x, content.y + 42.0f, content.width, 454.0f};
    ui::draw_section(graph_box);
    ui::draw_text_in_rect("Velocity Curve", 20,
                          {graph_box.x + 12.0f, graph_box.y + 10.0f, graph_box.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    const Rectangle graph = {graph_box.x + 12.0f, graph_box.y + 52.0f,
                             graph_box.width - 24.0f, graph_box.height - 92.0f};
    ui::draw_rect_f(graph, with_alpha(t.bg_alt, 120));
    ui::draw_rect_lines(graph, 1.0f, t.border_light);

    auto point_to_pos = [&](const scroll_automation_point& point) {
        const float tick_t = static_cast<float>(std::clamp(point.tick, 0, max_tick)) / static_cast<float>(max_tick);
        const float mult_t = automation_multiplier_to_t(guides, point.multiplier);
        return Vector2{graph.x + mult_t * graph.width, graph.y + tick_t * graph.height};
    };
    auto pos_to_point = [&](Vector2 pos,
                            scroll_automation_curve curve,
                            std::optional<float> pinned_multiplier = std::nullopt) {
        const float tick_t = std::clamp((pos.y - graph.y) / graph.height, 0.0f, 1.0f);
        const float mult_t = std::clamp((pos.x - graph.x) / graph.width, 0.0f, 1.0f);
        scroll_automation_point point;
        point.tick = static_cast<int>(std::round(tick_t * static_cast<float>(max_tick) / 10.0f)) * 10;
        point.multiplier = snap_automation_multiplier(
            automation_snap_candidates(guides, pinned_multiplier),
            automation_multiplier_at_t(guides, mult_t),
            IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
        point.curve_to_next = curve;
        return point;
    };

    for (std::size_t guide_index = 0; guide_index < guides.size(); ++guide_index) {
        const float guide = guides[guide_index];
        const float x = graph.x + graph.width * automation_guide_t(guide_index);
        const bool unity = std::fabs(guide - 1.0f) < 0.001f;
        ui::draw_line_f(x, graph.y, x, graph.y + graph.height,
                        unity ? with_alpha(t.fast, 210) : with_alpha(t.border_light, 120));
        ui::draw_text_in_rect(TextFormat("%.1fx", guide), 11,
                              {x - 22.0f, graph.y + graph.height + 6.0f, 44.0f, 16.0f},
                              unity ? t.fast : t.text_muted);
    }
    for (int i = 0; i <= 8; ++i) {
        const float y = graph.y + graph.height * static_cast<float>(i) / 8.0f;
        ui::draw_line_f(graph.x, y, graph.x + graph.width, y, with_alpha(t.editor_grid_minor, 120));
    }
    ui::draw_text_in_rect("BAR", 11, {graph_box.x + 8.0f, graph.y - 20.0f, 36.0f, 18.0f},
                          t.text_muted, ui::text_align::right);

    for (size_t i = 1; i < sorted_points.size(); ++i) {
        const Vector2 from = point_to_pos(sorted_points[i - 1].second);
        const Vector2 to = point_to_pos(sorted_points[i].second);
        DrawLineEx(from, to, 2.2f, with_alpha(t.fast, 210));
    }

    std::optional<size_t> hovered_point;
    for (size_t i = sorted_points.size(); i > 0; --i) {
        const size_t sorted_index = i - 1;
        const size_t point_index = sorted_points[sorted_index].first;
        const scroll_automation_point& point = sorted_points[sorted_index].second;
        const Vector2 p = point_to_pos(point);
        const bool selected = model.selected_scroll_event_index.has_value() &&
                              *model.selected_scroll_event_index == point_index;
        const Rectangle hit = {p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f};
        if (!hovered_point.has_value() && CheckCollisionPointRec(model.mouse, hit)) {
            hovered_point = point_index;
        }
        if (timing_state.automation_drag_point_index.has_value() &&
            *timing_state.automation_drag_point_index == point_index) {
            hovered_point = point_index;
        }
        const float handle_size = selected ? 14.0f : 11.0f;
        const Rectangle handle = {p.x - handle_size * 0.5f, p.y - handle_size * 0.5f,
                                  handle_size, handle_size};
        ui::draw_rect_f(handle, selected ? t.accent : t.fast);
        ui::draw_rect_lines(handle, selected ? 2.0f : 1.4f,
                            selected ? t.text : with_alpha(t.text, 170));
    }

    const bool graph_hovered = CheckCollisionPointRec(model.mouse, graph);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && graph_hovered) {
        if (hovered_point.has_value()) {
            result.panel_result.selected_scroll_event_index = hovered_point;
            timing_state.automation_drag_point_index = hovered_point;
            timing_state.automation_pending_add = false;
            result.scroll_automation_point_to_modify = std::make_pair(
                *hovered_point,
                pos_to_point(model.mouse,
                             points[*hovered_point].curve_to_next,
                             points[*hovered_point].multiplier));
        } else {
            timing_state.automation_drag_point_index.reset();
            timing_state.automation_pending_add = true;
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (timing_state.automation_pending_add && graph_hovered &&
            !timing_state.automation_drag_point_index.has_value()) {
            result.scroll_automation_point_to_add = pos_to_point(model.mouse, scroll_automation_curve::linear);
        }
        timing_state.automation_drag_point_index.reset();
        timing_state.automation_pending_add = false;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        timing_state.automation_drag_point_index.has_value() &&
        *timing_state.automation_drag_point_index < points.size()) {
        const size_t point_index = *timing_state.automation_drag_point_index;
        scroll_automation_point updated = pos_to_point(
            model.mouse,
            points[point_index].curve_to_next,
            points[point_index].multiplier);
        result.panel_result.selected_scroll_event_index = point_index;
        result.scroll_automation_point_to_modify = std::make_pair(point_index, updated);
    }

    const float button_gap = 8.0f;
    const float button_width = (graph_box.width - 24.0f - button_gap * 2.0f) / 3.0f;
    const Rectangle add_button = {graph_box.x + 12.0f, graph_box.y + graph_box.height - 34.0f,
                                  button_width, 26.0f};
    const Rectangle curve_button = {add_button.x + button_width + button_gap, add_button.y, button_width, 26.0f};
    const Rectangle delete_button = {curve_button.x + button_width + button_gap, add_button.y, button_width, 26.0f};
    if (ui::draw_button(add_button, "Add Point", 13).clicked) {
        result.scroll_automation_point_to_add =
            scroll_automation_point{std::clamp(max_tick / 2, 0, max_tick), 1.0f, scroll_automation_curve::linear};
    }
    const ui::button_state curve_state = ui::draw_button_colored(
        curve_button, "Curve", 13,
        model.scroll_delete_enabled ? t.row : t.section,
        model.scroll_delete_enabled ? t.row_hover : t.section,
        model.scroll_delete_enabled ? t.text : t.text_hint,
        1.4f);
    if (model.scroll_delete_enabled && curve_state.clicked) {
        result.panel_result.cycle_selected_scroll_curve = true;
    }
    const ui::button_state delete_state = ui::draw_button_colored(
        delete_button, "Delete", 13,
        model.scroll_delete_enabled ? t.row : t.section,
        model.scroll_delete_enabled ? t.row_hover : t.section,
        model.scroll_delete_enabled ? t.text : t.text_hint,
        1.4f);
    if (model.scroll_delete_enabled && delete_state.clicked) {
        result.panel_result.delete_selected_scroll = true;
    }

    const Rectangle inspector = {content.x, graph_box.y + graph_box.height + 14.0f,
                                 content.width, content.y + content.height - (graph_box.y + graph_box.height + 14.0f)};
    ui::draw_section(inspector);
    ui::draw_text_in_rect("Point", 20,
                          {inspector.x + 12.0f, inspector.y + 10.0f, inspector.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    if (model.selected_scroll_event_index.has_value() &&
        *model.selected_scroll_event_index < points.size()) {
        const scroll_automation_point& point = points[*model.selected_scroll_event_index];
        ui::draw_label_value({inspector.x + 12.0f, inspector.y + 48.0f, inspector.width - 24.0f, 22.0f},
                             "Bar", model.meter_map->bar_beat_label(point.tick).c_str(), 15,
                             t.text_secondary, t.text, 72.0f);
        ui::draw_label_value({inspector.x + 12.0f, inspector.y + 76.0f, inspector.width - 24.0f, 22.0f},
                             "Rate", TextFormat("%.2fx", point.multiplier), 15,
                             t.text_secondary, t.text, 72.0f);
        ui::draw_label_value({inspector.x + 12.0f, inspector.y + 104.0f, inspector.width - 24.0f, 22.0f},
                             "Curve", scroll_curve_label(point.curve_to_next), 15,
                             t.text_secondary, t.text, 72.0f);
    } else {
        ui::draw_text_in_rect("Click the graph to create a point.", 15,
                              {inspector.x + 12.0f, inspector.y + 54.0f,
                               inspector.width - 24.0f, 22.0f},
                              t.text_hint, ui::text_align::left);
    }

    result.clicked_outside_editor = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                    !CheckCollisionPointRec(model.mouse, graph_box) &&
                                    !CheckCollisionPointRec(model.mouse, inspector);
    return result;
}

editor_header_view_result draw_header(const editor_header_view_model& model, Rectangle snap_menu_rect) {
    const auto& t = *g_theme;
    editor_header_view_result result;
    const Rectangle bar = layout::kHeaderRect;
    const Rectangle content = inset_rect(bar, 10.0f);

    ui::draw_rect_f(bar, panel_tint(t.panel, t.bg_alt, 0.18f));
    ui::draw_rect_lines(bar, 1.5f, t.border);
    draw_icon_button(layout::kBackButtonRect, raythm_icons::draw_chevron_left, false, t.text);
    draw_icon_button(layout::kSettingsButtonRect, raythm_icons::draw_settings_gear, false, t.text);

    const Rectangle meta_button = {content.x + 168.0f, content.y + 8.0f, 86.0f, 34.0f};
    const Rectangle timing_button = {meta_button.x + meta_button.width + 8.0f, meta_button.y, 94.0f, 34.0f};
    result.metadata_modal_requested = ui::draw_button_colored(
        meta_button, "META", 13, t.row, t.row_hover, t.text_secondary, 1.2f).clicked;
    result.timing_modal_requested = ui::draw_button_colored(
        timing_button, "TIMING", 13, t.row, t.row_hover, t.text_secondary, 1.2f).clicked;

    const Rectangle transport = layout::kHeaderTransportRect;
    ui::draw_section(transport);
    const Rectangle restart_rect = layout::kHeaderRestartButtonRect;
    draw_icon_button(restart_rect, raythm_icons::draw_skip_back, false, t.text);
    const Rectangle play_rect = layout::kHeaderPlayButtonRect;
    if (model.audio_playing) {
        draw_icon_button(play_rect, raythm_icons::draw_pause, true, t.accent);
    } else {
        draw_icon_button(play_rect, raythm_icons::draw_play, false, t.text);
    }
    const Rectangle playtest_rect = layout::kHeaderPlaytestButtonRect;
    draw_icon_button(playtest_rect, raythm_icons::draw_flask_conical, false, t.text);
    ui::enqueue_hover_tooltip(playtest_rect, "プレイテスト");
    const ui::dropdown_state dropdown = ui::enqueue_dropdown(
        layout::kSnapDropdownRect, snap_menu_rect,
        "Snap", model.snap_labels[model.snap_index],
        model.snap_labels,
        model.snap_index, model.snap_dropdown_open,
        ui::draw_layer::base, ui::draw_layer::overlay,
        14, 58.0f);
    result.snap_dropdown_toggled = dropdown.trigger.clicked;
    result.snap_index_clicked = dropdown.clicked_index;
    result.snap_dropdown_close_requested =
        model.snap_dropdown_open && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !ui::is_hovered(layout::kSnapDropdownRect, ui::draw_layer::base) &&
        !ui::is_hovered(snap_menu_rect, ui::draw_layer::overlay);
    return result;
}

editor_right_panel_view_result draw_timeline(const editor_timeline_presenter_model& presenter_model,
                                             Rectangle snap_menu_rect,
                                             bool snap_dropdown_open) {
    static std::optional<size_t> automation_drag_point_index;
    static bool automation_pending_add = false;

    const auto& t = *g_theme;
    editor_right_panel_view_result result;
    const editor_timeline_view_model model = make_timeline_model(presenter_model);
    const Rectangle panel = model.metrics.panel_rect;
    const Rectangle content = model.metrics.content_rect();
    const Rectangle track = model.metrics.scrollbar_track_rect();
    auto contains_sorted_index = [](const std::vector<size_t>& indices, size_t index) {
        return std::binary_search(indices.begin(), indices.end(), index);
    };

    ui::draw_rect_f(panel, panel_tint(t.panel, t.bg_alt, 0.12f));
    ui::draw_rect_lines(panel, 1.5f, t.border);

    const Rectangle arrange = content;
    const Rectangle minimap = track;
    const Rectangle ruler = {track.x + track.width + 8.0f, arrange.y, 60.0f, arrange.height};
    const Rectangle automation = {arrange.x + arrange.width + 8.0f, arrange.y, kAutomationWidth, arrange.height};
    const Rectangle ruler_labels = {ruler.x, arrange.y, ruler.width, arrange.height};
    {
        ui::scoped_clip_rect clip_scope(arrange);

        for (int lane = 0; lane < std::max(1, model.metrics.key_count); ++lane) {
            Rectangle lane_rect = model.metrics.lane_rect(lane);
            lane_rect.y = arrange.y;
            lane_rect.height = arrange.height;
            ui::draw_rect_f(lane_rect, lane % 2 == 0 ? with_alpha(t.row, 28) : with_alpha(t.section, 36));
            ui::draw_rect_lines(lane_rect, 1.0f, with_alpha(t.border_light, 150));
        }

        const int snap_interval = std::max(1, model.snap_interval);
        const int first_snap_tick = std::max(0, (model.min_tick / snap_interval) * snap_interval);
        for (int tick = first_snap_tick; tick <= model.max_tick; tick += snap_interval) {
            const float y = model.metrics.tick_to_y(tick);
            if (y >= arrange.y && y <= arrange.y + arrange.height) {
                ui::draw_line_f(arrange.x, y, arrange.x + arrange.width, y, with_alpha(t.editor_grid_snap, 165));
            }
        }

        for (const editor_meter_map::grid_line& line : model.grid_lines) {
            const float y = model.metrics.tick_to_y(line.tick);
            if (y < arrange.y || y > arrange.y + arrange.height) {
                continue;
            }
            ui::draw_line_f(arrange.x, y, arrange.x + arrange.width, y,
                            line.major ? t.editor_grid_major : t.editor_grid_minor);
            if (line.major) {
                ui::draw_line_f(arrange.x, y + 1.0f, arrange.x + arrange.width, y + 1.0f,
                                t.editor_grid_major_glow);
            }
        }

        const bool simplified_notes =
            model.notes.size() > 450 || model.metrics.ticks_per_pixel >= 6.0f;
        for (size_t index = 0; index < model.notes.size(); ++index) {
            const editor_timeline_note& note = model.notes[index];
            if (contains_sorted_index(model.preview_note_indices, note.source_index)) {
                continue;
            }
            if (note.lane < 0 || note.lane >= model.metrics.key_count) {
                continue;
            }
            const editor_timeline_note_geometry geometry = model.metrics.note_rects(note);
            const bool selected = contains_sorted_index(model.selected_note_indices, note.source_index);
            draw_note_block(note, geometry, selected, false, false, simplified_notes);
        }

        for (const editor_timeline_note& preview_note : model.preview_notes) {
            const editor_timeline_note_geometry geometry = model.metrics.note_rects(preview_note);
            draw_note_block(preview_note, geometry, true, true, model.preview_has_overlap, simplified_notes);
        }

        if (model.selection_rect.has_value()) {
            const Rectangle rect = *model.selection_rect;
            ui::draw_rect_f(rect, with_alpha(t.accent, 32));
            ui::draw_rect_lines(rect, 1.5f, with_alpha(t.accent, 220));
        }

        if (model.playback_tick.has_value()) {
            const float y = model.metrics.tick_to_y(*model.playback_tick);
            DrawLineEx({arrange.x, y}, {arrange.x + arrange.width, y}, 3.0f, t.accent);
        }
    }

    draw_chart_minimap(model, minimap);

    ui::draw_rect_f(automation, with_alpha(t.section, 235));
    ui::draw_rect_lines(automation, 1.0f, t.border_light);
    const Rectangle automation_header = {automation.x, automation.y, 24.0f, automation.height};
    const Rectangle automation_body = {automation_header.x + automation_header.width, automation.y,
                                       automation.width - automation_header.width, automation.height};
    ui::draw_rect_f(automation_header, with_alpha(t.panel, 235));
    ui::draw_rect_lines(automation_header, 1.0f, with_alpha(t.border_light, 190));
    ui::draw_text_in_rect("S", 13,
                          {automation_header.x + 4.0f, automation_header.y + 8.0f,
                           automation_header.width - 8.0f, 18.0f},
                          t.text_secondary);
    std::optional<float> selected_multiplier;
    if (model.selected_scroll_event_index.has_value() &&
        *model.selected_scroll_event_index < model.scroll_automation.size()) {
        selected_multiplier = model.scroll_automation[*model.selected_scroll_event_index].multiplier;
    }
    ui::draw_text_in_rect(selected_multiplier.has_value()
                              ? TextFormat("%.2fx", *selected_multiplier)
                              : "Rate",
                          11,
                          {automation_header.x + 2.0f, automation_header.y + automation_header.height - 28.0f,
                           automation_header.width - 4.0f, 18.0f},
                          selected_multiplier.has_value() ? t.fast : t.text_muted, ui::text_align::left);
    const Rectangle automation_graph = ui::inset(automation_body, ui::edge_insets{10.0f, 10.0f, 10.0f, 8.0f});
    ui::draw_rect_f(automation_graph, with_alpha(t.bg_alt, 80));
    scroll_automation_guides editable_guides = presenter_model.state.data().scroll_guides;
    const scroll_automation_guides original_guides = editable_guides;
    const std::array<float, 5> guides = automation_guides(editable_guides);
    bool guide_input_interacting = false;
    for (size_t guide_index = 0; guide_index < guides.size(); ++guide_index) {
        const float guide = guides[guide_index];
        const float x = automation_graph.x + automation_graph.width * automation_guide_t(guide_index);
        const bool unity = std::fabs(guide - 1.0f) < 0.001f;
        ui::draw_line_f(x, automation_graph.y, x, automation_graph.y + automation_graph.height,
                        unity ? with_alpha(t.fast, 210) : with_alpha(t.border_light, 110));
        Rectangle value_rect = {x - 34.0f, automation_graph.y + 4.0f, 68.0f, 24.0f};
        value_rect.x = std::clamp(value_rect.x,
                                  automation_graph.x + 2.0f,
                                  automation_graph.x + automation_graph.width - value_rect.width - 2.0f);
        if (unity) {
            ui::draw_text_in_rect("1.0", 12, value_rect, t.fast);
        } else {
            const size_t side_index = guide_index < 2 ? guide_index : guide_index - 1;
            guide_input_interacting = draw_automation_guide_input(side_index, value_rect, editable_guides) ||
                                      guide_input_interacting;
        }
    }
    if (editable_guides.values != original_guides.values) {
        result.scroll_automation_guides_to_modify = editable_guides;
    }
    const int snap_interval = std::max(1, model.snap_interval);
    const int first_snap_tick = std::max(0, (model.min_tick / snap_interval) * snap_interval);
    for (int tick = first_snap_tick; tick <= model.max_tick; tick += snap_interval) {
        const float y = model.metrics.tick_to_y(tick);
        if (y >= automation_graph.y && y <= automation_graph.y + automation_graph.height) {
            ui::draw_line_f(automation_graph.x, y, automation_graph.x + automation_graph.width, y,
                            with_alpha(t.editor_grid_snap, 165));
        }
    }
    for (const editor_meter_map::grid_line& line : model.grid_lines) {
        const float y = model.metrics.tick_to_y(line.tick);
        if (y < automation_graph.y || y > automation_graph.y + automation_graph.height) {
            continue;
        }
        const Color color = line.major ? t.editor_grid_major : t.editor_grid_minor;
        ui::draw_line_f(automation_graph.x, y, automation_graph.x + automation_graph.width, y, color);
        if (line.major) {
            ui::draw_line_f(automation_graph.x, y + 1.0f, automation_graph.x + automation_graph.width, y + 1.0f,
                            t.editor_grid_major_glow);
        }
    }
    std::vector<std::pair<size_t, editor_timeline_scroll_automation_point>> sorted_points;
    sorted_points.reserve(model.scroll_automation.size());
    for (size_t index = 0; index < model.scroll_automation.size(); ++index) {
        sorted_points.push_back({index, model.scroll_automation[index]});
    }
    std::stable_sort(sorted_points.begin(), sorted_points.end(), [](const auto& left, const auto& right) {
        return left.second.tick < right.second.tick;
    });
    auto point_x = [&](float multiplier) {
        const float t_value = automation_multiplier_to_t(guides, multiplier);
        return automation_graph.x + t_value * automation_graph.width;
    };
    const float unity_x = point_x(1.0f);
    ui::draw_line_f(unity_x, automation_graph.y, unity_x, automation_graph.y + automation_graph.height,
                    with_alpha(t.fast, 150));
    auto point_at_mouse = [&](Vector2 mouse,
                              scroll_automation_curve curve,
                              std::optional<float> pinned_multiplier = std::nullopt) {
        scroll_automation_point point;
        const int raw_tick = std::max(0, model.metrics.y_to_tick(mouse.y));
        point.tick = std::max(0, (raw_tick + snap_interval / 2) / snap_interval * snap_interval);
        point.multiplier = snap_automation_multiplier(
            automation_snap_candidates(guides, pinned_multiplier),
            automation_multiplier_at_t(
                guides,
                std::clamp((mouse.x - automation_graph.x) / automation_graph.width, 0.0f, 1.0f)),
            IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
        point.curve_to_next = curve;
        return point;
    };
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool snap_ui_hovered =
        CheckCollisionPointRec(mouse, layout::kSnapDropdownRect) ||
        (snap_dropdown_open && CheckCollisionPointRec(mouse, snap_menu_rect));
    std::optional<size_t> hovered_point;
    {
        ui::scoped_clip_rect clip_scope(automation_graph);
        for (size_t index = 1; index < sorted_points.size(); ++index) {
            const auto& previous = sorted_points[index - 1].second;
            const auto& current = sorted_points[index].second;
            const Vector2 from = {point_x(previous.multiplier), model.metrics.tick_to_y(previous.tick)};
            const Vector2 to = {point_x(current.multiplier), model.metrics.tick_to_y(current.tick)};
            DrawLineEx(from, to, 2.0f, with_alpha(t.fast, 220));
        }
        for (size_t reverse = sorted_points.size(); reverse > 0; --reverse) {
            const size_t sorted_index = reverse - 1;
            const size_t point_index = sorted_points[sorted_index].first;
            const auto& point = sorted_points[sorted_index].second;
            const Vector2 pos = {point_x(point.multiplier), model.metrics.tick_to_y(point.tick)};
            if (pos.y < automation_graph.y || pos.y > automation_graph.y + automation_graph.height) {
                continue;
            }
            const Rectangle hit = {pos.x - 8.0f, pos.y - 8.0f, 16.0f, 16.0f};
            if (!snap_ui_hovered && !hovered_point.has_value() && CheckCollisionPointRec(mouse, hit)) {
                hovered_point = point_index;
            }
            if (automation_drag_point_index.has_value() && *automation_drag_point_index == point_index) {
                hovered_point = point_index;
            }
            const bool hovered = hovered_point.has_value() && *hovered_point == point_index;
            const float handle_size = hovered ? 13.0f : 11.0f;
            const Rectangle handle = {pos.x - handle_size * 0.5f, pos.y - handle_size * 0.5f,
                                      handle_size, handle_size};
            ui::draw_rect_f(handle, hovered ? t.accent : t.fast);
            ui::draw_rect_lines(handle, hovered ? 2.0f : 1.4f,
                                hovered ? t.text : with_alpha(t.text, 170));
        }
    }
    const bool automation_hovered =
        CheckCollisionPointRec(mouse, automation_graph) && !guide_input_interacting && !snap_ui_hovered;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && automation_hovered) {
        if (hovered_point.has_value()) {
            result.panel_result.selected_scroll_event_index = hovered_point;
            automation_drag_point_index = hovered_point;
            automation_pending_add = false;
            result.scroll_automation_point_to_modify = std::make_pair(
                *hovered_point,
                point_at_mouse(mouse,
                               model.scroll_automation[*hovered_point].curve_to_next,
                               model.scroll_automation[*hovered_point].multiplier));
        } else {
            automation_drag_point_index.reset();
            automation_pending_add = true;
        }
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && guide_input_interacting) {
        automation_drag_point_index.reset();
        automation_pending_add = false;
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && snap_ui_hovered) {
        automation_drag_point_index.reset();
        automation_pending_add = false;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (automation_pending_add && automation_hovered && !automation_drag_point_index.has_value()) {
            result.scroll_automation_point_to_add = point_at_mouse(mouse, scroll_automation_curve::linear);
        }
        automation_drag_point_index.reset();
        automation_pending_add = false;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        !snap_ui_hovered &&
        automation_drag_point_index.has_value() &&
        *automation_drag_point_index < model.scroll_automation.size()) {
        result.scroll_automation_point_to_modify = std::make_pair(
            *automation_drag_point_index,
            point_at_mouse(mouse,
                           model.scroll_automation[*automation_drag_point_index].curve_to_next,
                           model.scroll_automation[*automation_drag_point_index].multiplier));
        result.panel_result.selected_scroll_event_index = automation_drag_point_index;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !snap_ui_hovered && hovered_point.has_value()) {
        result.panel_result.selected_scroll_event_index = hovered_point;
        result.panel_result.delete_selected_scroll = true;
    }

    ui::draw_rect_f(ruler, with_alpha(t.section, 235));
    ui::draw_rect_lines(ruler, 1.0f, t.border_light);
    draw_waveform(model, ui::inset(ruler, 4.0f));
    ui::draw_text_in_rect("BAR", 11, {ruler.x, ruler.y + 8.0f, ruler.width, 16.0f},
                          t.text_muted);
    for (const editor_meter_map::grid_line& line : model.grid_lines) {
        if (!line.major) {
            continue;
        }
        const float y = model.metrics.tick_to_y(line.tick);
        if (y < ruler_labels.y || y > ruler_labels.y + ruler_labels.height) {
            continue;
        }
        const Rectangle tag = {ruler.x + 6.0f, y - 11.0f, ruler.width - 12.0f, 22.0f};
        ui::draw_rect_f(tag, with_alpha(t.panel, 225));
        ui::draw_rect_lines(tag, 1.0f, with_alpha(t.border_light, 190));
        ui::draw_text_in_rect(TextFormat("%d:%d", line.measure, line.beat), 12, tag, t.text_secondary);
    }
    return result;
}

metadata_modal_result draw_metadata_modal(const editor_left_panel_view_model& model) {
    const auto& t = *g_theme;
    metadata_modal_result result;
    metadata_panel_state& metadata_panel = *model.metadata_panel;
    const Rectangle modal = layout::kEditorMetadataModalRect;
    const Rectangle content = inset_rect(modal, 24.0f);

    ui::draw_rect_f(layout::kScreenRect, g_theme->pause_overlay);
    ui::draw_panel(modal);
    ui::draw_text_in_rect("Chart Metadata", 28,
                          {content.x, content.y, content.width, 32.0f},
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("These settings are outside the main editing surface.",
                          15,
                          {content.x, content.y + 34.0f, content.width, 22.0f},
                          t.text_muted, ui::text_align::left);

    const Rectangle song_box = {content.x, content.y + 76.0f, content.width, 72.0f};
    ui::draw_section(song_box);
    ui::draw_label_value({song_box.x + 14.0f, song_box.y + 13.0f, song_box.width - 28.0f, 22.0f},
                         "Song", model.song_title, 16, t.text_muted, t.text, 78.0f);
    ui::draw_label_value({song_box.x + 14.0f, song_box.y + 40.0f, song_box.width - 28.0f, 20.0f},
                         "Status", model.is_dirty ? "Modified" : (model.has_file ? "Saved" : "Unsaved"),
                         14, t.text_muted, model.is_dirty ? t.slow : t.success, 78.0f);

    const Rectangle form = {content.x, song_box.y + song_box.height + 16.0f, content.width, 148.0f};
    ui::draw_section(form);
    result.metadata_result.difficulty_result = ui::draw_text_input(
        {form.x + 16.0f, form.y + 18.0f, form.width - 32.0f, 38.0f},
        metadata_panel.difficulty_input, "Diff", "Difficulty", "New",
        ui::draw_layer::modal, 16, 24, accepts_metadata_character, 74.0f);
    result.metadata_result.author_result = ui::draw_text_input(
        {form.x + 16.0f, form.y + 66.0f, form.width - 32.0f, 38.0f},
        metadata_panel.chart_author_input, "Author", "Chart author", "Unknown",
        ui::draw_layer::modal, 16, 32, accepts_metadata_character, 74.0f);
    const ui::selector_state key_count_selector = ui::draw_value_selector(
        {form.x + 16.0f, form.y + 114.0f, form.width - 32.0f, 28.0f},
        "Lanes", key_count_label(metadata_panel.key_count),
        ui::draw_layer::modal, 14, 24.0f, 74.0f, 10.0f);
    result.metadata_result.key_count_left_clicked = key_count_selector.left.clicked;
    result.metadata_result.key_count_right_clicked = key_count_selector.right.clicked;

    if (!metadata_panel.error.empty()) {
        ui::draw_text_in_rect(metadata_panel.error.c_str(), 15,
                              {content.x, form.y + form.height + 10.0f, content.width, 22.0f},
                              t.error, ui::text_align::left);
    }

    const Rectangle apply_rect = {content.x + content.width - 260.0f, modal.y + modal.height - 58.0f, 116.0f, 34.0f};
    const Rectangle close_rect = {content.x + content.width - 132.0f, apply_rect.y, 132.0f, 34.0f};
    result.apply_requested = draw_layer_button(apply_rect, "APPLY", 14, ui::draw_layer::modal,
                                               t.row, t.row_hover, t.text).clicked;
    result.close_requested = draw_layer_button(close_rect, "CLOSE", 14, ui::draw_layer::modal,
                                               t.row, t.row_hover, t.text_secondary).clicked;
    return result;
}

timing_modal_result draw_timing_modal(const editor_right_panel_view_model& model,
                                      editor_timing_panel_state& timing_state,
                                      const char* offset_label) {
    const auto& t = *g_theme;
    timing_modal_result result;
    const Rectangle modal = layout::kEditorTimingModalRect;
    const Rectangle content = inset_rect(modal, 22.0f);

    ui::draw_rect_f(layout::kScreenRect, g_theme->pause_overlay);
    ui::draw_panel(modal);
    ui::draw_text_in_rect("Timing Map", 28,
                          {content.x, content.y, content.width, 32.0f},
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("BPM and meter changes are managed here.",
                          15,
                          {content.x, content.y + 34.0f, content.width, 22.0f},
                          t.text_muted, ui::text_align::left);
    const ui::selector_state chart_offset = ui::draw_value_selector(
        {content.x + content.width - 270.0f, content.y + 8.0f, 270.0f, 36.0f},
        "Offset", offset_label,
        ui::draw_layer::modal, 14, 24.0f, 54.0f, 10.0f);
    result.offset_left_clicked = chart_offset.left.clicked;
    result.offset_right_clicked = chart_offset.right.clicked;

    std::vector<editor_timing_panel_item> items;
    items.reserve(model.timing_events->size());
    for (size_t index = 0; index < model.timing_events->size(); ++index) {
        const timing_event& event = (*model.timing_events)[index];
        items.push_back({
            index,
            std::string(timing_event_type_label(event.type)) + " " + model.meter_map->bar_beat_label(event.tick),
            event.type == timing_event_type::bpm
                ? TextFormat("%.1f", event.bpm)
                : TextFormat("%d/%d", event.numerator, event.denominator),
            model.selected_event_index.has_value() && *model.selected_event_index == index
        });
    }

    std::optional<timing_event> selected_event;
    if (model.selected_event_index.has_value() && *model.selected_event_index < model.timing_events->size()) {
        selected_event = (*model.timing_events)[*model.selected_event_index];
    }

    const Rectangle list_box = {content.x, content.y + 74.0f, 390.0f, content.height - 142.0f};
    const Rectangle editor_box = {list_box.x + list_box.width + 18.0f, list_box.y,
                                  content.width - list_box.width - 18.0f, list_box.height};
    ui::draw_section(list_box);
    ui::draw_text_in_rect("Events", 20,
                          {list_box.x + 14.0f, list_box.y + 10.0f, list_box.width - 28.0f, 24.0f},
                          t.text, ui::text_align::left);

    const Rectangle list_view = {list_box.x + 12.0f, list_box.y + 48.0f, list_box.width - 32.0f, list_box.height - 100.0f};
    const Rectangle scrollbar = {list_view.x + list_view.width + 6.0f, list_view.y, 6.0f, list_view.height};
    const float row_height = 34.0f;
    const float row_gap = 5.0f;
    const float content_height = items.empty()
        ? list_view.height
        : static_cast<float>(items.size()) * row_height +
              static_cast<float>(std::max<int>(0, static_cast<int>(items.size()) - 1)) * row_gap;
    const float max_scroll = std::max(0.0f, content_height - list_view.height);
    timing_state.list_scroll_offset = std::clamp(timing_state.list_scroll_offset, 0.0f, max_scroll);
    const ui::scrollbar_interaction scrollbar_result = ui::update_vertical_scrollbar(
        scrollbar,
        content_height,
        timing_state.list_scroll_offset,
        timing_state.list_scrollbar_dragging,
        timing_state.list_scrollbar_drag_offset,
        28.0f);
    if (scrollbar_result.changed || scrollbar_result.dragging) {
        timing_state.list_scroll_offset = scrollbar_result.scroll_offset;
    }
    if (CheckCollisionPointRec(model.mouse, list_view) && GetMouseWheelMove() != 0.0f) {
        timing_state.list_scroll_offset = std::clamp(
            timing_state.list_scroll_offset - GetMouseWheelMove() * 42.0f, 0.0f, max_scroll);
    }
    {
        ui::scoped_clip_rect clip_scope(list_view);
        float y = list_view.y - timing_state.list_scroll_offset;
        for (const editor_timing_panel_item& item : items) {
            const Rectangle item_rect = {list_view.x, y, list_view.width, row_height};
            const ui::row_state row_state = draw_layer_row(item_rect, item.selected, ui::draw_layer::modal, t.accent);
            if (row_state.clicked) {
                result.panel_result.selected_event_index = item.event_index;
            }
            ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 10.0f)),
                                 item.label.c_str(), item.value.c_str(), 14,
                                 item.selected ? t.text : t.text_secondary,
                                 item.selected ? t.text : t.text_muted, 126.0f);
            y += row_height + row_gap;
        }
    }
    ui::draw_scrollbar(scrollbar, content_height, timing_state.list_scroll_offset,
                       t.scrollbar_track, t.scrollbar_thumb, 28.0f);

    const float button_width = (list_box.width - 32.0f - 16.0f) / 3.0f;
    const Rectangle bpm_button = {list_box.x + 12.0f, list_box.y + list_box.height - 42.0f,
                                  button_width, 30.0f};
    const Rectangle meter_button = {bpm_button.x + button_width + 8.0f, bpm_button.y, button_width, 30.0f};
    const Rectangle delete_button = {meter_button.x + button_width + 8.0f, bpm_button.y, button_width, 30.0f};
    if (draw_layer_button(bpm_button, "Add BPM", 13, ui::draw_layer::modal,
                          t.row, t.row_hover, t.text).clicked) {
        result.panel_result.add_bpm = true;
    }
    if (draw_layer_button(meter_button, "Add Meter", 13, ui::draw_layer::modal,
                          t.row, t.row_hover, t.text).clicked) {
        result.panel_result.add_meter = true;
    }
    if (draw_layer_button(delete_button, "Delete", 13, ui::draw_layer::modal,
                          model.delete_enabled ? t.row : t.section,
                          model.delete_enabled ? t.row_hover : t.section,
                          model.delete_enabled ? t.text : t.text_hint).clicked && model.delete_enabled) {
        result.panel_result.delete_selected = true;
    }

    ui::draw_section(editor_box);
    ui::draw_text_in_rect("Event Inspector", 20,
                          {editor_box.x + 14.0f, editor_box.y + 10.0f, editor_box.width - 28.0f, 24.0f},
                          t.text, ui::text_align::left);

    auto draw_pick_row = [&](Rectangle rect, const char* label, const std::string& value,
                             editor_timing_input_field field) {
        const bool selected = timing_state.active_input_field == field || timing_state.bar_pick_mode;
        const ui::row_state row_state = draw_layer_row(rect, selected, ui::draw_layer::modal, t.accent);
        if (row_state.clicked) {
            result.panel_result.clicked_input_row = true;
            timing_state.active_input_field = field;
            timing_state.bar_pick_mode = true;
            timing_state.input_error.clear();
            timing_state.inputs.bpm_value.active = false;
            timing_state.inputs.meter_numerator.active = false;
            timing_state.inputs.meter_denominator.active = false;
        }
        ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 12.0f)),
                             label, timing_state.bar_pick_mode ? "Pick timeline" : value.c_str(),
                             16, selected ? t.text : t.text_secondary,
                             timing_state.bar_pick_mode ? t.accent : t.text, 82.0f);
    };
    auto draw_input_row = [&](Rectangle rect, const char* label, ui::text_input_state& input,
                              editor_timing_input_field field, ui::text_input_filter filter,
                              const char* placeholder, float label_width = 82.0f) {
        const ui::text_input_result input_result = ui::draw_text_input(
            rect, input, label, placeholder, nullptr,
            ui::draw_layer::modal, 16, 16, filter, label_width);
        if (input_result.clicked) {
            result.panel_result.clicked_input_row = true;
            set_active_timing_input(timing_state, field);
            timing_state.bar_pick_mode = false;
            timing_state.input_error.clear();
        }
        if (input_result.submitted) {
            result.panel_result.apply_selected = true;
            set_active_timing_input(timing_state, editor_timing_input_field::none);
            timing_state.bar_pick_mode = false;
        } else if (input_result.deactivated && timing_state.active_input_field == field) {
            set_active_timing_input(timing_state, editor_timing_input_field::none);
        }
    };

    if (selected_event.has_value()) {
        const timing_event& event = *selected_event;
        ui::draw_label_value({editor_box.x + 14.0f, editor_box.y + 52.0f, editor_box.width - 28.0f, 24.0f},
                             "Type", timing_event_type_label(event.type), 16,
                             t.text_secondary, t.text, 82.0f);
        if (event.type == timing_event_type::bpm) {
            draw_pick_row({editor_box.x + 14.0f, editor_box.y + 90.0f, editor_box.width - 28.0f, 38.0f},
                          "Bar", timing_state.inputs.bpm_bar.value, editor_timing_input_field::bpm_measure);
            draw_input_row({editor_box.x + 14.0f, editor_box.y + 138.0f, editor_box.width - 28.0f, 38.0f},
                           "BPM", timing_state.inputs.bpm_value, editor_timing_input_field::bpm_value,
                           accepts_float_character, "BPM");
        } else {
            draw_pick_row({editor_box.x + 14.0f, editor_box.y + 90.0f, editor_box.width - 28.0f, 38.0f},
                          "Bar", timing_state.inputs.meter_bar.value, editor_timing_input_field::meter_measure);
            const float half = (editor_box.width - 36.0f) * 0.5f;
            draw_input_row({editor_box.x + 14.0f, editor_box.y + 138.0f, half, 38.0f},
                           "Num", timing_state.inputs.meter_numerator,
                           editor_timing_input_field::meter_numerator,
                           accepts_int_character, "Num", 46.0f);
            draw_input_row({editor_box.x + 22.0f + half, editor_box.y + 138.0f, half, 38.0f},
                           "Den", timing_state.inputs.meter_denominator,
                           editor_timing_input_field::meter_denominator,
                           accepts_int_character, "Den", 46.0f);
        }
        const Rectangle apply_button = {editor_box.x + 14.0f, editor_box.y + editor_box.height - 48.0f,
                                        editor_box.width - 28.0f, 34.0f};
        if (draw_layer_button(apply_button, "APPLY EVENT", 14, ui::draw_layer::modal,
                              t.row, t.row_hover, t.text).clicked) {
            result.panel_result.apply_selected = true;
        }
        if (!timing_state.input_error.empty()) {
            ui::draw_text_in_rect(timing_state.input_error.c_str(), 15,
                                  {editor_box.x + 14.0f, apply_button.y - 28.0f,
                                   editor_box.width - 28.0f, 22.0f},
                                  t.error, ui::text_align::left);
        }
    } else {
        ui::draw_text_in_rect("Select or add a BPM / meter event.", 17,
                              {editor_box.x + 14.0f, editor_box.y + 60.0f,
                               editor_box.width - 28.0f, 24.0f},
                              t.text_hint, ui::text_align::left);
    }

    const Rectangle close_button = {modal.x + modal.width - 144.0f, modal.y + modal.height - 52.0f, 116.0f, 32.0f};
    result.close_requested = draw_layer_button(close_button, "CLOSE", 14, ui::draw_layer::modal,
                                               t.row, t.row_hover, t.text_secondary).clicked;
    return result;
}

}  // namespace editor::daw
