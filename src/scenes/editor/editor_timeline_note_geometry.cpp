#include "editor_timeline_note_geometry.h"

#include <algorithm>

#include "editor_timeline_types.h"
#include "ui_hit.h"

namespace {

Rectangle visual_body_rect_for_note(editor_timeline_note_type type, Rectangle body_rect) {
    if (type != editor_timeline_note_type::decorative_hold) {
        return body_rect;
    }

    const float inset_x = std::max(3.0f, body_rect.width * 0.18f);
    return {
        body_rect.x + inset_x,
        body_rect.y,
        std::max(2.0f, body_rect.width - inset_x * 2.0f),
        body_rect.height
    };
}

void add_rect(std::array<Rectangle, 3>& rects, size_t& count, Rectangle rect) {
    if (count < rects.size()) {
        rects[count++] = rect;
    }
}

bool rects_overlap(Rectangle left, Rectangle right) {
    return left.x <= right.x + right.width && right.x <= left.x + left.width &&
           left.y <= right.y + right.height && right.y <= left.y + left.height;
}

}  // namespace

editor_timeline_note_geometry make_editor_timeline_note_geometry(editor_timeline_note_type type,
                                                                 Rectangle head_rect,
                                                                 std::optional<Rectangle> body_rect,
                                                                 Rectangle tail_rect,
                                                                 Rectangle left_resize_rect,
                                                                 Rectangle right_resize_rect,
                                                                 std::optional<Rectangle> start_resize_rect,
                                                                 std::optional<Rectangle> end_resize_rect) {
    editor_timeline_note_geometry geometry;
    geometry.visual.head_rect = head_rect;
    geometry.visual.tail_rect = tail_rect;
    geometry.visual.has_body = body_rect.has_value();
    if (body_rect.has_value()) {
        geometry.visual.body_rect = *body_rect;
        geometry.visual.visual_body_rect = visual_body_rect_for_note(type, *body_rect);
    }

    if (type == editor_timeline_note_type::decorative_hold) {
        if (geometry.visual.has_body) {
            add_rect(geometry.selection.point_rects,
                     geometry.selection.point_rect_count,
                     geometry.visual.visual_body_rect);
            add_rect(geometry.selection.range_rects,
                     geometry.selection.range_rect_count,
                     geometry.visual.visual_body_rect);
        }
    } else {
        add_rect(geometry.selection.point_rects, geometry.selection.point_rect_count, head_rect);
        add_rect(geometry.selection.range_rects, geometry.selection.range_rect_count, head_rect);
        if (geometry.visual.has_body) {
            add_rect(geometry.selection.point_rects, geometry.selection.point_rect_count, geometry.visual.body_rect);
            add_rect(geometry.selection.point_rects, geometry.selection.point_rect_count, tail_rect);
            add_rect(geometry.selection.range_rects, geometry.selection.range_rect_count, geometry.visual.body_rect);
            add_rect(geometry.selection.range_rects, geometry.selection.range_rect_count, tail_rect);
        }
    }

    geometry.resize.left_lane_rect = left_resize_rect;
    geometry.resize.right_lane_rect = right_resize_rect;
    geometry.resize.start_tick_rect = start_resize_rect;
    geometry.resize.end_tick_rect = end_resize_rect;
    return geometry;
}

bool editor_timeline_note_contains_point(const editor_timeline_note_geometry& geometry, Vector2 point) {
    for (size_t i = 0; i < geometry.selection.point_rect_count; ++i) {
        if (ui::contains_point(geometry.selection.point_rects[i], point)) {
            return true;
        }
    }
    return false;
}

bool editor_timeline_note_intersects_rect(const editor_timeline_note_geometry& geometry, Rectangle rect) {
    for (size_t i = 0; i < geometry.selection.range_rect_count; ++i) {
        if (rects_overlap(rect, geometry.selection.range_rects[i])) {
            return true;
        }
    }
    return false;
}

editor_timeline_note_resize_handle editor_timeline_note_resize_handle_at(
    const editor_timeline_note_geometry& geometry,
    Vector2 point) {
    if (geometry.resize.start_tick_rect.has_value() &&
        ui::contains_point(*geometry.resize.start_tick_rect, point)) {
        return editor_timeline_note_resize_handle::start_tick;
    }
    if (geometry.resize.end_tick_rect.has_value() &&
        ui::contains_point(*geometry.resize.end_tick_rect, point)) {
        return editor_timeline_note_resize_handle::end_tick;
    }
    if (ui::contains_point(geometry.resize.left_lane_rect, point)) {
        return editor_timeline_note_resize_handle::lane_left;
    }
    if (ui::contains_point(geometry.resize.right_lane_rect, point)) {
        return editor_timeline_note_resize_handle::lane_right;
    }
    return editor_timeline_note_resize_handle::none;
}
