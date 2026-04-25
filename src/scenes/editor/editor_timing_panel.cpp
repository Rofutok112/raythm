#include "editor_timing_panel.h"

#include <algorithm>
#include <cmath>

#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {
const char* timing_event_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Meter";
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

void set_active_input(editor_timing_panel_state& state, editor_timing_input_field field) {
    state.active_input_field = field;
    state.inputs.bpm_value.active = field == editor_timing_input_field::bpm_value;
    state.inputs.meter_numerator.active = field == editor_timing_input_field::meter_numerator;
    state.inputs.meter_denominator.active = field == editor_timing_input_field::meter_denominator;
    state.inputs.bpm_bar.active = false;
    state.inputs.meter_bar.active = false;
}

void clear_active_inputs(editor_timing_panel_state& state) {
    set_active_input(state, editor_timing_input_field::none);
}
}

editor_timing_panel_result editor_timing_panel::draw(const editor_timing_panel_model& model, editor_timing_panel_state& state) {
    const auto& t = *g_theme;
    editor_timing_panel_result result;

    const Rectangle editor_box = {model.content_rect.x, model.content_rect.y + 372.0f, model.content_rect.width, 164.0f};
    const Rectangle timing_box = {model.content_rect.x, model.content_rect.y, model.content_rect.width,
                                  editor_box.y - model.content_rect.y - 12.0f};

    auto draw_bar_pick_row = [&](Rectangle rect, const char* label, const std::string& value,
                                 editor_timing_input_field field, float label_width = 84.0f) {
        const bool selected = state.active_input_field == field || state.bar_pick_mode;
        const ui::row_state row = ui::draw_row(
            rect,
            selected ? t.row_selected : t.row,
            selected ? t.row_selected_hover : t.row_hover,
            state.bar_pick_mode ? t.accent : (selected ? t.border_active : t.border),
            1.5f);
        if (row.clicked) {
            result.clicked_input_row = true;
            state.active_input_field = field;
            state.bar_pick_mode = true;
            state.input_error.clear();
            state.inputs.bpm_value.active = false;
            state.inputs.meter_numerator.active = false;
            state.inputs.meter_denominator.active = false;
        }

        const Rectangle content_rect = ui::inset(row.visual, ui::edge_insets::symmetric(0.0f, 12.0f));
        const Rectangle label_rect = {content_rect.x, content_rect.y, label_width, content_rect.height};
        const Rectangle input_rect = {
            content_rect.x + label_width,
            content_rect.y + 4.0f,
            content_rect.width - label_width,
            content_rect.height - 8.0f
        };

        ui::draw_rect_f(input_rect, with_alpha(t.section, 255));
        ui::draw_rect_lines(input_rect, 1.5f, state.bar_pick_mode ? t.accent : t.border_light);
        ui::draw_text_in_rect(label, 16, label_rect,
                              selected ? t.text : t.text_secondary, ui::text_align::left);

        const char* display_value = state.bar_pick_mode ? "Click TL" : value.c_str();
        const Color value_color = state.bar_pick_mode ? t.accent : (value.empty() ? t.text_hint : t.text);
        ui::draw_text_in_rect(display_value, 16,
                              ui::inset(input_rect, ui::edge_insets::symmetric(0.0f, 10.0f)),
                              value_color, ui::text_align::left);
    };

    auto draw_input_row = [&](Rectangle rect, const char* label, ui::text_input_state& input_state,
                              editor_timing_input_field field, ui::text_input_filter filter,
                              const char* placeholder, float label_width = 84.0f) {
        const ui::text_input_result input_result = ui::draw_text_input(
            rect, input_state, label, placeholder, nullptr,
            ui::draw_layer::base, 16, 16, filter, label_width);

        if (input_result.clicked) {
            result.clicked_input_row = true;
            set_active_input(state, field);
            state.bar_pick_mode = false;
            state.input_error.clear();
        }

        if (input_result.submitted) {
            result.apply_selected = true;
            clear_active_inputs(state);
            state.bar_pick_mode = false;
        } else if (input_result.deactivated && state.active_input_field == field) {
            clear_active_inputs(state);
        }
    };

    ui::draw_section(timing_box);
    ui::draw_text_in_rect("Timing Events", 22,
                          {timing_box.x + 12.0f, timing_box.y + 10.0f, timing_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);

    const Rectangle timing_list_view_rect = {
        timing_box.x + 10.0f,
        timing_box.y + 42.0f,
        timing_box.width - 32.0f,
        timing_box.height - 88.0f
    };
    const Rectangle timing_list_scrollbar_rect = {
        timing_list_view_rect.x + timing_list_view_rect.width + 6.0f,
        timing_list_view_rect.y,
        6.0f,
        timing_list_view_rect.height
    };
    const float timing_row_height = 30.0f;
    const float timing_row_gap = 4.0f;
    const float timing_list_content_height = model.items.empty()
        ? timing_list_view_rect.height
        : static_cast<float>(model.items.size()) * timing_row_height +
              static_cast<float>(std::max<int>(0, static_cast<int>(model.items.size()) - 1)) * timing_row_gap;
    const float timing_list_max_scroll = std::max(0.0f, timing_list_content_height - timing_list_view_rect.height);
    state.list_scroll_offset = std::clamp(state.list_scroll_offset, 0.0f, timing_list_max_scroll);

    const ui::scrollbar_interaction timing_scrollbar = ui::update_vertical_scrollbar(
        timing_list_scrollbar_rect, timing_list_content_height, state.list_scroll_offset,
        state.list_scrollbar_dragging, state.list_scrollbar_drag_offset, 28.0f);
    if (timing_scrollbar.changed || timing_scrollbar.dragging) {
        state.list_scroll_offset = timing_scrollbar.scroll_offset;
    }

    if (CheckCollisionPointRec(model.mouse, timing_list_view_rect) && GetMouseWheelMove() != 0.0f) {
        state.list_scroll_offset = std::clamp(
            state.list_scroll_offset - GetMouseWheelMove() * 42.0f,
            0.0f, timing_list_max_scroll);
    }

    {
        ui::scoped_clip_rect clip_scope(timing_list_view_rect);
        float row_y = timing_list_view_rect.y - state.list_scroll_offset;
        for (const editor_timing_panel_item& item : model.items) {
            const Rectangle row_rect = {timing_list_view_rect.x, row_y, timing_list_view_rect.width, timing_row_height};
            const ui::row_state row = ui::draw_selectable_row(row_rect, item.selected, 1.5f);
            if (row.clicked) {
                result.selected_event_index = item.event_index;
            }

            ui::draw_label_value(ui::inset(row.visual, ui::edge_insets::symmetric(0.0f, 10.0f)),
                                 item.label.c_str(), item.value.c_str(), 15,
                                 item.selected ? t.text : t.text_secondary,
                                 item.selected ? t.text : t.text_muted, 118.0f);
            row_y += timing_row_height + timing_row_gap;
        }
    }
    ui::draw_scrollbar(timing_list_scrollbar_rect, timing_list_content_height, state.list_scroll_offset,
                       t.scrollbar_track, t.scrollbar_thumb, 28.0f);

    const float timing_button_gap = 8.0f;
    const float timing_button_width = (timing_box.width - 24.0f - timing_button_gap * 2.0f) / 3.0f;
    const Rectangle add_bpm_rect = {
        timing_box.x + 12.0f,
        timing_box.y + timing_box.height - 42.0f,
        timing_button_width,
        28.0f
    };
    const Rectangle add_meter_rect = {
        add_bpm_rect.x + timing_button_width + timing_button_gap,
        add_bpm_rect.y,
        timing_button_width,
        28.0f
    };
    const Rectangle delete_rect = {
        add_meter_rect.x + timing_button_width + timing_button_gap,
        add_bpm_rect.y,
        timing_button_width,
        28.0f
    };
    if (ui::draw_button(add_bpm_rect, "BPM", 14).clicked) {
        result.add_bpm = true;
    }
    if (ui::draw_button(add_meter_rect, "Meter", 14).clicked) {
        result.add_meter = true;
    }
    const ui::button_state delete_button = ui::draw_button_colored(
        delete_rect, "Delete", 14,
        model.delete_enabled ? t.row : t.section,
        model.delete_enabled ? t.row_hover : t.section,
        model.delete_enabled ? t.text : t.text_hint, 1.5f);
    if (model.delete_enabled && delete_button.clicked) {
        result.delete_selected = true;
    }

    ui::draw_section(editor_box);
    ui::draw_text_in_rect("Event Editor", 22,
                          {editor_box.x + 12.0f, editor_box.y + 10.0f, editor_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);

    if (model.selected_event.has_value()) {
        const timing_event& event = *model.selected_event;
        ui::draw_label_value({editor_box.x + 12.0f, editor_box.y + 44.0f, editor_box.width - 24.0f, 22.0f},
                             "Type", timing_event_type_label(event.type), 16,
                             t.text_secondary, t.text, 76.0f);
        if (event.type == timing_event_type::bpm) {
            draw_bar_pick_row({editor_box.x + 12.0f, editor_box.y + 74.0f, editor_box.width - 24.0f, 32.0f},
                              "Bar", state.inputs.bpm_bar.value, editor_timing_input_field::bpm_measure);
            draw_input_row({editor_box.x + 12.0f, editor_box.y + 112.0f, editor_box.width - 24.0f, 32.0f},
                           "BPM", state.inputs.bpm_value, editor_timing_input_field::bpm_value,
                           accepts_float_character, "BPM");
        } else {
            draw_bar_pick_row({editor_box.x + 12.0f, editor_box.y + 74.0f, editor_box.width - 24.0f, 32.0f},
                              "Bar", state.inputs.meter_bar.value, editor_timing_input_field::meter_measure);
            draw_input_row({editor_box.x + 12.0f, editor_box.y + 112.0f, (editor_box.width - 32.0f) * 0.5f, 32.0f},
                           "Num", state.inputs.meter_numerator, editor_timing_input_field::meter_numerator,
                           accepts_int_character, "Num", 40.0f);
            draw_input_row({editor_box.x + 20.0f + (editor_box.width - 32.0f) * 0.5f, editor_box.y + 112.0f,
                            (editor_box.width - 32.0f) * 0.5f, 32.0f},
                           "Den", state.inputs.meter_denominator, editor_timing_input_field::meter_denominator,
                           accepts_int_character, "Den", 40.0f);
        }

        if (!state.input_error.empty()) {
            ui::draw_text_in_rect(state.input_error.c_str(), 16,
                                  {editor_box.x + 12.0f, editor_box.y + 128.0f, editor_box.width - 24.0f, 24.0f},
                                  t.error, ui::text_align::left);
        }
    } else {
        ui::draw_text_in_rect("Select a timing event from the list.", 18,
                              {editor_box.x + 12.0f, editor_box.y + 54.0f, editor_box.width - 24.0f, 24.0f},
                              t.text_hint, ui::text_align::left);
    }

    return result;
}
