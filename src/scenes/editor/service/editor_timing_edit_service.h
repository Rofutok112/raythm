#pragma once

#include <optional>

#include "editor/editor_meter_map.h"
#include "editor/editor_state.h"
#include "editor/editor_timing_panel.h"

struct editor_timing_edit_context {
    editor_state& state;
    const editor_meter_map& meter_map;
    editor_timing_panel_state& timing_panel;
    int default_timing_event_tick = 0;
};

struct editor_timing_delete_query {
    const editor_state& state;
    const editor_timing_panel_state& timing_panel;
};

struct editor_timing_edit_result {
    bool success = false;
    std::optional<int> scroll_to_tick;
    std::optional<size_t> selected_event_index;
};

class editor_timing_edit_service final {
public:
    static bool can_delete_selected(const editor_timing_delete_query& query);
    static editor_timing_edit_result apply_selected(editor_timing_edit_context context);
    static editor_timing_edit_result add_event(editor_timing_edit_context context, timing_event_type type);
    static editor_timing_edit_result delete_selected(editor_timing_edit_context context);
};
