#pragma once

#include <optional>

#include "editor/editor_scene_sync.h"
#include "editor/editor_scene_types.h"

struct editor_shortcut_context {
    editor_state& state;
    editor_scene_sync_context sync_context;
    const metadata_panel_state& metadata_panel;
    const editor_timing_panel_state& timing_panel;
    const editor_timeline_note_drag_state& timeline_drag;
    std::optional<size_t>& selected_note_index;
    editor_transport_state& transport;
    std::optional<int>& space_playback_start_tick;
    const std::string& hitsound_path;
    bool blocking_modal = false;
    bool mv_script_editor_active = false;
    bool space_pressed = false;
    bool ctrl_down = false;
    bool shift_down = false;
    bool z_pressed = false;
    bool y_pressed = false;
    bool delete_pressed = false;
};

struct editor_shortcut_result {
    std::optional<int> restore_scroll_tick;
};

struct editor_runtime_timeline_context {
    editor_state& state;
    editor_meter_map& meter_map;
    editor_timing_panel_state& timing_panel;
    editor_transport_state& transport;
    std::optional<int>& space_playback_start_tick;
    std::optional<size_t>& selected_note_index;
    editor_timeline_note_drag_state& drag_state;
    const std::string& hitsound_path;
    editor_timeline_metrics metrics;
    Vector2 mouse = {};
    bool timeline_hovered = false;
    bool left_pressed = false;
    bool left_down = false;
    bool left_released = false;
    bool right_pressed = false;
    bool escape_pressed = false;
    bool alt_down = false;
    int snap_division = 1;
};

struct editor_runtime_timeline_result {
    bool request_apply_selected_timing = false;
    std::optional<int> scroll_to_tick;
};

class editor_runtime_controller final {
public:
    static editor_shortcut_result handle_shortcuts(const editor_shortcut_context& context);
    static editor_runtime_timeline_result handle_timeline_interaction(const editor_runtime_timeline_context& context);
};
