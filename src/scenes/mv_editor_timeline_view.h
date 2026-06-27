#pragma once

#include <string>

#include "mv/composition/mv_composition.h"
#include "raylib.h"

enum class mv_editor_timeline_layer_row_action {
    none,
    select,
    toggle_visibility,
    toggle_lock,
    delete_layer,
};

struct mv_editor_timeline_layer_row_result {
    mv_editor_timeline_layer_row_action action = mv_editor_timeline_layer_row_action::none;
    std::string layer_id;
};

struct mv_editor_timeline_scrub_result {
    bool requested = false;
    double playhead_ms = 0.0;
};

enum class mv_editor_timeline_drag_update_mode {
    none,
    move,
    trim_start,
    trim_end,
};

struct mv_editor_timeline_drag_update_result {
    bool active = false;
    std::string layer_id;
    mv_editor_timeline_drag_update_mode mode = mv_editor_timeline_drag_update_mode::none;
    double start_ms = 0.0;
    double duration_ms = 0.0;
    double playhead_ms = 0.0;
};

struct mv_editor_timeline_drag_start_result {
    bool started = false;
    std::string layer_id;
    mv_editor_timeline_drag_update_mode mode = mv_editor_timeline_drag_update_mode::none;
    float origin_mouse_x = 0.0f;
    double origin_start_ms = 0.0;
    double origin_duration_ms = 0.0;
};

struct mv_editor_timeline_drag_end_result {
    bool ended = false;
};

struct mv_editor_timeline_drag_view_state {
    bool active = false;
    std::string layer_id;
    mv_editor_timeline_drag_update_mode mode = mv_editor_timeline_drag_update_mode::none;
    float origin_mouse_x = 0.0f;
    double origin_start_ms = 0.0;
    double origin_duration_ms = 0.0;
};

struct mv_editor_timeline_view_state {
    float vertical_scroll_offset = 0.0f;
    double horizontal_scroll_ms = 0.0;
    float zoom = 1.0f;
    mv_editor_timeline_drag_view_state drag;
};

struct mv_editor_timeline_view_result {
    float vertical_scroll_offset = 0.0f;
    double horizontal_scroll_ms = 0.0;
    float zoom = 1.0f;
    mv_editor_timeline_layer_row_result layer_row;
    mv_editor_timeline_layer_row_result delete_layer;
    mv_editor_timeline_drag_start_result drag_start;
    mv_editor_timeline_drag_update_result drag_update;
    mv_editor_timeline_drag_end_result drag_end;
    mv_editor_timeline_scrub_result scrub;
};

mv_editor_timeline_view_result draw_mv_timeline_view(Rectangle panel,
                                                     const mv::composition::mv_composition& composition,
                                                     const std::string& selected_layer_id,
                                                     double playhead_ms,
                                                     double duration_ms,
                                                     mv_editor_timeline_view_state state,
                                                     Vector2 mouse,
                                                     float wheel,
                                                     bool shift_down,
                                                     bool ctrl_down);
