#pragma once

#include "editor_scene_types.h"

class editor_timeline_controller final {
public:
    static editor_timeline_result update(editor_timing_panel_state& timing_panel, const editor_timeline_context& context);
};
