#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "raylib.h"

enum class editor_timeline_note_type;

enum class editor_timeline_note_resize_handle {
    none,
    lane_left,
    lane_right,
    start_tick,
    end_tick
};

struct editor_timeline_note_visual_geometry {
    Rectangle head_rect = {};
    Rectangle body_rect = {};
    Rectangle visual_body_rect = {};
    Rectangle tail_rect = {};
    bool has_body = false;
};

struct editor_timeline_note_selection_geometry {
    std::array<Rectangle, 3> point_rects = {};
    size_t point_rect_count = 0;
    std::array<Rectangle, 3> range_rects = {};
    size_t range_rect_count = 0;
};

struct editor_timeline_note_resize_geometry {
    Rectangle left_lane_rect = {};
    Rectangle right_lane_rect = {};
    std::optional<Rectangle> start_tick_rect;
    std::optional<Rectangle> end_tick_rect;
};

struct editor_timeline_note_geometry {
    editor_timeline_note_visual_geometry visual;
    editor_timeline_note_selection_geometry selection;
    editor_timeline_note_resize_geometry resize;
};

editor_timeline_note_geometry make_editor_timeline_note_geometry(editor_timeline_note_type type,
                                                                 Rectangle head_rect,
                                                                 std::optional<Rectangle> body_rect,
                                                                 Rectangle tail_rect,
                                                                 Rectangle left_resize_rect,
                                                                 Rectangle right_resize_rect,
                                                                 std::optional<Rectangle> start_resize_rect,
                                                                 std::optional<Rectangle> end_resize_rect);

bool editor_timeline_note_contains_point(const editor_timeline_note_geometry& geometry, Vector2 point);
bool editor_timeline_note_intersects_rect(const editor_timeline_note_geometry& geometry, Rectangle rect);
editor_timeline_note_resize_handle editor_timeline_note_resize_handle_at(
    const editor_timeline_note_geometry& geometry,
    Vector2 point);
