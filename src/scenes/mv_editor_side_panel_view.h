#pragma once

#include <string>

#include "mv/composition/mv_composition.h"
#include "raylib.h"

struct mv_editor_hierarchy_result {
    float scroll_offset = 0.0f;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
    std::string selected_layer_id;
    int move_direction = 0;
};

struct mv_editor_project_panel_result {
    float scroll_offset = 0.0f;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
    std::string selected_asset_id;
    std::string assign_asset_id;
};

mv_editor_hierarchy_result draw_mv_hierarchy_panel(Rectangle panel,
                                                   const mv::composition::mv_composition& composition,
                                                   const std::string& selected_layer_id,
                                                   float scroll_offset,
                                                   bool scrollbar_dragging,
                                                   float scrollbar_drag_offset,
                                                   Vector2 mouse,
                                                   float wheel,
                                                   bool shift_down,
                                                   bool ctrl_down);

mv_editor_project_panel_result draw_mv_project_panel(Rectangle panel,
                                                     const mv::composition::mv_composition& composition,
                                                     const std::string& selected_asset_id,
                                                     float scroll_offset,
                                                     bool scrollbar_dragging,
                                                     float scrollbar_drag_offset,
                                                     Vector2 mouse,
                                                     float wheel,
                                                     bool shift_down,
                                                     bool ctrl_down);
