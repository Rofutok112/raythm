#include "editor/daw/editor_daw_view.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "editor/editor_timeline_view.h"
#include "editor/view/editor_layout.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_text_input.h"
#include "ui/icons/raythm_icons.h"
#include "ui_layout.h"

namespace {
namespace layout = editor::layout;

constexpr float kPanelInset = 14.0f;

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
    }
    return "TAP";
}

const char* timing_event_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Meter";
}

const char* scroll_event_type_label(scroll_event_type type) {
    return type == scroll_event_type::speed ? "Speed" : "Stop";
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

void draw_palette_pad(Rectangle rect,
                      note_type type,
                      const editor_note_palette_selection& selection,
                      editor_left_panel_view_result& result) {
    const auto& t = *g_theme;
    const bool selected = selection.type == type;
    const Color tone = type == note_type::tap ? t.error :
        (type == note_type::hold ? t.success :
        (type == note_type::release ? t.slow :
         (type == note_type::stay ? t.fast : t.accent)));
    const ui::row_state state = ui::draw_row(
        rect,
        selected ? panel_tint(t.row_selected, tone, 0.18f) : t.row,
        selected ? panel_tint(t.row_active, tone, 0.2f) : t.row_hover,
        selected ? tone : t.border_light,
        selected ? 2.0f : 1.0f);
    ui::draw_rect_f({state.visual.x + 10.0f, state.visual.y + state.visual.height - 7.0f,
                     state.visual.width - 20.0f, 3.0f},
                    selected ? tone : with_alpha(t.text_muted, 95));
    ui::draw_text_in_rect(palette_label(type), 14,
                          {state.visual.x + 10.0f, state.visual.y + 7.0f,
                           state.visual.width - 20.0f, 19.0f},
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

editor_timeline_note make_timeline_note(const note_data& note) {
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
    }
    return {type, note.tick, note.lane, note.end_tick, note.is_ray, note_lane_width(note)};
}

editor_timeline_view_model make_timeline_model(const editor_timeline_presenter_model& model) {
    const editor_timeline_metrics metrics = editor_timeline_viewport::metrics(model.viewport);
    const float visible_tick_span = editor_timeline_viewport::visible_tick_span(model.viewport);
    const int min_tick = static_cast<int>(std::floor(model.viewport.viewport.bottom_tick - visible_tick_span * 0.1f));
    const int max_tick = static_cast<int>(std::ceil(model.viewport.viewport.bottom_tick + visible_tick_span));

    std::vector<editor_timeline_note> notes;
    notes.reserve(model.state.data().notes.size());
    for (const note_data& note : model.state.data().notes) {
        notes.push_back(make_timeline_note(note));
    }

    std::vector<editor_timeline_scroll_event> scroll_events;
    scroll_events.reserve(model.state.data().scroll_events.size());
    for (const scroll_event& event : model.state.data().scroll_events) {
        scroll_events.push_back({event.type, event.tick, event.duration, event.multiplier});
    }

    std::vector<editor_timeline_note> preview_notes;
    preview_notes.reserve(model.preview_notes.size());
    for (const note_data& note : model.preview_notes) {
        preview_notes.push_back(make_timeline_note(note));
    }

    return {
        metrics,
        model.meter_map.visible_grid_lines(min_tick, max_tick),
        std::move(scroll_events),
        std::move(notes),
        model.selected_note_index,
        model.selected_note_indices,
        model.selected_scroll_event_index,
        model.audio_loaded ? std::optional<int>(model.playback_tick) : std::nullopt,
        model.loop_enabled,
        model.loop_start_tick,
        model.loop_end_tick,
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
    if (model.waveform_summary == nullptr || model.timing_engine == nullptr) {
        return;
    }

    const int row_count = std::max(1, static_cast<int>(std::ceil(content.height)));
    std::vector<float> rows(static_cast<size_t>(row_count), 0.0f);
    for (const audio_waveform_peak& peak : model.waveform_summary->peaks) {
        const double shifted_ms = peak.seconds * 1000.0 + static_cast<double>(model.waveform_offset_ms);
        const int tick = model.timing_engine->ms_to_tick(shifted_ms);
        if (tick < model.min_tick || tick > model.max_tick) {
            continue;
        }
        const int row_index = static_cast<int>(std::floor(model.metrics.tick_to_y(tick) - content.y));
        if (row_index >= 0 && row_index < row_count) {
            rows[static_cast<size_t>(row_index)] =
                std::max(rows[static_cast<size_t>(row_index)], std::clamp(peak.amplitude, 0.0f, 1.0f));
        }
    }

    const float base_x = content.x + content.width - 4.0f;
    const float max_width = std::max(1.0f, content.width - 10.0f);
    for (int i = 0; i < row_count; ++i) {
        const float amplitude = rows[static_cast<size_t>(i)];
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
    const float width = std::max(note_rect.width * 0.76f, note_rect.width * 0.78f);
    const float height = std::max(4.0f, note_rect.height * (0.40f / 0.78f));
    const float lift = note_rect.height * (2.5f / 0.78f);
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

void draw_note_block(const editor_timeline_note& note,
                     const editor_timeline_note_draw_info& info,
                     bool selected,
                     bool preview,
                     bool overlap) {
    const auto& t = *g_theme;
    const Color fill = overlap ? t.error : editor_play_note_color(note.type, note.is_ray, t.note_color);
    const Color draw_fill = preview ? with_alpha(fill, 170) : fill;

    if (info.has_body) {
        draw_editor_hold_body(info.body_rect, draw_fill, note.is_ray, selected);
        return;
    }

    if (note.type == editor_timeline_note_type::stay) {
        draw_editor_stay_dot(info.head_rect, draw_fill, note.is_ray, selected);
        return;
    }

    draw_editor_tap_slab(info.head_rect, draw_fill, note.type == editor_timeline_note_type::release,
                         note.is_ray, selected);
    if (note.type == editor_timeline_note_type::release) {
        const Color release_seed = note.is_ray ? Color{190, 112, 255, 255} : Color{255, 90, 132, 255};
        const Color release_base = lerp_color(release_seed, draw_fill, note.is_ray ? 0.24f : 0.16f);
        const Color marker = with_alpha(lerp_color(release_base, WHITE, 0.28f), 255);
        const Color contour = with_alpha(lerp_color(release_base, BLACK, 0.16f), 255);
        draw_editor_release_chevron(info.head_rect, marker, contour);
    }
}

float minimap_y_for_tick(const editor_timeline_view_model& model, Rectangle minimap, int tick) {
    const float full_tick_span = std::max(1.0f, model.content_height_pixels * model.metrics.ticks_per_pixel);
    const float max_bottom_tick = model.metrics.bottom_tick +
        model.scroll_offset_pixels * model.metrics.ticks_per_pixel;
    const float min_bottom_tick = max_bottom_tick -
        std::max(0.0f, model.content_height_pixels - model.metrics.content_rect().height) *
            model.metrics.ticks_per_pixel;
    const float ratio = std::clamp((static_cast<float>(tick) - min_bottom_tick) / full_tick_span, 0.0f, 1.0f);
    return minimap.y + minimap.height - ratio * minimap.height;
}

void draw_chart_minimap(const editor_timeline_view_model& model, Rectangle minimap, Rectangle viewport_box) {
    const auto& t = *g_theme;
    ui::draw_rect_f(minimap, with_alpha(t.section, 235));
    ui::draw_rect_lines(minimap, 1.0f, t.border_light);

    const Rectangle inner = ui::inset(minimap, 4.0f);
    {
        ui::scoped_clip_rect clip_scope(inner);
        for (const editor_timeline_note& note : model.notes) {
            if (note.lane < 0 || note.lane >= model.metrics.key_count) {
                continue;
            }
            const float y = minimap_y_for_tick(model, inner, note.tick);
            const float lane_width = inner.width / static_cast<float>(std::max(1, model.metrics.key_count));
            const float x = inner.x + lane_width * static_cast<float>(note.lane);
            const float width = lane_width * static_cast<float>(std::max(1, note.lane_width));
            const Color color = note.is_ray ? Color{174, 96, 255, 255} : WHITE;
            if (note.type == editor_timeline_note_type::hold && note.end_tick > note.tick) {
                const float end_y = minimap_y_for_tick(model, inner, note.end_tick);
                const Rectangle body = {x + width * 0.35f, std::min(y, end_y), std::max(2.0f, width * 0.3f),
                                        std::max(2.0f, std::fabs(end_y - y))};
                ui::draw_rect_f(body, with_alpha(color, note.is_ray ? 165 : 115));
            }
            ui::draw_rect_f({x + 1.0f, y - 1.5f, std::max(2.0f, width - 2.0f), 3.0f},
                            with_alpha(color, note.is_ray ? 235 : 170));
        }

        for (const editor_timeline_scroll_event& event : model.scroll_events) {
            if (event.duration <= 0) {
                continue;
            }
            const float start_y = minimap_y_for_tick(model, inner, event.tick);
            const float end_y = minimap_y_for_tick(model, inner, event.tick + event.duration);
            const Color color = event.type == scroll_event_type::speed ? t.fast : t.error;
            ui::draw_rect_f({inner.x, std::min(start_y, end_y), inner.width,
                             std::max(2.0f, std::fabs(end_y - start_y))},
                            with_alpha(color, 70));
        }
    }

    const Rectangle clipped_box = {
        viewport_box.x,
        std::clamp(viewport_box.y, minimap.y, minimap.y + minimap.height),
        viewport_box.width,
        std::min(viewport_box.height,
                 minimap.y + minimap.height - std::clamp(viewport_box.y, minimap.y, minimap.y + minimap.height))
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

    const Rectangle palette = {content.x, content.y + 112.0f, content.width, 252.0f};
    ui::draw_section(palette);
    ui::draw_text_in_rect("Tool", 22,
                          {palette.x + 12.0f, palette.y + 10.0f, palette.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("Place notes and toggle ray notes.",
                          13,
                          {palette.x + 12.0f, palette.y + 36.0f, palette.width - 24.0f, 18.0f},
                          t.text_muted, ui::text_align::left);
    const float gap = 8.0f;
    const float pad_width = (palette.width - 32.0f) * 0.5f;
    const float pad_height = 56.0f;
    const float note_row_y = palette.y + 66.0f;
    draw_palette_pad({palette.x + 12.0f, note_row_y, pad_width, pad_height},
                     note_type::tap, model.note_palette, result);
    draw_palette_pad({palette.x + 20.0f + pad_width, note_row_y, pad_width, pad_height},
                     note_type::hold, model.note_palette, result);
    draw_palette_pad({palette.x + 12.0f, note_row_y + pad_height + gap, pad_width, pad_height},
                     note_type::release, model.note_palette, result);
    draw_palette_pad({palette.x + 20.0f + pad_width, note_row_y + pad_height + gap, pad_width, pad_height},
                     note_type::stay, model.note_palette, result);

    result.ray_toggled = draw_ray_toggle(
        {palette.x + 12.0f, note_row_y + (pad_height + gap) * 2.0f,
         palette.width - 24.0f, 32.0f},
        model.note_palette.is_ray);

    const Rectangle ops = {content.x, palette.y + palette.height + 14.0f, content.width, 164.0f};
    ui::draw_section(ops);
    ui::draw_text_in_rect("Edit Focus", 20, {ops.x + 12.0f, ops.y + 10.0f, ops.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    ui::draw_label_value({ops.x + 12.0f, ops.y + 50.0f, ops.width - 24.0f, 22.0f},
                         "Edit Target", palette_label(model.note_palette.type),
                         14, t.text_muted, t.text_secondary, 78.0f);
    ui::draw_label_value({ops.x + 12.0f, ops.y + 82.0f, ops.width - 24.0f, 22.0f},
                         "Ray", model.note_palette.is_ray ? "On" : "Off",
                         14, t.text_muted, model.note_palette.is_ray ? t.fast : t.text_secondary, 78.0f);
    ui::draw_label_value({ops.x + 12.0f, ops.y + 114.0f, ops.width - 24.0f, 22.0f},
                         "Snap", "Header control", 14, t.text_muted, t.text_secondary, 78.0f);
    ui::draw_text_in_rect("Metadata and timing live in header modals.",
                          13,
                          {ops.x + 12.0f, ops.y + 138.0f, ops.width - 24.0f, 18.0f},
                          t.text_hint, ui::text_align::left);

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

    ui::draw_rect_f(panel, panel_tint(t.panel, t.bg_alt, 0.14f));
    ui::draw_rect_lines(panel, 1.5f, t.border);
    ui::draw_text_in_rect("SCROLL", 15, {content.x, content.y, content.width, 20.0f},
                          t.text_muted, ui::text_align::left);
    draw_badge({content.x + content.width - 140.0f, content.y - 2.0f, 140.0f, 24.0f},
               "SPEED / STOP", t.fast, t.fast);

    std::vector<editor_timing_panel_item> scroll_items;
    scroll_items.reserve(model.scroll_events->size());
    for (size_t index = 0; index < model.scroll_events->size(); ++index) {
        const scroll_event& event = (*model.scroll_events)[index];
        scroll_items.push_back({
            index,
            std::string("Scroll ") + model.meter_map->bar_beat_label(event.tick),
            event.type == scroll_event_type::speed
                ? TextFormat("%s %.2fx / %dt", scroll_event_type_label(event.type), event.multiplier, event.duration)
                : TextFormat("%s / %dt", scroll_event_type_label(event.type), event.duration),
            model.selected_scroll_event_index.has_value() && *model.selected_scroll_event_index == index
        });
    }

    std::optional<scroll_event> selected_scroll_event;
    if (model.selected_scroll_event_index.has_value() && *model.selected_scroll_event_index < model.scroll_events->size()) {
        selected_scroll_event = (*model.scroll_events)[*model.selected_scroll_event_index];
    }

    const Rectangle list_box = {content.x, content.y + 42.0f, content.width, 372.0f};
    ui::draw_section(list_box);
    ui::draw_text_in_rect("Scroll Regions", 20,
                          {list_box.x + 12.0f, list_box.y + 10.0f, list_box.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    const Rectangle list_view = {list_box.x + 10.0f, list_box.y + 50.0f, list_box.width - 32.0f, 268.0f};
    const Rectangle scrollbar = {list_view.x + list_view.width + 6.0f, list_view.y, 6.0f, list_view.height};
    const float row_height = 34.0f;
    const float row_gap = 5.0f;
    const float content_height = scroll_items.empty()
        ? list_view.height
        : static_cast<float>(scroll_items.size()) * row_height +
              static_cast<float>(std::max<int>(0, static_cast<int>(scroll_items.size()) - 1)) * row_gap;
    const float max_scroll = std::max(0.0f, content_height - list_view.height);
    timing_state.scroll_list_scroll_offset = std::clamp(timing_state.scroll_list_scroll_offset, 0.0f, max_scroll);
    const ui::scrollbar_interaction scrollbar_result = ui::update_vertical_scrollbar(
        scrollbar,
        content_height,
        timing_state.scroll_list_scroll_offset,
        timing_state.scroll_list_scrollbar_dragging,
        timing_state.scroll_list_scrollbar_drag_offset,
        28.0f);
    if (scrollbar_result.changed || scrollbar_result.dragging) {
        timing_state.scroll_list_scroll_offset = scrollbar_result.scroll_offset;
    }
    if (CheckCollisionPointRec(model.mouse, list_view) && GetMouseWheelMove() != 0.0f) {
        timing_state.scroll_list_scroll_offset = std::clamp(
            timing_state.scroll_list_scroll_offset - GetMouseWheelMove() * 42.0f, 0.0f, max_scroll);
    }
    {
        ui::scoped_clip_rect clip_scope(list_view);
        float y = list_view.y - timing_state.scroll_list_scroll_offset;
        for (const editor_timing_panel_item& item : scroll_items) {
            const Rectangle item_rect = {list_view.x, y, list_view.width, row_height};
            const ui::row_state row_state = ui::draw_selectable_row(item_rect, item.selected, 1.4f);
            if (row_state.clicked) {
                result.panel_result.selected_scroll_event_index = item.event_index;
            }
            ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 10.0f)),
                                 item.label.c_str(), item.value.c_str(), 14,
                                 item.selected ? t.text : t.text_secondary,
                                 item.selected ? t.text : t.text_muted, 122.0f);
            y += row_height + row_gap;
        }
    }
    ui::draw_scrollbar(scrollbar, content_height, timing_state.scroll_list_scroll_offset,
                       t.scrollbar_track, t.scrollbar_thumb, 28.0f);

    const float button_gap = 8.0f;
    const float button_width = (list_box.width - 24.0f - button_gap * 2.0f) / 3.0f;
    const Rectangle speed_button = {list_box.x + 12.0f, list_box.y + list_box.height - 40.0f,
                                    button_width, 28.0f};
    const Rectangle stop_button = {speed_button.x + button_width + button_gap, speed_button.y,
                                   button_width, 28.0f};
    const Rectangle delete_button = {stop_button.x + button_width + button_gap, speed_button.y,
                                     button_width, 28.0f};
    if (ui::draw_button(speed_button, "Speed", 14).clicked) {
        result.panel_result.add_speed = true;
    }
    if (ui::draw_button(stop_button, "Stop", 14).clicked) {
        result.panel_result.add_stop = true;
    }
    const ui::button_state delete_state = ui::draw_button_colored(
        delete_button, "Delete", 14,
        model.scroll_delete_enabled ? t.row : t.section,
        model.scroll_delete_enabled ? t.row_hover : t.section,
        model.scroll_delete_enabled ? t.text : t.text_hint,
        1.4f);
    if (model.scroll_delete_enabled && delete_state.clicked) {
        result.panel_result.delete_selected_scroll = true;
    }

    const Rectangle editor_box = {content.x, list_box.y + list_box.height + 14.0f,
                                  content.width, content.y + content.height - (list_box.y + list_box.height + 14.0f)};
    ui::draw_section(editor_box);
    ui::draw_text_in_rect("Region Inspector", 20,
                          {editor_box.x + 12.0f, editor_box.y + 10.0f, editor_box.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    if (selected_scroll_event.has_value()) {
        const scroll_event& event = *selected_scroll_event;
        ui::draw_label_value({editor_box.x + 12.0f, editor_box.y + 46.0f, editor_box.width - 24.0f, 22.0f},
                             "Mode", scroll_event_type_label(event.type), 15,
                             t.text_secondary, t.text, 72.0f);
        auto draw_pick_row = [&](Rectangle rect, const char* label, const std::string& value,
                                 editor_timing_input_field field) {
            const bool selected = timing_state.active_input_field == field || timing_state.bar_pick_mode;
            const ui::row_state row_state = ui::draw_row(
                rect,
                selected ? t.row_selected : t.row,
                selected ? t.row_selected_hover : t.row_hover,
                timing_state.bar_pick_mode ? t.accent : (selected ? t.border_active : t.border),
                1.4f);
            if (row_state.clicked) {
                result.panel_result.clicked_input_row = true;
                timing_state.active_input_field = field;
                timing_state.bar_pick_mode = true;
                timing_state.input_error.clear();
                timing_state.inputs.scroll_duration.active = false;
                timing_state.inputs.scroll_multiplier.active = false;
            }
            ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 10.0f)),
                                 label, timing_state.bar_pick_mode ? "Pick timeline" : value.c_str(),
                                 15, selected ? t.text : t.text_secondary,
                                 timing_state.bar_pick_mode ? t.accent : t.text, 72.0f);
        };
        auto draw_input_row = [&](Rectangle rect, const char* label, ui::text_input_state& input,
                                  editor_timing_input_field field, ui::text_input_filter filter,
                                  const char* placeholder) {
            const ui::text_input_result input_result = ui::draw_text_input(
                rect, input, label, placeholder, nullptr,
                ui::draw_layer::base, 15, 16, filter, 72.0f);
            if (input_result.clicked) {
                result.panel_result.clicked_input_row = true;
                set_active_timing_input(timing_state, field);
                timing_state.bar_pick_mode = false;
                timing_state.input_error.clear();
            }
            if (input_result.submitted) {
                result.panel_result.apply_selected_scroll = true;
                set_active_timing_input(timing_state, editor_timing_input_field::none);
                timing_state.bar_pick_mode = false;
            } else if (input_result.deactivated && timing_state.active_input_field == field) {
                set_active_timing_input(timing_state, editor_timing_input_field::none);
            }
        };
        draw_pick_row({editor_box.x + 12.0f, editor_box.y + 82.0f, editor_box.width - 24.0f, 34.0f},
                      "Start", timing_state.inputs.scroll_start_bar.value, editor_timing_input_field::scroll_start);
        draw_input_row({editor_box.x + 12.0f, editor_box.y + 124.0f, editor_box.width - 24.0f, 34.0f},
                       "Length", timing_state.inputs.scroll_duration, editor_timing_input_field::scroll_duration,
                       accepts_int_character, "ticks");
        if (event.type == scroll_event_type::speed) {
            draw_input_row({editor_box.x + 12.0f, editor_box.y + 166.0f, editor_box.width - 24.0f, 34.0f},
                           "Rate", timing_state.inputs.scroll_multiplier, editor_timing_input_field::scroll_multiplier,
                           accepts_float_character, "1.0x");
        }
        const Rectangle apply_rect = {editor_box.x + 12.0f, editor_box.y + editor_box.height - 42.0f,
                                      editor_box.width - 24.0f, 30.0f};
        if (ui::draw_button(apply_rect, "Apply Region", 14).clicked) {
            result.panel_result.apply_selected_scroll = true;
        }
        if (!timing_state.input_error.empty()) {
            ui::draw_text_in_rect(timing_state.input_error.c_str(), 14,
                                  {editor_box.x + 12.0f, apply_rect.y - 26.0f,
                                   editor_box.width - 24.0f, 20.0f},
                                  t.error, ui::text_align::left);
        }
    } else {
        ui::draw_text_in_rect("Select or create a speed / stop region.", 16,
                              {editor_box.x + 12.0f, editor_box.y + 56.0f,
                               editor_box.width - 24.0f, 22.0f},
                              t.text_hint, ui::text_align::left);
    }

    result.clicked_outside_editor = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                    !CheckCollisionPointRec(model.mouse, editor_box);
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

    const Rectangle transport = {bar.x + bar.width * 0.5f - 78.0f, content.y + 1.0f, 156.0f, 50.0f};
    ui::draw_section(transport);
    const Rectangle restart_rect = {transport.x + 10.0f, transport.y + 4.0f, 42.0f, 42.0f};
    const ui::button_state restart_button =
        draw_icon_button(restart_rect, raythm_icons::draw_skip_back, false, t.text);
    result.restart_requested = restart_button.clicked;
    const Rectangle play_rect = {restart_rect.x + restart_rect.width + 8.0f, restart_rect.y, 42.0f, 42.0f};
    const ui::button_state play_button = model.audio_playing
        ? draw_icon_button(play_rect, raythm_icons::draw_pause, true, t.accent)
        : draw_icon_button(play_rect, raythm_icons::draw_play, false, t.text);
    result.playback_toggled = play_button.clicked;
    const Rectangle loop_button_rect = {play_rect.x + play_rect.width + 8.0f, play_rect.y, 42.0f, 42.0f};
    const ui::button_state loop_button = draw_icon_button(loop_button_rect, raythm_icons::draw_repeat_2,
                                                          model.loop_enabled, t.success);
    result.loop_toggled = loop_button.clicked;

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

void draw_timeline(const editor_timeline_presenter_model& presenter_model) {
    const auto& t = *g_theme;
    const editor_timeline_view_model model = make_timeline_model(presenter_model);
    const Rectangle panel = model.metrics.panel_rect;
    const Rectangle content = model.metrics.content_rect();
    const Rectangle track = model.metrics.scrollbar_track_rect();
    const std::set<size_t> selected_indices(model.selected_note_indices.begin(), model.selected_note_indices.end());

    ui::draw_rect_f(panel, panel_tint(t.panel, t.bg_alt, 0.12f));
    ui::draw_rect_lines(panel, 1.5f, t.border);

    const Rectangle arrange = content;
    const Rectangle minimap = track;
    const Rectangle ruler = {track.x + track.width + 8.0f, arrange.y, 60.0f, arrange.height};
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

        if (model.loop_end_tick > model.loop_start_tick &&
            model.loop_end_tick >= model.min_tick && model.loop_start_tick <= model.max_tick) {
            const float start_y = model.metrics.tick_to_y(model.loop_start_tick);
            const float end_y = model.metrics.tick_to_y(model.loop_end_tick);
            const Rectangle loop = {arrange.x, std::min(start_y, end_y), arrange.width,
                                    std::max(8.0f, std::fabs(end_y - start_y))};
            ui::draw_rect_f(loop, with_alpha(t.success, model.loop_enabled ? 48 : 20));
            ui::draw_rect_lines(loop, model.loop_enabled ? 2.0f : 1.0f,
                                with_alpha(t.success, model.loop_enabled ? 220 : 130));
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

        for (size_t index = 0; index < model.scroll_events.size(); ++index) {
            const editor_timeline_scroll_event& event = model.scroll_events[index];
            if (event.duration <= 0 || event.tick > model.max_tick || event.tick + event.duration < model.min_tick) {
                continue;
            }
            const float start_y = model.metrics.tick_to_y(event.tick);
            const float end_y = model.metrics.tick_to_y(event.tick + event.duration);
            const bool selected = model.selected_scroll_event_index.has_value() &&
                                  *model.selected_scroll_event_index == index;
            const Color tone = event.type == scroll_event_type::speed ? t.fast : t.error;
            const Rectangle band = {arrange.x, std::min(start_y, end_y), arrange.width,
                                    std::max(8.0f, std::fabs(end_y - start_y))};
            ui::draw_rect_f(band, with_alpha(tone, selected ? 92 : 42));
            ui::draw_rect_lines(band, selected ? 2.0f : 1.0f, with_alpha(tone, selected ? 230 : 140));
            ui::draw_text_in_rect(event.type == scroll_event_type::speed
                                      ? TextFormat("Speed %.2fx", event.multiplier)
                                      : "Stop",
                                  13,
                                  {band.x + band.width - 130.0f, band.y + 4.0f, 118.0f, 18.0f},
                                  selected ? t.text : t.text_secondary, ui::text_align::right);
        }

        const std::set<size_t> preview_indices(model.preview_note_indices.begin(), model.preview_note_indices.end());
        for (size_t index = 0; index < model.notes.size(); ++index) {
            if (preview_indices.find(index) != preview_indices.end()) {
                continue;
            }
            const editor_timeline_note& note = model.notes[index];
            if (note.lane < 0 || note.lane >= model.metrics.key_count) {
                continue;
            }
            const editor_timeline_note_draw_info info = model.metrics.note_rects(note);
            const bool selected = selected_indices.find(index) != selected_indices.end() ||
                                  (model.selected_note_index.has_value() && *model.selected_note_index == index);
            draw_note_block(note, info, selected, false, false);
        }

        for (const editor_timeline_note& preview_note : model.preview_notes) {
            const editor_timeline_note_draw_info info = model.metrics.note_rects(preview_note);
            draw_note_block(preview_note, info, true, true, model.preview_has_overlap);
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

    const float viewport_ratio = model.content_height_pixels <= 1.0f
        ? 1.0f
        : std::clamp(content.height / model.content_height_pixels, 0.06f, 1.0f);
    const float viewport_height = std::max(36.0f, minimap.height * viewport_ratio);
    const float max_scroll = std::max(1.0f, model.content_height_pixels - content.height);
    const float viewport_y = minimap.y + (minimap.height - viewport_height) *
        std::clamp(model.scroll_offset_pixels / max_scroll, 0.0f, 1.0f);
    draw_chart_minimap(model, minimap, {minimap.x + 3.0f, viewport_y, minimap.width - 6.0f, viewport_height});

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
