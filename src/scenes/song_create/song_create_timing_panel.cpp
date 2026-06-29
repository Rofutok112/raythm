#include "song_create/song_create_timing_panel.h"

#include <array>
#include <string>

#include "localization/localization.h"
#include "song_create/song_create_timing_service.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_hit.h"
#include "ui_scroll.h"
#include "ui_text.h"
#include "virtual_screen.h"

namespace song_create::timing_panel {
namespace {

constexpr float kTimingEventRowHeight = 28.0f;
constexpr float kTimingEventRowGap = 5.0f;
constexpr float kTimingEventWheelStep = 42.0f;

bool numeric_filter(int codepoint, const std::string&) {
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '.';
}

bool int_filter(int codepoint, const std::string&) {
    return codepoint >= '0' && codepoint <= '9';
}

bool signed_int_filter(int codepoint, const std::string& value) {
    return (codepoint >= '0' && codepoint <= '9') || (codepoint == '-' && value.find('-') == std::string::npos);
}

bool bar_beat_filter(int codepoint, const std::string& value) {
    return (codepoint >= '0' && codepoint <= '9') || (codepoint == ':' && value.find(':') == std::string::npos);
}

struct timing_summary_layout {
    Rectangle summary;
    Rectangle import_status;
    Rectangle edit_button;
};

struct timing_modal_layout {
    Rectangle title;
    std::array<Rectangle, 2> timing_inputs;
    Rectangle import_button;
    Rectangle import_hint;
    Rectangle editor;
    Rectangle done_button;
};

struct timing_editor_layout {
    Rectangle label;
    Rectangle list;
    Rectangle event_editor;
    Rectangle list_view;
    Rectangle scrollbar;
    std::array<Rectangle, 3> action_buttons;
    Rectangle selected_type;
    std::array<Rectangle, 3> event_inputs;
    Rectangle metronome;
};

enum class timing_editor_action {
    add_bpm,
    add_meter,
    delete_selected,
};

struct timing_editor_action_definition {
    timing_editor_action action;
    const char* label;
};

struct timing_button_descriptor {
    Rectangle rect;
    const char* label;
    int font_size;
    ui::draw_layer layer;
    Color bg;
    Color bg_hover;
    Color text;
    float border_width;
};

struct timing_event_row {
    const timing_event* event = nullptr;
    std::size_t index = 0;
    Rectangle rect{};
    bool selected = false;
};

constexpr std::array<timing_editor_action_definition, 3> kTimingEditorActions = {{
    {timing_editor_action::add_bpm, "Add BPM"},
    {timing_editor_action::add_meter, "Add Time Sig"},
    {timing_editor_action::delete_selected, "Delete"},
}};

bool parse_int_text(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_float_text(const std::string& text, float& value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const float parsed = std::stof(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

timing_button_descriptor timing_button_for(Rectangle rect,
                                           const char* label,
                                           int font_size,
                                           ui::draw_layer layer,
                                           Color bg = g_theme->row,
                                           Color bg_hover = g_theme->row_hover,
                                           Color text = g_theme->text,
                                           float border_width = 2.0f) {
    return {
        .rect = rect,
        .label = label,
        .font_size = font_size,
        .layer = layer,
        .bg = bg,
        .bg_hover = bg_hover,
        .text = text,
        .border_width = border_width,
    };
}

ui::button_state draw_timing_button(const timing_button_descriptor& button) {
    return ui::button(button.rect, localization::tr_literal(button.label), {
        .layer = button.layer,
        .font_size = button.font_size,
        .border_width = button.border_width,
        .bg = button.bg,
        .bg_hover = button.bg_hover,
        .text_color = button.text,
        .custom_colors = true,
    });
}

ui::row_state timing_selectable_row(Rectangle rect, bool selected, ui::draw_layer layer,
                                    float border_width = 2.0f) {
    return ui::row(rect, {
        .layer = layer,
        .border_width = border_width,
        .bg = selected ? g_theme->row_selected : g_theme->row,
        .bg_hover = selected ? g_theme->row_active : g_theme->row_hover,
        .border_color = selected ? g_theme->border_active : g_theme->border,
        .custom_colors = true,
    });
}

const char* timing_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Time Sig";
}

std::string timing_value_label(const timing_event& event) {
    if (event.type == timing_event_type::bpm) {
        return TextFormat("%.3g", event.bpm);
    }
    return std::to_string(event.numerator) + "/" + std::to_string(event.denominator);
}

void draw_field_label(Rectangle row, const char* label, float label_width) {
    const Rectangle label_rect = {row.x + 12.0f, row.y, label_width - 12.0f, row.height};
    ui::draw_text_in_rect(label, 16, label_rect, g_theme->text_secondary, ui::text_align::left);
}

timing_summary_layout summary_layout_for(Rectangle rect, float label_width) {
    const Rectangle summary = {
        rect.x + label_width + 12.0f,
        rect.y + 8.0f,
        rect.width - label_width - 180.0f,
        rect.height - 16.0f,
    };
    return {
        summary,
        {summary.x, summary.y + 30.0f, summary.width, 18.0f},
        {rect.x + rect.width - 150.0f, rect.y + 10.0f, 138.0f, rect.height - 20.0f},
    };
}

timing_modal_layout modal_layout_for(Rectangle modal) {
    const float content_x = modal.x + 30.0f;
    const float content_width = modal.width - 60.0f;
    const float input_y = modal.y + 80.0f;
    std::array<Rectangle, 2> timing_inputs{};
    ui::hstack_fill({content_x, input_y, content_width, 52.0f}, 12.0f, timing_inputs);

    const float import_y = input_y + 64.0f;
    const Rectangle import_button = {content_x, import_y, 188.0f, 38.0f};
    const Rectangle import_hint = {
        import_button.x + import_button.width + 16.0f,
        import_y,
        content_width - import_button.width - 16.0f,
        38.0f,
    };

    return {
        {modal.x + 30.0f, modal.y + 24.0f, modal.width - 180.0f, 34.0f},
        timing_inputs,
        import_button,
        import_hint,
        {content_x, import_y + 52.0f, content_width, 430.0f},
        {modal.x + modal.width - 170.0f, modal.y + modal.height - 58.0f, 140.0f, 38.0f},
    };
}

timing_editor_layout editor_layout_for(Rectangle rect) {
    const ui::rect_pair editor_columns = ui::split_columns(
        {rect.x + 200.0f, rect.y + 10.0f, rect.width - 212.0f, rect.height - 20.0f},
        366.0f,
        14.0f);
    const Rectangle list = editor_columns.first;
    const Rectangle event_editor = editor_columns.second;
    const Rectangle list_view = {list.x, list.y, list.width - 14.0f, list.height - 38.0f};
    const Rectangle scrollbar = {list_view.x + list_view.width + 6.0f, list_view.y,
                                 6.0f, list_view.height};
    constexpr std::array<float, 3> kActionButtonWidths = {104.0f, 124.0f, 92.0f};
    std::array<Rectangle, 3> action_buttons{};
    ui::hstack_widths({list.x, list.y + list.height - 30.0f, 336.0f, 28.0f},
                      kActionButtonWidths, 8.0f, action_buttons);
    std::array<Rectangle, 3> event_inputs{};
    ui::hstack_fill({event_editor.x, event_editor.y + 32.0f, event_editor.width, 34.0f},
                    8.0f, event_inputs);
    return {
        {rect.x + 12.0f, rect.y + 10.0f, 188.0f, 28.0f},
        list,
        event_editor,
        list_view,
        scrollbar,
        action_buttons,
        {event_editor.x, event_editor.y, event_editor.width, 24.0f},
        event_inputs,
        {event_editor.x, event_editor.y + event_editor.height - 34.0f, 132.0f, 30.0f},
    };
}

float timing_event_list_content_height(std::size_t event_count, float row_height, float row_gap, float fallback_height) {
    return ui::vertical_list_content_height(event_count, row_height, row_gap, fallback_height);
}

Rectangle timing_event_row_rect(Rectangle list_view, std::size_t index, float row_height, float row_gap, float scroll) {
    return ui::vertical_list_row_rect(list_view, static_cast<int>(index), row_height, row_gap, scroll);
}

timing_event_row timing_event_row_for(Rectangle list_view,
                                      const state_refs& state,
                                      std::size_t index,
                                      float scroll) {
    return {
        .event = index < state.events.size() ? &state.events[index] : nullptr,
        .index = index,
        .rect = timing_event_row_rect(
            list_view,
            index,
            kTimingEventRowHeight,
            kTimingEventRowGap,
            scroll),
        .selected = state.selected_event_index.has_value() && *state.selected_event_index == index,
    };
}

std::array<ui::action_button_definition<timing_editor_action>, kTimingEditorActions.size()>
timing_editor_action_buttons_for(const timing_editor_layout& layout) {
    std::array<ui::action_button_definition<timing_editor_action>, kTimingEditorActions.size()> buttons{};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        buttons[i] = {
            .rect = layout.action_buttons[i],
            .label = localization::tr_literal(kTimingEditorActions[i].label),
            .action = kTimingEditorActions[i].action,
        };
    }
    return buttons;
}

ui::row_state draw_timing_event_row(const timing_event_row& row,
                                    const editor_meter_map& meter_map,
                                    ui::draw_layer layer) {
    if (row.event == nullptr) {
        return {};
    }

    const ui::row_state row_state = timing_selectable_row(row.rect, row.selected, layer, 1.2f);
    const std::string label =
        std::string(timing_type_label(row.event->type)) + " " + meter_map.bar_beat_label(row.event->tick);
    ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 8.0f)),
                         label.c_str(), timing_value_label(*row.event).c_str(), 14,
                         row.selected ? g_theme->text : g_theme->text_secondary,
                         row.selected ? g_theme->text : g_theme->text_muted,
                         160.0f);
    return row_state;
}

void apply_timing_editor_action(editor_result& result, timing_editor_action action) {
    switch (action) {
        case timing_editor_action::add_bpm:
            result.add_event_type = timing_event_type::bpm;
            break;
        case timing_editor_action::add_meter:
            result.add_event_type = timing_event_type::meter;
            break;
        case timing_editor_action::delete_selected:
            result.delete_selected_event_requested = true;
            break;
    }
}

}  // namespace

summary_result draw_summary(Rectangle rect, state_refs state, const callbacks& actions, const config& view_config) {
    summary_result result;
    actions.ensure_initialized();
    float bpm = 0.0f;
    if (!parse_float_text(state.bpm_input.value, bpm) || bpm <= 0.0f) {
        bpm = 120.0f;
    }
    int offset_ms = 0;
    if (!state.offset_input.value.empty()) {
        parse_int_text(state.offset_input.value, offset_ms);
    }

    ui::surface(rect, g_theme->row, g_theme->border, 1.5f);
    draw_field_label(rect, "Song Timing", view_config.text_input_label_width);

    const timing_summary_layout layout = summary_layout_for(rect, view_config.text_input_label_width);
    const std::string summary = TextFormat("BPM %.6g / Offset %d ms / %d events / 480 PPQ",
                                           bpm, offset_ms,
                                           static_cast<int>(state.events.size()));
    ui::draw_text_in_rect(summary.c_str(), 16, layout.summary, g_theme->text, ui::text_align::left);
    if (!state.import_status.empty()) {
        ui::draw_text_in_rect(state.import_status.c_str(), 12, layout.import_status,
                              g_theme->text_muted, ui::text_align::left);
    }

    const timing_button_descriptor edit_button =
        timing_button_for(layout.edit_button, "EDIT", 15, view_config.base_layer);
    if (draw_timing_button(edit_button).clicked) {
        result.open_requested = true;
    }
    return result;
}

modal_result draw_modal(state_refs state, const callbacks& actions, const config& view_config) {
    modal_result result;
    ui::backdrop(view_config.screen_rect, with_alpha(BLACK, 150));
    ui::panel(view_config.modal_rect);

    const timing_modal_layout layout = modal_layout_for(view_config.modal_rect);
    ui::draw_text_in_rect("Song Timing", 24, layout.title, g_theme->text, ui::text_align::left);
    ui::text_input(layout.timing_inputs[0], state.bpm_input, "Song BPM", "120.0", {
        .layer = view_config.modal_layer,
        .font_size = 15,
        .max_length = 16,
        .filter = numeric_filter,
        .label_width = 100.0f,
    });
    ui::text_input(layout.timing_inputs[1], state.offset_input, "Song Offset", "0", {
        .default_value = "0",
        .layer = view_config.modal_layer,
        .font_size = 15,
        .max_length = 10,
        .filter = signed_int_filter,
        .label_width = 118.0f,
    });

    const timing_button_descriptor import_button =
        timing_button_for(layout.import_button, "IMPORT MIDI", 13, view_config.modal_layer,
                          g_theme->section, g_theme->row_hover, g_theme->text, 1.5f);
    if (draw_timing_button(import_button).clicked) {
        result.import_midi_requested = true;
    }
    ui::draw_text_in_rect(
        state.import_status.empty() ? "Reads MIDI tempo and time signature events." : state.import_status.c_str(),
        13, layout.import_hint, state.import_status.empty() ? g_theme->text_muted : g_theme->text_secondary,
        ui::text_align::left);

    result.editor = draw_editor(layout.editor, view_config.modal_layer, state, actions);
    const timing_button_descriptor done_button =
        timing_button_for(layout.done_button, "DONE", 14, view_config.modal_layer,
                          g_theme->row_active, g_theme->row_hover, g_theme->text);
    if (draw_timing_button(done_button).clicked) {
        result.close_requested = true;
    }
    return result;
}

editor_result draw_editor(Rectangle rect, ui::draw_layer layer, state_refs state, const callbacks& actions) {
    editor_result result;
    if (state.events.empty()) {
        actions.ensure_initialized();
    }
    ui::surface(rect, g_theme->row, g_theme->border, 1.5f);

    const timing_editor_layout layout = editor_layout_for(rect);
    ui::draw_text_in_rect("Song Timing", 16, layout.label, g_theme->text_secondary, ui::text_align::left);

    const float content_height =
        timing_event_list_content_height(
            state.events.size(),
            kTimingEventRowHeight,
            kTimingEventRowGap,
            layout.list_view.height);
    ui::scroll_offset_state event_scroll =
        ui::scroll_offset_state_for(layout.list_view, content_height, state.event_scroll_offset);
    float event_scroll_offset = event_scroll.offset;
    bool event_scrollbar_dragging = state.event_scrollbar_dragging;
    float event_scrollbar_drag_offset = state.event_scrollbar_drag_offset;
    const ui::scrollbar_interaction scrollbar = ui::vertical_scrollbar(
        layout.scrollbar, content_height, event_scroll_offset,
        event_scrollbar_dragging, event_scrollbar_drag_offset, {
            .layer = layer,
            .min_thumb_height = 28.0f,
        });
    if (scrollbar.changed || scrollbar.dragging) {
        event_scroll_offset = scrollbar.scroll_offset;
    }
    const float mouse_wheel = ui::mouse_wheel_move();
    if (ui::is_hovered(layout.list_view, layer)) {
        event_scroll = ui::wheel_scrolled_offset_state(
            layout.list_view,
            virtual_screen::get_virtual_mouse(),
            mouse_wheel,
            content_height,
            event_scroll_offset,
            kTimingEventWheelStep);
        event_scroll_offset = event_scroll.offset;
    }
    if (event_scroll_offset != state.event_scroll_offset) {
        result.event_scroll_changed = true;
        result.event_scroll_offset = event_scroll_offset;
    }
    if (event_scrollbar_dragging != state.event_scrollbar_dragging ||
        event_scrollbar_drag_offset != state.event_scrollbar_drag_offset) {
        result.event_scrollbar_drag_state_changed = true;
        result.event_scrollbar_dragging = event_scrollbar_dragging;
        result.event_scrollbar_drag_offset = event_scrollbar_drag_offset;
    }
    const editor_meter_map meter_map = song_create::timing_service::build_meter_map(state.events, 480);
    {
        ui::scoped_clip_rect clip_scope(layout.list_view);
        const ui::index_range visible_rows = ui::vertical_list_visible_range(
            state.events.size(), layout.list_view, kTimingEventRowHeight, kTimingEventRowGap, event_scroll_offset);
        for (int index = visible_rows.begin; index < visible_rows.end; ++index) {
            const timing_event_row row =
                timing_event_row_for(layout.list_view, state, static_cast<std::size_t>(index), event_scroll_offset);
            const ui::row_state row_state = draw_timing_event_row(row, meter_map, layer);
            if (row_state.clicked) {
                result.selected_event_index = row.index;
            }
        }
    }
    ui::scrollbar(layout.scrollbar, content_height, event_scroll_offset, {
        .track_color = g_theme->scrollbar_track,
        .thumb_color = g_theme->scrollbar_thumb,
        .min_thumb_height = 28.0f,
        .custom_colors = true,
    });

    const std::array<ui::action_button_definition<timing_editor_action>, kTimingEditorActions.size()> action_buttons =
        timing_editor_action_buttons_for(layout);
    const auto clicked_action = ui::draw_action_buttons<timing_editor_action>(action_buttons, {
        .layer = layer,
        .font_size = 13,
        .border_width = 2.0f,
    });
    if (clicked_action.has_value()) {
        apply_timing_editor_action(result, *clicked_action);
    }

    const bool has_selection = state.selected_event_index.has_value() &&
                               *state.selected_event_index < state.events.size();
    if (!has_selection) {
        ui::draw_text_in_rect("Select or add a timing event.", 15, layout.event_editor,
                              g_theme->text_hint, ui::text_align::center);
        return result;
    }

    const timing_event& selected = state.events[*state.selected_event_index];
    ui::draw_label_value(layout.selected_type, "Type", timing_type_label(selected.type), 15,
                         g_theme->text_secondary, g_theme->text, 72.0f);

    ui::text_input(layout.event_inputs[0], state.bar_input, "Bar", "1:1", {
                       .layer = layer,
                       .font_size = 14,
                       .max_length = 8,
                       .filter = bar_beat_filter,
                       .label_width = 42.0f,
                   });
    if (selected.type == timing_event_type::bpm) {
        ui::text_input(layout.event_inputs[1], state.event_bpm_input, "BPM", "120", {
                           .layer = layer,
                           .font_size = 14,
                           .max_length = 12,
                           .filter = numeric_filter,
                           .label_width = 42.0f,
                       });
    } else {
        ui::text_input(layout.event_inputs[1], state.numerator_input, "Num", "4", {
                           .layer = layer,
                           .font_size = 14,
                           .max_length = 4,
                           .filter = int_filter,
                           .label_width = 42.0f,
                       });
        ui::text_input(layout.event_inputs[2], state.denominator_input, "Den", "4", {
                           .layer = layer,
                           .font_size = 14,
                           .max_length = 4,
                           .filter = int_filter,
                           .label_width = 42.0f,
                       });
    }

    const Color metro_base = state.metronome_enabled ? g_theme->accent : g_theme->section;
    const Color metro_text = state.metronome_enabled ? g_theme->panel : g_theme->text;
    const timing_button_descriptor metronome_button =
        timing_button_for(layout.metronome,
                          state.metronome_enabled ? "Metronome On" : "Metronome",
                          13, layer, metro_base, g_theme->row_hover, metro_text, 1.2f);
    if (draw_timing_button(metronome_button).clicked) {
        if (state.metronome_enabled) {
            result.stop_metronome_requested = true;
        } else {
            result.start_metronome_requested = true;
        }
    }
    return result;
}

}  // namespace song_create::timing_panel
