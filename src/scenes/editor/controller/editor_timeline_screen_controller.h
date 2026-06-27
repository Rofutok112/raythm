#pragma once

#include <vector>

#include "audio_waveform.h"
#include "editor/daw/editor_daw_types.h"
#include "editor/editor_meter_map.h"
#include "editor/editor_scene_types.h"
#include "editor/editor_state.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "raylib.h"

namespace editor_timeline_screen_controller {

struct context {
    editor_state& state;
    editor_meter_map& meter_map;
    const audio_waveform_summary& waveform_summary;
    bool waveform_visible = false;
    int waveform_offset_ms = 0;
    editor_transport_state& transport;
    editor_timeline_viewport_state& viewport;
    bool snap_dropdown_open = false;
    const std::vector<size_t>& selected_note_indices;
    editor_timing_panel_state& timing_panel;
    const editor_timeline_note_drag_state& timeline_drag;
    const editor_note_palette_selection& note_palette;
};

struct cursor_context {
    const editor_state& state;
    const editor_timeline_note_drag_state& timeline_drag;
    const std::vector<size_t>& selected_note_indices;
    const editor_timeline_metrics& metrics;
    Vector2 mouse{};
    bool blocking_modal = false;
};

editor_right_panel_view_result draw(const context& context);

int mouse_cursor(const cursor_context& context);

}  // namespace editor_timeline_screen_controller
