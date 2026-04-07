#pragma once

#include <optional>
#include <vector>

#include "editor/editor_meter_map.h"
#include "editor/editor_mv_script_panel.h"
#include "editor/editor_scene_types.h"
#include "editor/editor_timing_panel.h"

struct editor_right_panel_view_model {
    editor_right_panel_tab active_tab = editor_right_panel_tab::timing;
    const std::vector<timing_event>* timing_events = nullptr;
    const editor_meter_map* meter_map = nullptr;
    std::optional<size_t> selected_event_index;
    bool delete_enabled = false;
    Vector2 mouse = {};
};

struct editor_right_panel_view_result {
    editor_timing_panel_result panel_result;
    bool clicked_outside_editor = false;
    bool timing_tab_clicked = false;
    bool mv_script_tab_clicked = false;
    editor_mv_script_panel_result mv_script_result;
};

class editor_right_panel_view final {
public:
    static editor_right_panel_view_result draw(const editor_right_panel_view_model& model,
                                               editor_timing_panel_state& timing_state,
                                               editor_mv_script_panel_state& mv_script_state);
};
