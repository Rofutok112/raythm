#include "editor_timing_panel.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {
const char* timing_event_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Time Sig";
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
    state.inputs.scroll_duration.active = field == editor_timing_input_field::scroll_duration;
    state.inputs.scroll_multiplier.active = field == editor_timing_input_field::scroll_multiplier;
    state.inputs.bpm_bar.active = false;
    state.inputs.meter_bar.active = false;
    state.inputs.scroll_start_bar.active = false;
}

void clear_active_inputs(editor_timing_panel_state& state) {
    set_active_input(state, editor_timing_input_field::none);
}

void draw_inspector_tabs(Rectangle rect, const char* active_label) {
    const auto& t = *g_theme;
    ui::surface(rect, with_alpha(t.panel, 220), t.border_light, 1.2f);

    std::array<Rectangle, 3> tabs{};
    ui::hstack_fill(rect, 4.0f, tabs);
    const char* labels[] = {"Selection", "Timing", "Scroll"};
    for (int i = 0; i < 3; ++i) {
        const bool active = std::string(labels[i]) == active_label;
        const Rectangle tab = tabs[static_cast<size_t>(i)];
        ui::surface_fill(ui::inset(tab, 1.0f), active ? t.row_selected : t.row);
        if (active) {
            ui::accent_bar({tab.x + 8.0f, tab.y + tab.height - 3.0f, tab.width - 16.0f, 2.0f}, t.accent);
        }
        ui::draw_text_in_rect(labels[i], 13, tab, active ? t.text : t.text_muted);
    }
}

void draw_section_heading(Rectangle box, const char* title, const char* tab_label) {
    const auto& t = *g_theme;
    ui::draw_text_in_rect(title, 20,
                          {box.x + 12.0f, box.y + 10.0f, box.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);
    const Rectangle badge = {box.x + box.width - 88.0f, box.y + 12.0f, 74.0f, 20.0f};
    ui::surface(badge, with_alpha(t.row, 230), t.border_light, 1.0f);
    ui::draw_text_in_rect(tab_label, 12, badge, t.text_muted);
    ui::divider({box.x + 12.0f, box.y + 38.0f, box.width - 24.0f, 1.5f}, t.border_light);
}

void draw_action_chip(Rectangle rect, const char* label, bool enabled) {
    const auto& t = *g_theme;
    ui::surface(rect,
                enabled ? with_alpha(t.row, 245) : with_alpha(t.section, 230),
                enabled ? t.border_light : with_alpha(t.border_light, 120),
                1.0f);
    ui::draw_text_in_rect(label, 13, rect, enabled ? t.text_secondary : t.text_hint);
}
}

editor_timing_panel_result editor_timing_panel::draw(const editor_timing_panel_model& model, editor_timing_panel_state& state) {
    const auto& t = *g_theme;
    editor_timing_panel_result result;

    const Rectangle selection_box = {model.content_rect.x, model.content_rect.y, model.content_rect.width, 116.0f};
    const Rectangle timing_box = {model.content_rect.x, selection_box.y + selection_box.height + 12.0f,
                                  model.content_rect.width, 220.0f};
    const Rectangle scroll_box = {model.content_rect.x, timing_box.y + timing_box.height + 12.0f,
                                  model.content_rect.width, 252.0f};
    const Rectangle editor_box = {model.content_rect.x, scroll_box.y + scroll_box.height + 12.0f,
                                  model.content_rect.width,
                                  model.content_rect.y + model.content_rect.height - (scroll_box.y + scroll_box.height + 12.0f)};

    auto draw_bar_pick_row = [&](Rectangle rect, const char* label, const std::string& value,
                                 editor_timing_input_field field, float label_width = 84.0f) {
        const bool selected = state.active_input_field == field || state.bar_pick_mode;
        const ui::row_state row = ui::row(rect, {
            .border_width = 1.5f,
            .bg = selected ? t.row_selected : t.row,
            .bg_hover = selected ? t.row_selected_hover : t.row_hover,
            .border_color = state.bar_pick_mode ? t.accent : (selected ? t.border_active : t.border),
            .custom_colors = true,
        });
        if (row.clicked) {
            result.clicked_input_row = true;
            state.active_input_field = field;
            state.bar_pick_mode = true;
            state.input_error.clear();
            state.inputs.bpm_value.active = false;
            state.inputs.meter_numerator.active = false;
            state.inputs.meter_denominator.active = false;
            state.inputs.scroll_duration.active = false;
            state.inputs.scroll_multiplier.active = false;
        }

        const Rectangle content_rect = ui::inset(row.visual, ui::edge_insets::symmetric(0.0f, 12.0f));
        const Rectangle label_rect = {content_rect.x, content_rect.y, label_width, content_rect.height};
        const Rectangle input_rect = {
            content_rect.x + label_width,
            content_rect.y + 4.0f,
            content_rect.width - label_width,
            content_rect.height - 8.0f
        };

        ui::surface(input_rect,
                    with_alpha(t.section, 255),
                    state.bar_pick_mode ? t.accent : t.border_light,
                    1.5f);
        ui::draw_text_in_rect(label, 16, label_rect,
                              selected ? t.text : t.text_secondary, ui::text_align::left);

        const char* display_value = state.bar_pick_mode ? "Pick timeline" : value.c_str();
        const Color value_color = state.bar_pick_mode ? t.accent : (value.empty() ? t.text_hint : t.text);
        ui::draw_text_in_rect(display_value, 16,
                              ui::inset(input_rect, ui::edge_insets::symmetric(0.0f, 10.0f)),
                              value_color, ui::text_align::left);
    };

    auto draw_input_row = [&](Rectangle rect, const char* label, ui::text_input_state& input_state,
                              editor_timing_input_field field, ui::text_input_filter filter,
                              const char* placeholder, float label_width = 84.0f) {
        const ui::text_input_result input_result = ui::text_input(
            rect, input_state, label, placeholder, {
                .font_size = 16,
                .max_length = 16,
                .filter = filter,
                .label_width = label_width,
            });

        if (input_result.clicked) {
            result.clicked_input_row = true;
            set_active_input(state, field);
            state.bar_pick_mode = false;
            state.input_error.clear();
        }

        if (input_result.submitted) {
            if (field == editor_timing_input_field::scroll_duration ||
                field == editor_timing_input_field::scroll_multiplier) {
                result.apply_selected_scroll = true;
            } else {
                result.apply_selected = true;
            }
            clear_active_inputs(state);
            state.bar_pick_mode = false;
        } else if (input_result.deactivated && state.active_input_field == field) {
            clear_active_inputs(state);
        }
    };

    auto draw_event_list = [&](const Rectangle& box,
                               const char* title,
                               const std::vector<editor_timing_panel_item>& items,
                               float& scroll_offset,
                               bool& scrollbar_dragging,
                               float& scrollbar_drag_offset,
                               bool automation_items) {
        ui::section(box);
        draw_section_heading(box, title, automation_items ? "Scroll" : "Timing");

        const Rectangle list_view_rect = {
            box.x + 10.0f,
            box.y + 50.0f,
            box.width - 32.0f,
            box.height - 96.0f
        };
        const Rectangle scrollbar_rect = {
            list_view_rect.x + list_view_rect.width + 6.0f,
            list_view_rect.y,
            6.0f,
            list_view_rect.height
        };
        const float row_height = 30.0f;
        const float row_gap = 4.0f;
        const float content_height = items.empty()
            ? list_view_rect.height
            : static_cast<float>(items.size()) * row_height +
                  static_cast<float>(std::max<int>(0, static_cast<int>(items.size()) - 1)) * row_gap;
        const float max_scroll = std::max(0.0f, content_height - list_view_rect.height);
        scroll_offset = std::clamp(scroll_offset, 0.0f, max_scroll);

        const ui::scrollbar_interaction scrollbar = ui::vertical_scrollbar(
            scrollbar_rect, content_height, scroll_offset, scrollbar_dragging, scrollbar_drag_offset, {
                .min_thumb_height = 28.0f,
                .drag_blocked_by_layer = false,
            });
        if (scrollbar.changed || scrollbar.dragging) {
            scroll_offset = scrollbar.scroll_offset;
        }

        if (ui::contains_point(list_view_rect, model.mouse) && GetMouseWheelMove() != 0.0f) {
            scroll_offset = std::clamp(scroll_offset - GetMouseWheelMove() * 42.0f, 0.0f, max_scroll);
        }

        {
            ui::scoped_clip_rect clip_scope(list_view_rect);
            float row_y = list_view_rect.y - scroll_offset;
            for (const editor_timing_panel_item& item : items) {
                const Rectangle row_rect = {list_view_rect.x, row_y, list_view_rect.width, row_height};
                const ui::row_state row = ui::selectable_row(row_rect, item.selected, 1.5f);
                if (row.clicked) {
                    if (automation_items) {
                        result.selected_scroll_event_index = item.event_index;
                    } else {
                        result.selected_event_index = item.event_index;
                    }
                }

                ui::draw_label_value(ui::inset(row.visual, ui::edge_insets::symmetric(0.0f, 10.0f)),
                                     item.label.c_str(), item.value.c_str(), 15,
                                     item.selected ? t.text : t.text_secondary,
                                     item.selected ? t.text : t.text_muted, 118.0f);
                row_y += row_height + row_gap;
            }
        }
        ui::scrollbar(scrollbar_rect, content_height, scroll_offset, {
            .track_color = t.scrollbar_track,
            .thumb_color = t.scrollbar_thumb,
            .min_thumb_height = 28.0f,
            .custom_colors = true,
        });
    };

    ui::section(selection_box);
    draw_section_heading(selection_box, "Selection", "Notes");
    const bool has_notes = model.selected_note_count > 0;
    const Rectangle count_rect = {selection_box.x + 12.0f, selection_box.y + 50.0f, 118.0f, 42.0f};
    ui::surface(count_rect,
                has_notes ? with_alpha(t.row_selected, 245) : with_alpha(t.row, 210),
                has_notes ? t.border_active : t.border_light,
                1.5f);
    ui::draw_text_in_rect(has_notes ? TextFormat("%d", static_cast<int>(model.selected_note_count)) : "-",
                          26, count_rect, has_notes ? t.text : t.text_muted);
    ui::draw_text_in_rect(model.selected_note_summary.empty() ? "No notes selected" : model.selected_note_summary.c_str(),
                          15,
                          {selection_box.x + 142.0f, selection_box.y + 50.0f,
                           selection_box.width - 154.0f, 20.0f},
                          has_notes ? t.text : t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect("Move / copy / duplicate as a grouped edit",
                          13,
                          {selection_box.x + 142.0f, selection_box.y + 72.0f,
                           selection_box.width - 154.0f, 18.0f},
                          t.text_hint, ui::text_align::left);
    const float chip_y = selection_box.y + selection_box.height - 30.0f;
    std::array<Rectangle, 3> action_chips{};
    ui::hstack_fill({
        selection_box.x + 12.0f,
        chip_y,
        selection_box.width - 24.0f,
        22.0f,
    }, 6.0f, action_chips);
    draw_action_chip(action_chips[0], "Copy", has_notes);
    draw_action_chip(action_chips[1], "Duplicate", has_notes);
    draw_action_chip(action_chips[2], "Delete", has_notes);

    draw_event_list(timing_box, "Timing Map", model.items,
                    state.list_scroll_offset, state.list_scrollbar_dragging,
                    state.list_scrollbar_drag_offset, false);

    std::array<Rectangle, 3> timing_buttons{};
    ui::hstack_fill({
        timing_box.x + 12.0f,
        timing_box.y + timing_box.height - 42.0f,
        timing_box.width - 24.0f,
        28.0f,
    }, 8.0f, timing_buttons);
    const Rectangle add_bpm_rect = timing_buttons[0];
    const Rectangle add_meter_rect = timing_buttons[1];
    const Rectangle delete_rect = timing_buttons[2];
    if (ui::button(add_bpm_rect, "Add BPM", {.font_size = 14}).clicked) {
        result.add_bpm = true;
    }
    if (ui::button(add_meter_rect, "Add Time Sig", {.font_size = 14}).clicked) {
        result.add_meter = true;
    }
    const ui::button_state delete_button = ui::action_button(delete_rect, "Delete", {
        .font_size = 14,
        .border_width = 1.5f,
        .enabled = model.delete_enabled,
        .disabled_text_color = t.text_hint,
        .disabled_border_color = t.border,
    });
    if (model.delete_enabled && delete_button.clicked) {
        result.delete_selected = true;
    }

    draw_event_list(scroll_box, "Automation", model.scroll_items,
                    state.scroll_list_scroll_offset, state.scroll_list_scrollbar_dragging,
                    state.scroll_list_scrollbar_drag_offset, true);

    std::array<Rectangle, 3> scroll_buttons{};
    ui::hstack_fill({
        scroll_box.x + 12.0f,
        scroll_box.y + scroll_box.height - 42.0f,
        scroll_box.width - 24.0f,
        28.0f,
    }, 8.0f, scroll_buttons);
    const Rectangle add_speed_rect = scroll_buttons[0];
    const Rectangle add_stop_rect = scroll_buttons[1];
    const Rectangle delete_scroll_rect = scroll_buttons[2];
    if (ui::button(add_speed_rect, "Add Point", {.font_size = 14}).clicked) {
        result.add_speed = true;
    }
    if (ui::button(add_stop_rect, "Curve", {.font_size = 14}).clicked) {
        result.cycle_selected_scroll_curve = true;
    }
    const ui::button_state delete_scroll_button = ui::action_button(delete_scroll_rect, "Delete", {
        .font_size = 14,
        .border_width = 1.5f,
        .enabled = model.scroll_delete_enabled,
        .disabled_text_color = t.text_hint,
        .disabled_border_color = t.border,
    });
    if (model.scroll_delete_enabled && delete_scroll_button.clicked) {
        result.delete_selected_scroll = true;
    }

    ui::section(editor_box);
    const char* active_tab = model.selected_scroll_event.has_value()
        ? "Scroll"
        : (model.selected_event.has_value() ? "Timing" : "Selection");
    draw_inspector_tabs({editor_box.x + 12.0f, editor_box.y + 10.0f, editor_box.width - 24.0f, 28.0f}, active_tab);
    const char* editor_title = model.selected_scroll_event.has_value()
        ? "Automation Point"
        : (model.selected_event.has_value() ? "Timing Event" : "Inspector");
    ui::draw_text_in_rect(editor_title, 20,
                          {editor_box.x + 12.0f, editor_box.y + 46.0f, editor_box.width - 24.0f, 24.0f},
                          t.text, ui::text_align::left);

    if (model.selected_scroll_event.has_value()) {
        const scroll_automation_point& point = *model.selected_scroll_event;
        ui::draw_label_value({editor_box.x + 12.0f, editor_box.y + 76.0f, editor_box.width - 24.0f, 22.0f},
                             "Mode", "Automation", 16,
                             t.text_secondary, t.text, 76.0f);
        draw_bar_pick_row({editor_box.x + 12.0f, editor_box.y + 106.0f, editor_box.width - 24.0f, 32.0f},
                          "Start", state.inputs.scroll_start_bar.value, editor_timing_input_field::scroll_start);
        draw_input_row({editor_box.x + 12.0f, editor_box.y + 144.0f, editor_box.width - 24.0f, 32.0f},
                       "Rate", state.inputs.scroll_multiplier, editor_timing_input_field::scroll_multiplier,
                       accepts_float_character, "1.0x");
        ui::draw_label_value({editor_box.x + 12.0f, editor_box.y + 182.0f, editor_box.width - 24.0f, 22.0f},
                             "Tick", TextFormat("%d", point.tick), 16,
                             t.text_secondary, t.text, 76.0f);
        if (!state.input_error.empty()) {
            ui::draw_text_in_rect(state.input_error.c_str(), 16,
                                  {editor_box.x + 12.0f, editor_box.y + editor_box.height - 32.0f,
                                   editor_box.width - 24.0f, 24.0f},
                                  t.error, ui::text_align::left);
        }
    } else if (model.selected_event.has_value()) {
        const timing_event& event = *model.selected_event;
        ui::draw_label_value({editor_box.x + 12.0f, editor_box.y + 76.0f, editor_box.width - 24.0f, 22.0f},
                             "Type", timing_event_type_label(event.type), 16,
                             t.text_secondary, t.text, 76.0f);
        if (event.type == timing_event_type::bpm) {
            draw_bar_pick_row({editor_box.x + 12.0f, editor_box.y + 106.0f, editor_box.width - 24.0f, 32.0f},
                              "Bar", state.inputs.bpm_bar.value, editor_timing_input_field::bpm_measure);
            draw_input_row({editor_box.x + 12.0f, editor_box.y + 144.0f, editor_box.width - 24.0f, 32.0f},
                           "BPM", state.inputs.bpm_value, editor_timing_input_field::bpm_value,
                           accepts_float_character, "BPM");
        } else {
            draw_bar_pick_row({editor_box.x + 12.0f, editor_box.y + 106.0f, editor_box.width - 24.0f, 32.0f},
                              "Bar", state.inputs.meter_bar.value, editor_timing_input_field::meter_measure);
            draw_input_row({editor_box.x + 12.0f, editor_box.y + 144.0f, (editor_box.width - 32.0f) * 0.5f, 32.0f},
                           "Num", state.inputs.meter_numerator, editor_timing_input_field::meter_numerator,
                           accepts_int_character, "Num", 40.0f);
            draw_input_row({editor_box.x + 20.0f + (editor_box.width - 32.0f) * 0.5f, editor_box.y + 144.0f,
                            (editor_box.width - 32.0f) * 0.5f, 32.0f},
                           "Den", state.inputs.meter_denominator, editor_timing_input_field::meter_denominator,
                           accepts_int_character, "Den", 40.0f);
        }

        if (!state.input_error.empty()) {
            ui::draw_text_in_rect(state.input_error.c_str(), 16,
                                  {editor_box.x + 12.0f, editor_box.y + editor_box.height - 32.0f,
                                   editor_box.width - 24.0f, 24.0f},
                                  t.error, ui::text_align::left);
        }
    } else {
        ui::draw_text_in_rect("Select a song timing or scroll event.", 18,
                              {editor_box.x + 12.0f, editor_box.y + 82.0f, editor_box.width - 24.0f, 24.0f},
                              t.text_hint, ui::text_align::left);
    }

    return result;
}
