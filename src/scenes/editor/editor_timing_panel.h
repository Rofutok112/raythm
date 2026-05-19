#pragma once

#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "raylib.h"
#include "ui_text_input.h"

enum class editor_timing_input_field {
    none,
    bpm_measure,
    bpm_value,
    meter_measure,
    meter_numerator,
    meter_denominator,
    scroll_start,
    scroll_duration,
    scroll_multiplier,
};

struct editor_timing_event_editor_inputs {
    ui::text_input_state bpm_bar;
    ui::text_input_state bpm_value;
    ui::text_input_state meter_bar;
    ui::text_input_state meter_numerator;
    ui::text_input_state meter_denominator;
    ui::text_input_state scroll_start_bar;
    ui::text_input_state scroll_duration;
    ui::text_input_state scroll_multiplier;
};

struct editor_timing_panel_state {
    std::optional<size_t> selected_event_index;
    std::optional<size_t> selected_scroll_event_index;
    editor_timing_input_field active_input_field = editor_timing_input_field::none;
    editor_timing_event_editor_inputs inputs;
    std::string input_error;
    bool bar_pick_mode = false;
    float list_scroll_offset = 0.0f;
    bool list_scrollbar_dragging = false;
    float list_scrollbar_drag_offset = 0.0f;
    float scroll_list_scroll_offset = 0.0f;
    bool scroll_list_scrollbar_dragging = false;
    float scroll_list_scrollbar_drag_offset = 0.0f;
};

struct editor_timing_panel_item {
    size_t event_index = 0;
    std::string label;
    std::string value;
    bool selected = false;
};

struct editor_timing_panel_model {
    Rectangle content_rect = {};
    Vector2 mouse = {};
    std::vector<editor_timing_panel_item> items;
    std::vector<editor_timing_panel_item> scroll_items;
    std::optional<timing_event> selected_event;
    std::optional<scroll_event> selected_scroll_event;
    size_t selected_note_count = 0;
    std::string selected_note_summary;
    bool delete_enabled = false;
    bool scroll_delete_enabled = false;
};

struct editor_timing_panel_result {
    bool clicked_input_row = false;
    bool add_bpm = false;
    bool add_meter = false;
    bool add_speed = false;
    bool add_stop = false;
    bool delete_selected = false;
    bool delete_selected_scroll = false;
    bool apply_selected = false;
    bool apply_selected_scroll = false;
    std::optional<size_t> selected_event_index;
    std::optional<size_t> selected_scroll_event_index;
};

class editor_timing_panel {
public:
    static editor_timing_panel_result draw(const editor_timing_panel_model& model, editor_timing_panel_state& state);
};
