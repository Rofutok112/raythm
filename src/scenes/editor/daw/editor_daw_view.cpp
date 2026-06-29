#include "editor/daw/editor_daw_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "editor/daw/editor_daw_automation_view.h"
#include "editor/editor_timeline_types.h"
#include "editor/view/editor_layout.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_scroll.h"
#include "ui_text_input.h"
#include "ui/icons/raythm_icons.h"
#include "ui_layout.h"
#include "ui_tooltip.h"
#include "virtual_screen.h"

namespace {
namespace layout = editor::layout;

constexpr float kPanelInset = 14.0f;
constexpr float kAutomationWidth = 380.0f;

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

struct daw_left_panel_layout {
    Rectangle panel = {};
    Rectangle content = {};
    Rectangle title = {};
    Rectangle status_badge = {};
    Rectangle level_badge = {};
    Rectangle palette = {};
    Rectangle palette_title = {};
    Rectangle palette_subtitle = {};
    std::array<Rectangle, 5> note_pads = {};
    Rectangle ray_row = {};
    Rectangle load_error = {};
};

struct daw_right_panel_layout {
    Rectangle panel = {};
    Rectangle content = {};
    Rectangle title = {};
    Rectangle graph_box = {};
    Rectangle graph_title = {};
    Rectangle graph = {};
    Rectangle bar_axis_label = {};
    std::array<Rectangle, 3> automation_buttons = {};
    Rectangle inspector = {};
    Rectangle inspector_title = {};
    Rectangle inspector_bar = {};
    Rectangle inspector_rate = {};
    Rectangle inspector_curve = {};
    Rectangle inspector_empty = {};
};

daw_left_panel_layout daw_left_panel_layout_for() {
    daw_left_panel_layout layout_model;
    layout_model.panel = layout::kLeftPanelRect;
    layout_model.content = inset_rect(layout_model.panel, kPanelInset);
    layout_model.title = {layout_model.content.x, layout_model.content.y, layout_model.content.width, 20.0f};
    layout_model.status_badge = {layout_model.content.x, layout_model.content.y + 62.0f, 95.0f, 24.0f};
    layout_model.level_badge = {layout_model.content.x + 103.0f, layout_model.content.y + 62.0f, 76.0f, 24.0f};
    layout_model.palette = {
        layout_model.content.x,
        layout_model.content.y + 112.0f,
        layout_model.content.width,
        384.0f
    };
    layout_model.palette_title = {
        layout_model.palette.x + 12.0f,
        layout_model.palette.y + 10.0f,
        layout_model.palette.width - 24.0f,
        24.0f
    };
    layout_model.palette_subtitle = {
        layout_model.palette.x + 12.0f,
        layout_model.palette.y + 36.0f,
        layout_model.palette.width - 24.0f,
        18.0f
    };
    constexpr float pad_height = 44.0f;
    constexpr float pad_gap = 8.0f;
    ui::vstack({
        layout_model.palette.x + 12.0f,
        layout_model.palette.y + 62.0f,
        layout_model.palette.width - 24.0f,
        pad_height * static_cast<float>(layout_model.note_pads.size()) +
            pad_gap * static_cast<float>(layout_model.note_pads.size() - 1),
    }, pad_height, pad_gap, layout_model.note_pads);
    const Rectangle last_pad = layout_model.note_pads.back();
    layout_model.ray_row = {
        last_pad.x,
        last_pad.y + last_pad.height + pad_gap + 8.0f,
        last_pad.width,
        pad_height
    };
    layout_model.load_error = {
        layout_model.content.x,
        layout_model.content.y + layout_model.content.height - 58.0f,
        layout_model.content.width,
        52.0f
    };
    return layout_model;
}

daw_right_panel_layout daw_right_panel_layout_for() {
    daw_right_panel_layout layout_model;
    layout_model.panel = layout::kRightPanelRect;
    layout_model.content = inset_rect(layout_model.panel, kPanelInset);
    layout_model.title = {layout_model.content.x, layout_model.content.y, layout_model.content.width, 20.0f};
    layout_model.graph_box = {
        layout_model.content.x,
        layout_model.content.y + 42.0f,
        layout_model.content.width,
        454.0f
    };
    layout_model.graph_title = {
        layout_model.graph_box.x + 12.0f,
        layout_model.graph_box.y + 10.0f,
        layout_model.graph_box.width - 24.0f,
        24.0f
    };
    layout_model.graph = {
        layout_model.graph_box.x + 12.0f,
        layout_model.graph_box.y + 52.0f,
        layout_model.graph_box.width - 24.0f,
        layout_model.graph_box.height - 92.0f
    };
    layout_model.bar_axis_label = {
        layout_model.graph_box.x + 8.0f,
        layout_model.graph.y - 20.0f,
        36.0f,
        18.0f
    };
    ui::hstack_fill({
        layout_model.graph_box.x + 12.0f,
        layout_model.graph_box.y + layout_model.graph_box.height - 34.0f,
        layout_model.graph_box.width - 24.0f,
        26.0f,
    }, 8.0f, layout_model.automation_buttons);
    const float inspector_top = layout_model.graph_box.y + layout_model.graph_box.height + 14.0f;
    layout_model.inspector = {
        layout_model.content.x,
        inspector_top,
        layout_model.content.width,
        layout_model.content.y + layout_model.content.height - inspector_top
    };
    layout_model.inspector_title = {
        layout_model.inspector.x + 12.0f,
        layout_model.inspector.y + 10.0f,
        layout_model.inspector.width - 24.0f,
        24.0f
    };
    layout_model.inspector_bar = {
        layout_model.inspector.x + 12.0f,
        layout_model.inspector.y + 48.0f,
        layout_model.inspector.width - 24.0f,
        22.0f
    };
    layout_model.inspector_rate = {
        layout_model.inspector.x + 12.0f,
        layout_model.inspector.y + 76.0f,
        layout_model.inspector.width - 24.0f,
        22.0f
    };
    layout_model.inspector_curve = {
        layout_model.inspector.x + 12.0f,
        layout_model.inspector.y + 104.0f,
        layout_model.inspector.width - 24.0f,
        22.0f
    };
    layout_model.inspector_empty = {
        layout_model.inspector.x + 12.0f,
        layout_model.inspector.y + 54.0f,
        layout_model.inspector.width - 24.0f,
        22.0f
    };
    return layout_model;
}

Vector2 rect_center(Rectangle rect) {
    return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
}

ui::button_state draw_icon_button(Rectangle rect,
                                  void (*draw_icon)(Rectangle, Color, float),
                                  bool active,
                                  Color active_color) {
    const auto& t = *g_theme;
    return ui::icon_button(rect, draw_icon, {
        .border_width = active ? 2.2f : 1.2f,
        .bg = active ? panel_tint(t.row_selected, active_color, 0.14f) : t.row,
        .bg_hover = active ? panel_tint(t.row_active, active_color, 0.18f) : t.row_hover,
        .icon_color = active ? active_color : t.text_secondary,
        .icon_hover_color = active ? active_color : t.text_secondary,
        .icon_inset = 9.0f,
        .icon_stroke_width = 3.2f,
        .icon_pressed_inset = 0.0f,
    });
}

void draw_micro_label(Rectangle rect, const char* label, Color color) {
    ui::draw_text_in_rect(label, 12, rect, color, ui::text_align::left);
}

void draw_badge(Rectangle rect, const char* label, Color border, Color text) {
    ui::surface(rect, with_alpha(border, 28), with_alpha(border, 190), 1.0f);
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
    return ui::button(rect, label, {
        .layer = layer,
        .font_size = font_size,
        .border_width = border_width,
        .bg = bg,
        .bg_hover = bg_hover,
        .text_color = text_color,
        .custom_colors = true,
    });
}

ui::row_state draw_layer_row(Rectangle rect,
                             bool selected,
                             ui::draw_layer layer,
                             Color selected_tone) {
    const auto& t = *g_theme;
    return ui::row(rect, {
        .layer = layer,
        .border_width = selected ? 2.0f : 1.0f,
        .bg = selected ? panel_tint(t.row_selected, selected_tone, 0.16f) : t.row,
        .bg_hover = selected ? panel_tint(t.row_selected, selected_tone, 0.16f) : t.row_hover,
        .border_color = selected ? selected_tone : t.border_light,
        .custom_colors = true,
    });
}

enum class timing_modal_action {
    add_bpm,
    add_meter,
    delete_selected,
};

std::array<ui::action_button_definition<timing_modal_action>, 3> timing_modal_action_buttons_for(
    std::span<const Rectangle, 3> rects,
    bool delete_enabled) {
    return {{
        {rects[0], "Add BPM", timing_modal_action::add_bpm, true},
        {rects[1], "Add Meter", timing_modal_action::add_meter, true},
        {rects[2], "Delete", timing_modal_action::delete_selected, delete_enabled},
    }};
}

void apply_timing_modal_action(editor_timing_panel_result& result, timing_modal_action action) {
    switch (action) {
        case timing_modal_action::add_bpm:
            result.add_bpm = true;
            break;
        case timing_modal_action::add_meter:
            result.add_meter = true;
            break;
        case timing_modal_action::delete_selected:
            result.delete_selected = true;
            break;
    }
}

void draw_timing_modal_action_buttons(const std::array<ui::action_button_definition<timing_modal_action>, 3>& buttons,
                                      editor_timing_panel_result& result) {
    const auto& t = *g_theme;
    const auto action = ui::draw_action_buttons<timing_modal_action>(buttons, {
        .layer = ui::draw_layer::modal,
        .font_size = 13,
        .border_width = 1.5f,
        .disabled_bg = t.section,
        .disabled_bg_hover = t.section,
        .disabled_text_color = t.text_hint,
        .disabled_border_color = t.border,
    });
    if (action.has_value()) {
        apply_timing_modal_action(result, *action);
    }
}

enum class scroll_panel_action {
    add_point,
    cycle_curve,
    delete_selected,
};

std::array<ui::action_button_definition<scroll_panel_action>, 3> scroll_panel_action_buttons_for(
    std::span<const Rectangle, 3> rects,
    bool selection_actions_enabled) {
    return {{
        {rects[0], "Add Point", scroll_panel_action::add_point, true},
        {rects[1], "Curve", scroll_panel_action::cycle_curve, selection_actions_enabled},
        {rects[2], "Delete", scroll_panel_action::delete_selected, selection_actions_enabled},
    }};
}

void apply_scroll_panel_action(editor_right_panel_view_result& result,
                               scroll_panel_action action,
                               int max_tick) {
    switch (action) {
        case scroll_panel_action::add_point:
            result.scroll_automation_point_to_add =
                scroll_automation_point{std::clamp(max_tick / 2, 0, max_tick), 1.0f, scroll_automation_curve::linear};
            break;
        case scroll_panel_action::cycle_curve:
            result.panel_result.cycle_selected_scroll_curve = true;
            break;
        case scroll_panel_action::delete_selected:
            result.panel_result.delete_selected_scroll = true;
            break;
    }
}

void draw_scroll_panel_action_buttons(const std::array<ui::action_button_definition<scroll_panel_action>, 3>& buttons,
                                      int max_tick,
                                      editor_right_panel_view_result& result) {
    const auto& t = *g_theme;
    const auto action = ui::draw_action_buttons<scroll_panel_action>(buttons, {
        .font_size = 13,
        .border_width = 1.4f,
        .disabled_text_color = t.text_hint,
        .disabled_border_color = t.border,
    });
    if (action.has_value()) {
        apply_scroll_panel_action(result, *action, max_tick);
    }
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

void draw_timing_modal_pick_row(Rectangle rect,
                                const char* label,
                                const std::string& value,
                                editor_timing_input_field field,
                                editor_timing_panel_state& state,
                                editor_timing_panel_result& result) {
    const auto& t = *g_theme;
    const bool selected = state.active_input_field == field || state.bar_pick_mode;
    const ui::row_state row_state = draw_layer_row(rect, selected, ui::draw_layer::modal, t.accent);
    if (row_state.clicked) {
        result.clicked_input_row = true;
        state.active_input_field = field;
        state.bar_pick_mode = true;
        state.input_error.clear();
        state.inputs.bpm_value.active = false;
        state.inputs.meter_numerator.active = false;
        state.inputs.meter_denominator.active = false;
    }
    ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 12.0f)),
                         label, state.bar_pick_mode ? "Pick timeline" : value.c_str(),
                         16, selected ? t.text : t.text_secondary,
                         state.bar_pick_mode ? t.accent : t.text, 82.0f);
}

void draw_timing_modal_input_row(Rectangle rect,
                                 const char* label,
                                 ui::text_input_state& input,
                                 editor_timing_input_field field,
                                 ui::text_input_filter filter,
                                 const char* placeholder,
                                 float label_width,
                                 editor_timing_panel_state& state,
                                 editor_timing_panel_result& result) {
    const ui::text_input_result input_result = ui::text_input(
        rect, input, label, placeholder, {
            .layer = ui::draw_layer::modal,
            .font_size = 16,
            .max_length = 16,
            .filter = filter,
            .label_width = label_width,
        });
    if (input_result.clicked) {
        result.clicked_input_row = true;
        set_active_timing_input(state, field);
        state.bar_pick_mode = false;
        state.input_error.clear();
    }
    if (input_result.submitted) {
        result.apply_selected = true;
        set_active_timing_input(state, editor_timing_input_field::none);
        state.bar_pick_mode = false;
    } else if (input_result.deactivated && state.active_input_field == field) {
        set_active_timing_input(state, editor_timing_input_field::none);
    }
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
    const ui::row_state state = ui::row(rect, {
        .border_width = selected ? 2.0f : 1.0f,
        .bg = selected ? panel_tint(t.row_selected, tone, 0.18f) : t.row,
        .bg_hover = selected ? panel_tint(t.row_active, tone, 0.2f) : t.row_hover,
        .border_color = selected ? tone : t.border_light,
        .custom_colors = true,
    });
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
    const ui::row_state state = ui::row(rect, {
        .border_width = enabled ? 2.0f : 1.0f,
        .bg = enabled ? panel_tint(t.row_selected, ray_tone, 0.2f) : t.row,
        .bg_hover = enabled ? panel_tint(t.row_active, ray_tone, 0.24f) : t.row_hover,
        .border_color = enabled ? ray_tone : t.border_light,
        .custom_colors = true,
    });
    const Rectangle label_rect = {state.visual.x + 12.0f, state.visual.y, state.visual.width - 96.0f,
                                  state.visual.height};
    ui::draw_text_in_rect("Ray", 15, label_rect, enabled ? t.text : t.text_secondary, ui::text_align::left);
    const Rectangle track = {state.visual.x + state.visual.width - 78.0f,
                             state.visual.y + state.visual.height * 0.5f - 10.0f,
                             54.0f, 20.0f};
    ui::surface(track,
                enabled ? with_alpha(ray_tone, 165) : with_alpha(t.text_muted, 70),
                enabled ? ray_tone : t.border_light,
                1.0f);
    const float knob_x = enabled ? track.x + track.width - 18.0f : track.x + 2.0f;
    ui::surface_fill({knob_x, track.y + 2.0f, 16.0f, 16.0f}, enabled ? t.text : t.text_secondary);
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
        ui::surface_fill(strip, color_at(base, t));
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
    ui::horizontal_gradient({rect.x, rect.y, rect.width, rim_height}, frame_left, frame_right);
    ui::horizontal_gradient({rect.x, rect.y + rect.height - rim_height, rect.width, rim_height},
                            frame_left, frame_right);
    ui::vertical_gradient({rect.x, rect.y, side_width, rect.height}, frame_near, frame_far);
    ui::vertical_gradient({rect.x + rect.width - side_width, rect.y, side_width, rect.height},
                          frame_near, frame_far);
    if (selected) {
        ui::frame(ui::inset(rect, -2.0f), g_theme->accent, 2.0f);
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
    ui::horizontal_gradient({rect.x, rect.y, rect.width, cap_height}, cap_left, cap_right);
    ui::horizontal_gradient({rect.x, rect.y + rect.height - cap_height, rect.width, cap_height},
                            cap_left, cap_right);
    if (selected) {
        ui::frame(ui::inset(rect, -2.0f), g_theme->accent, 2.0f);
    }
}

void draw_editor_decorative_hold_body(Rectangle rect, Color fill, bool ray_style, bool selected) {
    const Color decor_base = ray_style
                                 ? lerp_color(fill, {194, 156, 255, 255}, 0.62f)
                                 : lerp_color(fill, {86, 220, 232, 255}, 0.64f);
    draw_horizontal_strip_gradient(rect, 18, editor_decorative_hold_gradient_color, decor_base);

    const Color rail = with_alpha(lerp_color(decor_base, WHITE, 0.30f), 150);
    const float rail_width = std::clamp(rect.width * 0.035f, 1.5f, 4.0f);
    ui::vertical_gradient({rect.x, rect.y, rail_width, rect.height}, rail, with_alpha(rail, 72));
    ui::vertical_gradient({rect.x + rect.width - rail_width, rect.y, rail_width, rect.height},
                          rail, with_alpha(rail, 72));
    if (selected) {
        ui::frame(ui::inset(rect, -2.0f), g_theme->accent, 2.0f);
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
    ui::vertical_gradient(left_cap, end_inner, end_edge);
    ui::vertical_gradient(right_cap, end_inner, end_edge);
    if (selected) {
        ui::frame(ui::inset(bar, -2.0f), g_theme->accent, 2.0f);
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

    ui::draw_triangle(left_outer_bottom, left_outer_top, center_top, marker);
    ui::draw_triangle(left_outer_bottom, center_top, center_bottom, marker);
    ui::draw_triangle(center_bottom, center_top, right_outer_top, marker);
    ui::draw_triangle(center_bottom, right_outer_top, right_outer_bottom, marker);

    const float line_width = std::clamp(height * 0.10f, 1.6f, 2.8f);
    ui::draw_line_ex(left_outer_bottom, left_outer_top, line_width, contour);
    ui::draw_line_ex(left_outer_top, center_top, line_width, contour);
    ui::draw_line_ex(center_top, right_outer_top, line_width, contour);
    ui::draw_line_ex(right_outer_top, right_outer_bottom, line_width, contour);
    ui::draw_line_ex(right_outer_bottom, center_bottom, line_width, contour);
    ui::draw_line_ex(center_bottom, left_outer_bottom, line_width, contour);
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
    ui::surface_fill(rect, color);
    if (selected || preview) {
        ui::frame(ui::inset(rect, selected ? -1.5f : 0.0f),
                  overlap ? t.error : (selected ? t.accent : t.success),
                  selected ? 2.0f : 1.0f);
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
        ui::max_scroll_offset(model.content_height_pixels, model.metrics.content_rect()) * model.metrics.ticks_per_pixel;
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
        ui::max_scroll_offset(model.content_height_pixels, model.metrics.content_rect()) * model.metrics.ticks_per_pixel;

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
    ui::surface(minimap, with_alpha(t.section, 235), t.border_light, 1.0f);

    const Rectangle inner = ui::inset(minimap, 4.0f);
    {
        ui::scoped_clip_rect clip_scope(inner);
        const minimap_shape_cache& cache = cached_minimap_shapes(model, inner, t);
        for (const auto& body : cache.bodies) {
            ui::surface_fill(body.first, body.second);
        }
        for (const auto& marker : cache.markers) {
            ui::surface_fill(marker.first, marker.second);
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
    ui::surface(clipped_box, with_alpha(t.accent, 36), with_alpha(t.accent, 220), 2.0f);
}

}  // namespace

namespace editor::daw {

editor_left_panel_view_result draw_left_panel(const editor_left_panel_view_model& model) {
    const auto& t = *g_theme;
    editor_left_panel_view_result result;
    const daw_left_panel_layout panel_layout = daw_left_panel_layout_for();
    const Rectangle panel = panel_layout.panel;
    const Rectangle content = panel_layout.content;
    const char* status_label = model.is_dirty ? "MODIFIED" : (model.has_file ? "SAVED" : "UNSAVED");

    ui::surface(panel, panel_tint(t.panel, t.bg_alt, 0.18f), t.border, 1.5f);

    ui::draw_text_in_rect("CHART", 15, panel_layout.title, t.text_muted, ui::text_align::left);
    draw_marquee_text(model.song_title, content.x, content.y + 26.0f, 24, t.text, content.width, model.now);
    draw_badge(panel_layout.status_badge, status_label,
               model.is_dirty ? t.slow : t.success, model.is_dirty ? t.slow : t.success);
    draw_difficulty_level_badge(model.level,
                                panel_layout.level_badge,
                                13, 255);

    const Rectangle palette = panel_layout.palette;
    ui::section(palette);
    ui::draw_text_in_rect("Tool", 22, panel_layout.palette_title, t.text, ui::text_align::left);
    ui::draw_text_in_rect("Place notes and toggle ray notes.",
                          13,
                          panel_layout.palette_subtitle,
                          t.text_muted, ui::text_align::left);
    const std::array<note_type, 5> note_types{
        note_type::tap,
        note_type::hold,
        note_type::release,
        note_type::stay,
        note_type::decorative_hold,
    };
    for (size_t i = 0; i < note_types.size(); ++i) {
        draw_palette_pad(panel_layout.note_pads[i], note_types[i], model.note_palette, result);
    }

    result.ray_toggled = draw_ray_toggle(
        panel_layout.ray_row, model.note_palette.is_ray);

    if (model.load_error != nullptr) {
        ui::draw_text_in_rect(model.load_error->c_str(), 16, panel_layout.load_error,
                              t.error, ui::text_align::left);
    }

    return result;
}

editor_right_panel_view_result draw_right_panel(const editor_right_panel_view_model& model,
                                                editor_timing_panel_state& timing_state) {
    const auto& t = *g_theme;
    editor_right_panel_view_result result;
    const daw_right_panel_layout panel_layout = daw_right_panel_layout_for();
    const Rectangle panel = panel_layout.panel;

    const std::vector<scroll_automation_point> empty_points;
    const std::vector<scroll_automation_point>& points =
        model.scroll_automation != nullptr ? *model.scroll_automation : empty_points;
    const scroll_automation_guides scroll_guides =
        model.scroll_guides != nullptr ? *model.scroll_guides : scroll_automation_guides{};

    ui::surface(panel, panel_tint(t.panel, t.bg_alt, 0.14f), t.border, 1.5f);
    ui::draw_text_in_rect("SCROLL", 15, panel_layout.title, t.text_muted, ui::text_align::left);

    const Rectangle graph_box = panel_layout.graph_box;
    ui::section(graph_box);
    ui::draw_text_in_rect("Velocity Curve", 20, panel_layout.graph_title, t.text, ui::text_align::left);
    const Rectangle graph = panel_layout.graph;
    const right_panel_automation_graph_view_result graph_result = draw_right_panel_automation_graph_view({
        .graph = graph,
        .points = &points,
        .scroll_guides = scroll_guides,
        .selected_scroll_event_index = model.selected_scroll_event_index,
        .mouse = model.mouse,
    }, timing_state);
    const int max_tick = graph_result.max_tick;
    if (graph_result.actions.panel_result.selected_scroll_event_index.has_value()) {
        result.panel_result.selected_scroll_event_index =
            graph_result.actions.panel_result.selected_scroll_event_index;
    }
    if (graph_result.actions.scroll_automation_point_to_add.has_value()) {
        result.scroll_automation_point_to_add = graph_result.actions.scroll_automation_point_to_add;
    }
    if (graph_result.actions.scroll_automation_point_to_modify.has_value()) {
        result.scroll_automation_point_to_modify = graph_result.actions.scroll_automation_point_to_modify;
    }
    ui::draw_text_in_rect("BAR", 11, panel_layout.bar_axis_label, t.text_muted, ui::text_align::right);

    draw_scroll_panel_action_buttons(
        scroll_panel_action_buttons_for(panel_layout.automation_buttons, model.scroll_delete_enabled),
        max_tick,
        result);

    const Rectangle inspector = panel_layout.inspector;
    ui::section(inspector);
    ui::draw_text_in_rect("Point", 20, panel_layout.inspector_title, t.text, ui::text_align::left);
    if (model.selected_scroll_event_index.has_value() &&
        *model.selected_scroll_event_index < points.size()) {
        const scroll_automation_point& point = points[*model.selected_scroll_event_index];
        ui::draw_label_value(panel_layout.inspector_bar,
                             "Bar", model.meter_map->bar_beat_label(point.tick).c_str(), 15,
                             t.text_secondary, t.text, 72.0f);
        ui::draw_label_value(panel_layout.inspector_rate,
                             "Rate", TextFormat("%.2fx", point.multiplier), 15,
                             t.text_secondary, t.text, 72.0f);
        ui::draw_label_value(panel_layout.inspector_curve,
                             "Curve", scroll_curve_label(point.curve_to_next), 15,
                             t.text_secondary, t.text, 72.0f);
    } else {
        ui::draw_text_in_rect("Click the graph to create a point.", 15,
                              panel_layout.inspector_empty,
                              t.text_hint, ui::text_align::left);
    }

    const std::array<Rectangle, 2> editor_regions = {{
        graph_box,
        inspector,
    }};
    result.clicked_outside_editor =
        ui::is_mouse_button_pressed_outside(std::span<const Rectangle>(editor_regions), model.mouse);
    return result;
}

editor_header_view_result draw_header(const editor_header_view_model& model, Rectangle snap_menu_rect) {
    const auto& t = *g_theme;
    editor_header_view_result result;
    const Rectangle bar = layout::kHeaderRect;
    const Rectangle content = inset_rect(bar, 10.0f);

    ui::surface(bar, panel_tint(t.panel, t.bg_alt, 0.18f), t.border, 1.5f);
    draw_icon_button(layout::kBackButtonRect, raythm_icons::draw_chevron_left, false, t.text);
    draw_icon_button(layout::kSettingsButtonRect, raythm_icons::draw_settings_gear, false, t.text);

    const ui::rect_pair header_modal_buttons = ui::split_columns(
        {content.x + 168.0f, content.y + 8.0f, 188.0f, 34.0f},
        86.0f,
        8.0f);
    const Rectangle meta_button = header_modal_buttons.first;
    const Rectangle timing_button = header_modal_buttons.second;
    result.metadata_modal_requested = ui::button(meta_button, "META", {
        .font_size = 13,
        .border_width = 1.2f,
        .bg = t.row,
        .bg_hover = t.row_hover,
        .text_color = t.text_secondary,
        .custom_colors = true,
    }).clicked;
    result.timing_modal_requested = ui::button(timing_button, "TIMING", {
        .font_size = 13,
        .border_width = 1.2f,
        .bg = t.row,
        .bg_hover = t.row_hover,
        .text_color = t.text_secondary,
        .custom_colors = true,
    }).clicked;

    const Rectangle transport = layout::kHeaderTransportRect;
    ui::section(transport);
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
    const ui::dropdown_state dropdown = ui::queued_dropdown(
        layout::kSnapDropdownRect, snap_menu_rect,
        "Snap", model.snap_labels[model.snap_index],
        model.snap_labels,
        model.snap_index, model.snap_dropdown_open, {
            .trigger_layer = ui::draw_layer::base,
            .menu_layer = ui::draw_layer::overlay,
            .font_size = 14,
            .label_width = 58.0f,
        });
    result.snap_dropdown_toggled = dropdown.trigger.clicked;
    result.snap_index_clicked = dropdown.clicked_index;
    result.snap_dropdown_close_requested =
        model.snap_dropdown_open && ui::is_mouse_button_released() &&
        !ui::is_hovered(layout::kSnapDropdownRect, ui::draw_layer::base) &&
        !ui::is_hovered(snap_menu_rect, ui::draw_layer::overlay);
    return result;
}

editor_right_panel_view_result draw_timeline(const editor_timeline_presenter_model& presenter_model,
                                             Rectangle snap_menu_rect,
                                             bool snap_dropdown_open,
                                             editor_timing_panel_state& timing_state) {
    const auto& t = *g_theme;
    editor_right_panel_view_result result;
    const editor_timeline_view_model model = make_timeline_model(presenter_model);
    const Rectangle panel = model.metrics.panel_rect;
    const Rectangle content = model.metrics.content_rect();
    const Rectangle track = model.metrics.scrollbar_track_rect();
    auto contains_sorted_index = [](const std::vector<size_t>& indices, size_t index) {
        return std::binary_search(indices.begin(), indices.end(), index);
    };

    ui::surface(panel, panel_tint(t.panel, t.bg_alt, 0.12f), t.border, 1.5f);

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
            ui::surface(lane_rect,
                        lane % 2 == 0 ? with_alpha(t.row, 28) : with_alpha(t.section, 36),
                        with_alpha(t.border_light, 150),
                        1.0f);
        }

        const int snap_interval = std::max(1, model.snap_interval);
        const int first_snap_tick = std::max(0, (model.min_tick / snap_interval) * snap_interval);
        for (int tick = first_snap_tick; tick <= model.max_tick; tick += snap_interval) {
            const float y = model.metrics.tick_to_y(tick);
            if (ui::y_visible_in_viewport(y, arrange)) {
                ui::draw_line_f(arrange.x, y, arrange.x + arrange.width, y, with_alpha(t.editor_grid_snap, 165));
            }
        }

        for (const editor_meter_map::grid_line& line : model.grid_lines) {
            const float y = model.metrics.tick_to_y(line.tick);
            if (!ui::y_visible_in_viewport(y, arrange)) {
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
            ui::surface(rect, with_alpha(t.accent, 32), with_alpha(t.accent, 220), 1.5f);
        }

        if (model.playback_tick.has_value()) {
            const float y = model.metrics.tick_to_y(*model.playback_tick);
            ui::draw_line_ex({arrange.x, y}, {arrange.x + arrange.width, y}, 3.0f, t.accent);
        }
    }

    draw_chart_minimap(model, minimap);

    result = draw_timeline_automation_view({
        .panel = automation,
        .timeline = &model,
        .scroll_guides = presenter_model.state.data().scroll_guides,
        .snap_menu_rect = snap_menu_rect,
        .snap_dropdown_open = snap_dropdown_open,
        .mouse = virtual_screen::get_virtual_mouse(),
    }, timing_state);

    ui::surface(ruler, with_alpha(t.section, 235), t.border_light, 1.0f);
    draw_waveform(model, ui::inset(ruler, 4.0f));
    ui::draw_text_in_rect("BAR", 11, {ruler.x, ruler.y + 8.0f, ruler.width, 16.0f},
                          t.text_muted);
    for (const editor_meter_map::grid_line& line : model.grid_lines) {
        if (!line.major) {
            continue;
        }
        const float y = model.metrics.tick_to_y(line.tick);
        if (!ui::y_visible_in_viewport(y, ruler_labels)) {
            continue;
        }
        const Rectangle tag = {ruler.x + 6.0f, y - 11.0f, ruler.width - 12.0f, 22.0f};
        ui::surface(tag, with_alpha(t.panel, 225), with_alpha(t.border_light, 190), 1.0f);
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

    ui::backdrop(layout::kScreenRect, g_theme->pause_overlay);
    ui::panel(modal);
    ui::draw_text_in_rect("Chart Metadata", 28,
                          {content.x, content.y, content.width, 32.0f},
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("These settings are outside the main editing surface.",
                          15,
                          {content.x, content.y + 34.0f, content.width, 22.0f},
                          t.text_muted, ui::text_align::left);

    const Rectangle song_box = {content.x, content.y + 76.0f, content.width, 72.0f};
    ui::section(song_box);
    ui::draw_label_value({song_box.x + 14.0f, song_box.y + 13.0f, song_box.width - 28.0f, 22.0f},
                         "Song", model.song_title, 16, t.text_muted, t.text, 78.0f);
    ui::draw_label_value({song_box.x + 14.0f, song_box.y + 40.0f, song_box.width - 28.0f, 20.0f},
                         "Status", model.is_dirty ? "Modified" : (model.has_file ? "Saved" : "Unsaved"),
                         14, t.text_muted, model.is_dirty ? t.slow : t.success, 78.0f);

    const Rectangle form = {content.x, song_box.y + song_box.height + 16.0f, content.width, 148.0f};
    ui::section(form);
    result.metadata_result.difficulty_result = ui::text_input(
        {form.x + 16.0f, form.y + 18.0f, form.width - 32.0f, 38.0f},
        metadata_panel.difficulty_input, "Diff", "Difficulty", {
            .default_value = "New",
            .layer = ui::draw_layer::modal,
            .font_size = 16,
            .max_length = 24,
            .filter = accepts_metadata_character,
            .label_width = 74.0f,
        });
    result.metadata_result.author_result = ui::text_input(
        {form.x + 16.0f, form.y + 66.0f, form.width - 32.0f, 38.0f},
        metadata_panel.chart_author_input, "Author", "Chart author", {
            .default_value = "Unknown",
            .layer = ui::draw_layer::modal,
            .font_size = 16,
            .max_length = 32,
            .filter = accepts_metadata_character,
            .label_width = 74.0f,
        });
    const ui::selector_state key_count_selector = ui::value_selector(
        {form.x + 16.0f, form.y + 114.0f, form.width - 32.0f, 28.0f},
        "Lanes", key_count_label(metadata_panel.key_count), {
            .layer = ui::draw_layer::modal,
            .font_size = 14,
            .button_size = 24.0f,
            .label_width = 74.0f,
            .content_padding = 10.0f,
        });
    result.metadata_result.key_count_left_clicked = key_count_selector.left.clicked;
    result.metadata_result.key_count_right_clicked = key_count_selector.right.clicked;

    if (!metadata_panel.error.empty()) {
        ui::draw_text_in_rect(metadata_panel.error.c_str(), 15,
                              {content.x, form.y + form.height + 10.0f, content.width, 22.0f},
                              t.error, ui::text_align::left);
    }

    const Rectangle unlock_rect = {content.x, modal.y + modal.height - 58.0f, 168.0f, 34.0f};
    const ui::rect_pair footer_actions = ui::split_columns(
        {content.x + content.width - 260.0f, modal.y + modal.height - 58.0f, 260.0f, 34.0f},
        116.0f,
        12.0f);
    const Rectangle apply_rect = footer_actions.first;
    const Rectangle close_rect = footer_actions.second;
    result.unlock_rules_requested = draw_layer_button(unlock_rect, "UNLOCK RULES", 14, ui::draw_layer::modal,
                                                      t.row, t.row_hover, t.text).clicked;
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

    ui::backdrop(layout::kScreenRect, g_theme->pause_overlay);
    ui::panel(modal);
    ui::draw_text_in_rect("Timing Map", 28,
                          {content.x, content.y, content.width, 32.0f},
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("BPM and meter changes are managed here.",
                          15,
                          {content.x, content.y + 34.0f, content.width, 22.0f},
                          t.text_muted, ui::text_align::left);
    const ui::selector_state chart_offset = ui::value_selector(
        {content.x + content.width - 270.0f, content.y + 8.0f, 270.0f, 36.0f},
        "Offset", offset_label, {
            .layer = ui::draw_layer::modal,
            .font_size = 14,
            .button_size = 24.0f,
            .label_width = 54.0f,
            .content_padding = 10.0f,
        });
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

    const ui::rect_pair timing_columns = ui::split_columns(
        {content.x, content.y + 74.0f, content.width, content.height - 142.0f},
        390.0f,
        18.0f);
    const Rectangle list_box = timing_columns.first;
    const Rectangle editor_box = timing_columns.second;
    ui::section(list_box);
    ui::draw_text_in_rect("Events", 20,
                          {list_box.x + 14.0f, list_box.y + 10.0f, list_box.width - 28.0f, 24.0f},
                          t.text, ui::text_align::left);

    const Rectangle list_view = {list_box.x + 12.0f, list_box.y + 48.0f, list_box.width - 32.0f, list_box.height - 100.0f};
    const Rectangle scrollbar = {list_view.x + list_view.width + 6.0f, list_view.y, 6.0f, list_view.height};
    const float row_height = 34.0f;
    const float row_gap = 5.0f;
    const float content_height =
        ui::vertical_list_content_height(items.size(), row_height, row_gap, list_view.height);
    ui::scroll_offset_state scroll_state =
        ui::scroll_offset_state_for(list_view, content_height, timing_state.list_scroll_offset);
    timing_state.list_scroll_offset = scroll_state.offset;
    const ui::scrollbar_interaction scrollbar_result = ui::vertical_scrollbar(
        scrollbar,
        content_height,
        timing_state.list_scroll_offset,
        timing_state.list_scrollbar_dragging,
        timing_state.list_scrollbar_drag_offset,
        {
            .min_thumb_height = 28.0f,
            .drag_blocked_by_layer = false,
        });
    if (scrollbar_result.changed || scrollbar_result.dragging) {
        timing_state.list_scroll_offset = scrollbar_result.scroll_offset;
    }
    const float wheel = ui::mouse_wheel_move();
    scroll_state = ui::wheel_scrolled_offset_state(
        list_view, model.mouse, wheel, content_height, timing_state.list_scroll_offset, 42.0f);
    timing_state.list_scroll_offset = scroll_state.offset;
    {
        ui::scoped_clip_rect clip_scope(list_view);
        const ui::index_range visible_rows = ui::vertical_list_visible_range(
            items.size(), list_view, row_height, row_gap, timing_state.list_scroll_offset);
        for (int row_index = visible_rows.begin; row_index < visible_rows.end; ++row_index) {
            const editor_timing_panel_item& item = items[static_cast<std::size_t>(row_index)];
            const Rectangle item_rect =
                ui::vertical_list_row_rect(list_view, row_index, row_height, row_gap, timing_state.list_scroll_offset);
            const ui::row_state row_state = draw_layer_row(item_rect, item.selected, ui::draw_layer::modal, t.accent);
            if (row_state.clicked) {
                result.panel_result.selected_event_index = item.event_index;
            }
            ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 10.0f)),
                                 item.label.c_str(), item.value.c_str(), 14,
                                 item.selected ? t.text : t.text_secondary,
                                 item.selected ? t.text : t.text_muted, 126.0f);
        }
    }
    ui::scrollbar(scrollbar, content_height, timing_state.list_scroll_offset, {
        .track_color = t.scrollbar_track,
        .thumb_color = t.scrollbar_thumb,
        .min_thumb_height = 28.0f,
        .custom_colors = true,
    });

    std::array<Rectangle, 3> timing_buttons{};
    ui::hstack_fill({
        list_box.x + 12.0f,
        list_box.y + list_box.height - 42.0f,
        list_box.width - 32.0f,
        30.0f,
    }, 8.0f, timing_buttons);
    draw_timing_modal_action_buttons(
        timing_modal_action_buttons_for(timing_buttons, model.delete_enabled),
        result.panel_result);

    ui::section(editor_box);
    ui::draw_text_in_rect("Event Inspector", 20,
                          {editor_box.x + 14.0f, editor_box.y + 10.0f, editor_box.width - 28.0f, 24.0f},
                          t.text, ui::text_align::left);

    if (selected_event.has_value()) {
        const timing_event& event = *selected_event;
        ui::draw_label_value({editor_box.x + 14.0f, editor_box.y + 52.0f, editor_box.width - 28.0f, 24.0f},
                             "Type", timing_event_type_label(event.type), 16,
                             t.text_secondary, t.text, 82.0f);
        if (event.type == timing_event_type::bpm) {
            draw_timing_modal_pick_row(
                {editor_box.x + 14.0f, editor_box.y + 90.0f, editor_box.width - 28.0f, 38.0f},
                "Bar", timing_state.inputs.bpm_bar.value, editor_timing_input_field::bpm_measure,
                timing_state, result.panel_result);
            draw_timing_modal_input_row(
                {editor_box.x + 14.0f, editor_box.y + 138.0f, editor_box.width - 28.0f, 38.0f},
                "BPM", timing_state.inputs.bpm_value, editor_timing_input_field::bpm_value,
                accepts_float_character, "BPM", 82.0f, timing_state, result.panel_result);
        } else {
            draw_timing_modal_pick_row(
                {editor_box.x + 14.0f, editor_box.y + 90.0f, editor_box.width - 28.0f, 38.0f},
                "Bar", timing_state.inputs.meter_bar.value, editor_timing_input_field::meter_measure,
                timing_state, result.panel_result);
            const ui::rect_pair meter_inputs = ui::split_columns(
                {editor_box.x + 14.0f, editor_box.y + 138.0f, editor_box.width - 28.0f, 38.0f},
                (editor_box.width - 36.0f) * 0.5f, 8.0f);
            draw_timing_modal_input_row(
                meter_inputs.first,
                "Num", timing_state.inputs.meter_numerator,
                editor_timing_input_field::meter_numerator,
                accepts_int_character, "Num", 46.0f, timing_state, result.panel_result);
            draw_timing_modal_input_row(
                meter_inputs.second,
                "Den", timing_state.inputs.meter_denominator,
                editor_timing_input_field::meter_denominator,
                accepts_int_character, "Den", 46.0f, timing_state, result.panel_result);
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
