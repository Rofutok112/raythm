#pragma once

#include "audio_waveform.h"
#include "editor/editor_scene_sync.h"
#include "editor/editor_scene_types.h"
#include "editor/viewport/editor_timeline_viewport.h"

namespace editor_timing_action_controller {

struct context {
    editor_state& state;
    editor_meter_map& meter_map;
    editor_timing_panel_state& timing_panel;
    metadata_panel_state& metadata_panel;
    std::vector<size_t>& selected_note_indices;
    editor_transport_state& transport;
    editor_timeline_viewport_state& viewport;
    editor_timeline_viewport_model viewport_model;
    const std::string& hitsound_path;
    const editor_hitsound_paths& hitsounds;
};

int default_timing_event_tick(const context& context);
bool apply_selected_timing_event(const context& context);
bool apply_selected_scroll_event(const context& context);
void cycle_selected_scroll_curve(const context& context);
void add_timing_event(const context& context, timing_event_type type);
void add_scroll_event(const context& context);
void delete_selected_timing_event(const context& context);
void delete_selected_scroll_event(const context& context);
bool can_delete_selected_timing_event(const editor_state& state, const editor_timing_panel_state& timing_panel);
bool can_delete_selected_scroll_event(const editor_state& state, const editor_timing_panel_state& timing_panel);

}  // namespace editor_timing_action_controller
