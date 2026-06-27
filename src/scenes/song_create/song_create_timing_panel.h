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
    float& event_scroll_offset;
    bool& event_scrollbar_dragging;
    float& event_scrollbar_drag_offset;
    std::string& import_status;
    std::string& error;
};

struct callbacks {
    std::function<void()> ensure_initialized;
};

struct config {
    Rectangle screen_rect;
    Rectangle modal_rect;
    float text_input_label_width = 180.0f;
    ui::draw_layer base_layer = ui::draw_layer::base;
    ui::draw_layer modal_layer = ui::draw_layer::modal;
};

struct summary_result {
    bool open_requested = false;
};

struct editor_result {
    bool event_scroll_changed = false;
    float event_scroll_offset = 0.0f;
    bool event_scrollbar_drag_state_changed = false;
    bool event_scrollbar_dragging = false;
    float event_scrollbar_drag_offset = 0.0f;
    std::optional<size_t> selected_event_index;
    std::optional<timing_event_type> add_event_type;
    bool delete_selected_event_requested = false;
    bool start_metronome_requested = false;
    bool stop_metronome_requested = false;
};

struct modal_result {
    bool import_midi_requested = false;
    bool close_requested = false;
    editor_result editor;
};

[[nodiscard]] summary_result draw_summary(Rectangle rect,
                                          state_refs state,
                                          const callbacks& actions,
                                          const config& view_config);
[[nodiscard]] modal_result draw_modal(state_refs state, const callbacks& actions, const config& view_config);
[[nodiscard]] editor_result draw_editor(Rectangle rect,
                                        ui::draw_layer layer,
                                        state_refs state,
                                        const callbacks& actions);

}  // namespace song_create::timing_panel
