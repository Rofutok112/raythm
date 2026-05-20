#include "song_create/song_create_timing_panel.h"

#include <algorithm>
#include <string>

#include "file_dialog.h"
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

ui::button_state draw_button_on_layer(Rectangle rect, const char* label, int font_size,
                                      ui::draw_layer layer,
                                      Color bg = g_theme->row,
                                      Color bg_hover = g_theme->row_hover,
                                      Color text = g_theme->text,
                                      float border_width = 2.0f) {
    const bool hovered = ui::is_hovered(rect, layer);
    const bool pressed = ui::is_pressed(rect, layer);
    const bool clicked = ui::is_clicked(rect, layer);
    ui::detail::draw_button_visual(rect, hovered, pressed, localization::tr_literal(label), font_size,
                                   bg, bg_hover, text, border_width);
    return {hovered, pressed, clicked};
}

ui::row_state draw_selectable_row_on_layer(Rectangle rect, bool selected, ui::draw_layer layer,
                                           float border_width = 2.0f) {
    const bool hovered = ui::is_hovered(rect, layer);
    const bool pressed = ui::is_pressed(rect, layer);
    const bool clicked = ui::is_clicked(rect, layer);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    ui::detail::draw_row_visual(rect, hovered, pressed,
                                selected ? g_theme->row_selected : g_theme->row,
                                selected ? g_theme->row_active : g_theme->row_hover,
                                selected ? g_theme->border_active : g_theme->border,
                                border_width);
    return {hovered, pressed, clicked, visual};
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

}  // namespace

void draw_summary(Rectangle rect, state_refs state, const callbacks& actions, const config& view_config) {
    actions.ensure_initialized();
    float bpm = 0.0f;
    if (!parse_float_text(state.bpm_input.value, bpm) || bpm <= 0.0f) {
        bpm = 120.0f;
    }
    int offset_ms = 0;
    if (!state.offset_input.value.empty()) {
        parse_int_text(state.offset_input.value, offset_ms);
    }

    ui::draw_rect_f(rect, g_theme->row);
    ui::draw_rect_lines(rect, 1.5f, g_theme->border);
    draw_field_label(rect, "Song Timing", view_config.text_input_label_width);

    const Rectangle summary_rect = {
        rect.x + view_config.text_input_label_width + 12.0f,
        rect.y + 8.0f,
        rect.width - view_config.text_input_label_width - 180.0f,
        rect.height - 16.0f,
    };
    const std::string summary = TextFormat("BPM %.6g / Offset %d ms / %d events / 480 PPQ",
                                           bpm, offset_ms,
                                           static_cast<int>(state.events.size()));
    ui::draw_text_in_rect(summary.c_str(), 16, summary_rect, g_theme->text, ui::text_align::left);
    if (!state.import_status.empty()) {
        ui::draw_text_in_rect(state.import_status.c_str(), 12,
                              {summary_rect.x, summary_rect.y + 30.0f, summary_rect.width, 18.0f},
                              g_theme->text_muted, ui::text_align::left);
    }

    const Rectangle edit_rect = {rect.x + rect.width - 150.0f, rect.y + 10.0f, 138.0f, rect.height - 20.0f};
    if (ui::draw_button(edit_rect, "EDIT", 15).clicked) {
        state.modal_open = true;
        state.import_status.clear();
        actions.ensure_initialized();
    }
}

void draw_modal(state_refs state, const callbacks& actions, const config& view_config) {
    ui::draw_rect_f(view_config.screen_rect, with_alpha(BLACK, 150));
    ui::draw_panel(view_config.modal_rect);

    const Rectangle title_rect = {view_config.modal_rect.x + 30.0f, view_config.modal_rect.y + 24.0f,
                                  view_config.modal_rect.width - 180.0f, 34.0f};
    ui::draw_text_in_rect("Song Timing", 24, title_rect, g_theme->text, ui::text_align::left);

    const float content_x = view_config.modal_rect.x + 30.0f;
    float y = view_config.modal_rect.y + 80.0f;
    const float content_width = view_config.modal_rect.width - 60.0f;
    const float gap = 12.0f;
    const float half_width = (content_width - gap) / 2.0f;
    ui::draw_text_input({content_x, y, half_width, 52.0f}, state.bpm_input,
                        "Song BPM", "120.0", nullptr, view_config.modal_layer, 15, 16,
                        numeric_filter, 100.0f);
    ui::draw_text_input({content_x + half_width + gap, y, half_width, 52.0f}, state.offset_input,
                        "Song Offset", "0", "0", view_config.modal_layer, 15, 10,
                        signed_int_filter, 118.0f);
    y += 64.0f;

    const Rectangle import_rect = {content_x, y, 188.0f, 38.0f};
    const Rectangle hint_rect = {import_rect.x + import_rect.width + 16.0f, y,
                                 content_width - import_rect.width - 16.0f, 38.0f};
    if (draw_button_on_layer(import_rect, "IMPORT MIDI", 13, view_config.modal_layer,
                             g_theme->section, g_theme->row_hover, g_theme->text, 1.5f).clicked) {
        const std::string path = file_dialog::open_midi_file();
        if (!path.empty()) {
            actions.import_midi(path);
        }
    }
    ui::draw_text_in_rect(
        state.import_status.empty() ? "Reads MIDI tempo and time signature events." : state.import_status.c_str(),
        13, hint_rect, state.import_status.empty() ? g_theme->text_muted : g_theme->text_secondary,
        ui::text_align::left);
    y += 52.0f;

    draw_editor({content_x, y, content_width, 430.0f}, view_config.modal_layer, state, actions);

    const Rectangle done_rect = {view_config.modal_rect.x + view_config.modal_rect.width - 170.0f,
                                 view_config.modal_rect.y + view_config.modal_rect.height - 58.0f,
                                 140.0f, 38.0f};
    if (draw_button_on_layer(done_rect, "DONE", 14, view_config.modal_layer,
                             g_theme->row_active, g_theme->row_hover, g_theme->text).clicked) {
        actions.close_modal();
    }
}

void draw_editor(Rectangle rect, ui::draw_layer layer, state_refs state, const callbacks& actions) {
    if (state.events.empty()) {
        actions.ensure_initialized();
    }
    ui::draw_rect_f(rect, g_theme->row);
    ui::draw_rect_lines(rect, 1.5f, g_theme->border);

    const Rectangle label_rect = {rect.x + 12.0f, rect.y + 10.0f, 188.0f, 28.0f};
    ui::draw_text_in_rect("Song Timing", 16, label_rect, g_theme->text_secondary, ui::text_align::left);

    const Rectangle list_rect = {rect.x + 200.0f, rect.y + 10.0f, 366.0f, rect.height - 20.0f};
    const Rectangle editor_rect = {list_rect.x + list_rect.width + 14.0f, rect.y + 10.0f,
                                   rect.x + rect.width - (list_rect.x + list_rect.width + 26.0f),
                                   rect.height - 20.0f};

    const float row_height = 28.0f;
    const float row_gap = 5.0f;
    const Rectangle list_view_rect = {list_rect.x, list_rect.y, list_rect.width - 14.0f, list_rect.height - 38.0f};
    const Rectangle scrollbar_rect = {list_view_rect.x + list_view_rect.width + 6.0f, list_view_rect.y,
                                      6.0f, list_view_rect.height};
    const float content_height = state.events.empty()
        ? list_view_rect.height
        : static_cast<float>(state.events.size()) * row_height +
              static_cast<float>(std::max<int>(0, static_cast<int>(state.events.size()) - 1)) * row_gap;
    const float max_scroll = std::max(0.0f, content_height - list_view_rect.height);
    state.event_scroll_offset = std::clamp(state.event_scroll_offset, 0.0f, max_scroll);
    const ui::scrollbar_interaction scrollbar = ui::update_vertical_scrollbar(
        scrollbar_rect, content_height, state.event_scroll_offset,
        state.event_scrollbar_dragging, state.event_scrollbar_drag_offset, layer, 28.0f);
    if (scrollbar.changed || scrollbar.dragging) {
        state.event_scroll_offset = scrollbar.scroll_offset;
    }
    if (ui::is_hovered(list_view_rect, layer) && GetMouseWheelMove() != 0.0f) {
        state.event_scroll_offset =
            std::clamp(state.event_scroll_offset - GetMouseWheelMove() * 42.0f, 0.0f, max_scroll);
    }
    const editor_meter_map meter_map = song_create::timing_service::build_meter_map(state.events, 480);
    {
        ui::scoped_clip_rect clip_scope(list_view_rect);
        float row_y = list_view_rect.y - state.event_scroll_offset;
        for (size_t index = 0; index < state.events.size(); ++index) {
            const timing_event& event = state.events[index];
            const bool selected = state.selected_event_index.has_value() && *state.selected_event_index == index;
            const Rectangle row = {list_view_rect.x, row_y, list_view_rect.width, row_height};
            const ui::row_state row_state = draw_selectable_row_on_layer(row, selected, layer, 1.2f);
            if (row_state.clicked) {
                if (selected || actions.flush_selected_inputs()) {
                    state.selected_event_index = index;
                    actions.sync_selected_inputs();
                    state.error.clear();
                }
            }
            const std::string label = std::string(timing_type_label(event.type)) + " " + meter_map.bar_beat_label(event.tick);
            ui::draw_label_value(ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 8.0f)),
                                 label.c_str(), timing_value_label(event).c_str(), 14,
                                 selected ? g_theme->text : g_theme->text_secondary,
                                 selected ? g_theme->text : g_theme->text_muted,
                                 160.0f);
            row_y += row_height + row_gap;
        }
    }
    ui::draw_scrollbar(scrollbar_rect, content_height, state.event_scroll_offset,
                       g_theme->scrollbar_track, g_theme->scrollbar_thumb, 28.0f);

    const Rectangle add_bpm = {list_rect.x, list_rect.y + list_rect.height - 30.0f, 104.0f, 28.0f};
    const Rectangle add_sig = {add_bpm.x + add_bpm.width + 8.0f, add_bpm.y, 124.0f, 28.0f};
    const Rectangle delete_rect = {add_sig.x + add_sig.width + 8.0f, add_bpm.y, 92.0f, 28.0f};
    if (draw_button_on_layer(add_bpm, "Add BPM", 13, layer).clicked) {
        actions.add_event(timing_event_type::bpm);
    }
    if (draw_button_on_layer(add_sig, "Add Time Sig", 13, layer).clicked) {
        actions.add_event(timing_event_type::meter);
    }
    if (draw_button_on_layer(delete_rect, "Delete", 13, layer).clicked) {
        actions.delete_selected_event();
    }

    const bool has_selection = state.selected_event_index.has_value() &&
                               *state.selected_event_index < state.events.size();
    if (!has_selection) {
        ui::draw_text_in_rect("Select or add a timing event.", 15, editor_rect,
                              g_theme->text_hint, ui::text_align::center);
        return;
    }

    const timing_event& selected = state.events[*state.selected_event_index];
    ui::draw_label_value({editor_rect.x, editor_rect.y, editor_rect.width, 24.0f},
                         "Type", timing_type_label(selected.type), 15,
                         g_theme->text_secondary, g_theme->text, 72.0f);

    const float input_y = editor_rect.y + 32.0f;
    const float gap = 8.0f;
    const float small_width = (editor_rect.width - gap * 2.0f) / 3.0f;
    ui::draw_text_input({editor_rect.x, input_y, small_width, 34.0f},
                        state.bar_input, "Bar", "1:1", nullptr, layer, 14, 8,
                        bar_beat_filter, 42.0f);
    if (selected.type == timing_event_type::bpm) {
        ui::draw_text_input({editor_rect.x + small_width + gap, input_y, small_width, 34.0f},
                            state.event_bpm_input, "BPM", "120", nullptr, layer, 14, 12,
                            numeric_filter, 42.0f);
    } else {
        ui::draw_text_input({editor_rect.x + small_width + gap, input_y, small_width, 34.0f},
                            state.numerator_input, "Num", "4", nullptr, layer, 14, 4,
                            int_filter, 42.0f);
        ui::draw_text_input({editor_rect.x + (small_width + gap) * 2.0f, input_y, small_width, 34.0f},
                            state.denominator_input, "Den", "4", nullptr, layer, 14, 4,
                            int_filter, 42.0f);
    }

    const Rectangle metro_rect = {editor_rect.x, editor_rect.y + editor_rect.height - 34.0f, 132.0f, 30.0f};
    const Color metro_base = state.metronome_enabled ? g_theme->accent : g_theme->section;
    const Color metro_text = state.metronome_enabled ? g_theme->panel : g_theme->text;
    if (draw_button_on_layer(metro_rect,
                             state.metronome_enabled ? "Metronome On" : "Metronome",
                             13, layer, metro_base, g_theme->row_hover, metro_text, 1.2f).clicked) {
        if (state.metronome_enabled) {
            actions.stop_preview();
        } else if (actions.start_preview()) {
            state.metronome_enabled = true;
        }
    }
}

}  // namespace song_create::timing_panel
