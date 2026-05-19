#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "editor/editor_meter_map.h"
#include "editor/editor_scene_types.h"
#include "editor/editor_timing_panel.h"

struct editor_right_panel_view_model {
    const std::vector<timing_event>* timing_events = nullptr;
    const std::vector<scroll_event>* scroll_events = nullptr;
    const std::vector<scroll_automation_point>* scroll_automation = nullptr;
    const editor_meter_map* meter_map = nullptr;
    std::optional<size_t> selected_event_index;
    std::optional<size_t> selected_scroll_event_index;
    size_t selected_note_count = 0;
    std::string selected_note_summary;
    bool delete_enabled = false;
    bool scroll_delete_enabled = false;
    Vector2 mouse = {};
};

struct editor_right_panel_view_result {
    editor_timing_panel_result panel_result;
    std::optional<scroll_automation_point> scroll_automation_point_to_add;
    std::optional<std::pair<size_t, scroll_automation_point>> scroll_automation_point_to_modify;
    bool clicked_outside_editor = false;
};

class editor_right_panel_view final {
public:
    static editor_right_panel_view_result draw(const editor_right_panel_view_model& model,
                                               editor_timing_panel_state& timing_state);
};
