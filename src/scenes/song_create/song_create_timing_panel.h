#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "raylib.h"
#include "ui_draw.h"
#include "ui_text_input.h"

namespace song_create::timing_panel {

struct state_refs {
    ui::text_input_state& bpm_input;
    ui::text_input_state& offset_input;
    ui::text_input_state& bar_input;
    ui::text_input_state& event_bpm_input;
    ui::text_input_state& numerator_input;
    ui::text_input_state& denominator_input;
    std::vector<timing_event>& events;
    std::optional<size_t>& selected_event_index;
    bool& metronome_enabled;
    bool& modal_open;
    float& event_scroll_offset;
    bool& event_scrollbar_dragging;
    float& event_scrollbar_drag_offset;
    std::string& import_status;
    std::string& error;
};

struct callbacks {
    std::function<void()> ensure_initialized;
    std::function<bool()> flush_selected_inputs;
    std::function<void()> sync_selected_inputs;
    std::function<void(timing_event_type)> add_event;
    std::function<void()> delete_selected_event;
    std::function<bool(const std::string&)> import_midi;
    std::function<void()> close_modal;
    std::function<bool()> start_preview;
    std::function<void()> stop_preview;
};

struct config {
    Rectangle screen_rect;
    Rectangle modal_rect;
    float text_input_label_width = 180.0f;
    ui::draw_layer base_layer = ui::draw_layer::base;
    ui::draw_layer modal_layer = ui::draw_layer::modal;
};

void draw_summary(Rectangle rect, state_refs state, const callbacks& actions, const config& view_config);
void draw_modal(state_refs state, const callbacks& actions, const config& view_config);
void draw_editor(Rectangle rect, ui::draw_layer layer, state_refs state, const callbacks& actions);

}  // namespace song_create::timing_panel
