#pragma once

#include <string>

#include "editor/editor_scene_types.h"
#include "editor/editor_state.h"

struct editor_metadata_apply_context {
    editor_state& state;
    metadata_panel_state& metadata_panel;
    bool clear_notes_for_key_count_change = false;
    std::string generated_chart_id;
};

struct editor_metadata_apply_result {
    bool success = false;
    bool key_count_changed = false;
    bool confirmation_required = false;
};

class editor_metadata_service final {
public:
    static editor_metadata_apply_result apply_changes(editor_metadata_apply_context context);
    static bool apply_chart_offset(editor_state& state, int offset_ms);
};
