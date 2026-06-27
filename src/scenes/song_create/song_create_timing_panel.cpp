#include "song_create/song_create_timing_panel.h"

#include <algorithm>
#include <array>
#include <string>

#include "localization/localization.h"
#include "song_create/song_create_timing_service.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_text.h"

namespace song_create::timing_panel {
namespace {

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

ui::button_state timing_button(Rectangle rect, const char* label, int font_size,
                               ui::draw_layer layer,
                               Color bg = g_theme->row,
                               Color bg_hover = g_theme->row_hover,
                               Color text = g_theme->text,
                               float border_width = 2.0f) {
    return ui::button(rect, localization::tr_literal(label), {
        .layer = layer,
        .font_size = font_size,
        .border_width = border_width,
        .bg = bg,
        .bg_hover = bg_hover,
        .text_color = text,
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
    if (event_count == 0) {
        return fallback_height;
    }
    return static_cast<float>(event_count) * row_height +
           static_cast<float>(std::max<int>(0, static_cast<int>(event_count) - 1)) * row_gap;
}

Rectangle timing_event_row_rect(Rectangle list_view, std::size_t index, float row_height, float row_gap, float scroll) {
    return {
        list_view.x,
        list_view.y + static_cast<float>(index) * (row_height + row_gap) - scroll,
        list_view.width,
        row_height,
    };
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

    if (ui::button(layout.edit_button, "EDIT", {
            .layer = view_config.base_layer,
            .font_size = 15,
        }).clicked) {
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

    if (timing_button(layout.import_button, "IMPORT MIDI", 13, view_config.modal_layer,
                      g_theme->section, g_theme->row_hover, g_theme->text, 1.5f).clicked) {
        result.import_midi_requested = true;
    }
    ui::draw_text_in_rect(
        state.import_status.empty() ? "Reads MIDI tempo and time signature events." : state.import_status.c_str(),
        13, layout.import_hint, state.import_status.empty() ? g_theme->text_muted : g_theme->text_secondary,
        ui::text_align::left);

    result.editor = draw_editor(layout.editor, view_config.modal_layer, state, actions);
    if (timing_button(layout.done_button, "DONE", 14, view_config.modal_layer,
                      g_theme->row_active, g_theme->row_hover, g_theme->text).clicked) {
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

    const float row_height = 28.0f;
    const float row_gap = 5.0f;
    const float content_height =
        timing_event_list_content_height(state.events.size(), row_height, row_gap, layout.list_view.height);
    const float max_scroll = std::max(0.0f, content_height - layout.list_view.height);
    float event_scroll_offset = std::clamp(state.event_scroll_offset, 0.0f, max_scroll);
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
    const float mouse_wheel = GetMouseWheelMove();
    if (ui::is_hovered(layout.list_view, layer) && mouse_wheel != 0.0f) {
        event_scroll_offset = std::clamp(event_scroll_offset - mouse_wheel * 42.0f, 0.0f, max_scroll);
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
        for (size_t index = 0; index < state.events.size(); ++index) {
            const timing_event& event = state.events[index];
            const bool selected = state.selected_event_index.has_value() && *state.selected_event_index == index;
            const Rectangle row = timing_event_row_rect(layout.list_view, index, row_height, row_gap,
                                                        event_scroll_offset);
            const ui::row_state row_state = timing_selectable_row(row, selected, layer, 1.2f);
            if (row_state.clicked) {
                result.selected_event_index = index;
            }
            const std::string label = std::string(timing_type_label(event.type)) + " " + meter_map.bar_beat_label(event.tick);
            ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 8.0f)),
                                 label.c_str(), timing_value_label(event).c_str(), 14,
                                 selected ? g_theme->text : g_theme->text_secondary,
                                 selected ? g_theme->text : g_theme->text_muted,
                                 160.0f);
        }
    }
    ui::scrollbar(layout.scrollbar, content_height, event_scroll_offset, {
        .track_color = g_theme->scrollbar_track,
        .thumb_color = g_theme->scrollbar_thumb,
        .min_thumb_height = 28.0f,
        .custom_colors = true,
    });

    if (timing_button(layout.action_buttons[0], "Add BPM", 13, layer).clicked) {
        result.add_event_type = timing_event_type::bpm;
    }
    if (timing_button(layout.action_buttons[1], "Add Time Sig", 13, layer).clicked) {
        result.add_event_type = timing_event_type::meter;
    }
    if (timing_button(layout.action_buttons[2], "Delete", 13, layer).clicked) {
        result.delete_selected_event_requested = true;
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
    if (timing_button(layout.metronome,
                      state.metronome_enabled ? "Metronome On" : "Metronome",
                      13, layer, metro_base, g_theme->row_hover, metro_text, 1.2f).clicked) {
        if (state.metronome_enabled) {
            result.stop_metronome_requested = true;
        } else {
            result.start_metronome_requested = true;
        }
    }
    return result;
}

}  // namespace song_create::timing_panel
