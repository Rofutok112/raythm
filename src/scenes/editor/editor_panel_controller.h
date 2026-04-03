#pragma once

#include "editor_scene_types.h"

class editor_panel_controller final {
public:
    static editor_metadata_panel_result update_metadata_panel(metadata_panel_state& metadata_panel,
                                                              editor_timing_panel_state& timing_panel,
                                                              const editor_metadata_panel_actions& actions);
    static editor_timing_panel_update_result update_timing_panel(metadata_panel_state& metadata_panel,
                                                                 editor_timing_panel_state& timing_panel,
                                                                 const editor_timing_panel_actions& actions);
};
