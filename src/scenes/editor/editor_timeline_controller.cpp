#include "editor_timeline_controller.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace {

int snap_tick(const editor_timeline_context& context, int raw_tick) {
    if (context.state == nullptr) {
        return std::max(0, raw_tick);
    }
    return std::max(0, context.state->snap_tick(std::max(0, raw_tick), context.snap_division));
}

int seek_tick(int raw_tick) {
    return std::max(0, raw_tick);
}

std::optional<int> lane_at_position(const editor_timeline_context& context, Vector2 point) {
    const Rectangle content = context.metrics.content_rect();
    if (!CheckCollisionPointRec(point, content) || context.state == nullptr) {
        return std::nullopt;
    }

    for (int lane = 0; lane < context.state->data().meta.key_count; ++lane) {
        if (CheckCollisionPointRec(point, context.metrics.lane_rect(lane))) {
            return lane;
        }
    }

    return std::nullopt;
}

int lane_at_x_clamped(const editor_timeline_context& context, float x) {
    if (context.state == nullptr) {
        return 0;
    }

    const int key_count = std::max(1, context.state->data().meta.key_count);
    const Rectangle content = context.metrics.content_rect();
    const float lane_step = context.metrics.lane_width() + context.metrics.lane_gap;
    const int lane = static_cast<int>(std::floor((x - content.x) / std::max(1.0f, lane_step)));
    return std::clamp(lane, 0, key_count - 1);
}

editor_timeline_note make_timeline_note(const note_data& note) {
    return {
        note.type == note_type::hold ? editor_timeline_note_type::hold :
        note.type == note_type::release ? editor_timeline_note_type::release :
        note.type == note_type::stay ? editor_timeline_note_type::stay :
        editor_timeline_note_type::tap,
        note.tick,
        note.lane,
        note.end_tick,
        note.is_ray,
        note_lane_width(note),
    };
}

bool note_intersects_tick_range(const note_data& note, int min_tick, int max_tick) {
    const int start_tick = note.tick;
    const int end_tick = note.type == note_type::hold ? std::max(note.tick, note.end_tick) : note.tick;
    return end_tick >= min_tick && start_tick <= max_tick;
}

std::optional<size_t> note_at_position(const editor_timeline_context& context, Vector2 point) {
    const Rectangle content = context.metrics.content_rect();
    if (!CheckCollisionPointRec(point, content) || context.state == nullptr) {
        return std::nullopt;
    }

    const int hit_tick = context.metrics.y_to_tick(point.y);
    const int tick_margin = static_cast<int>(std::ceil(context.metrics.note_head_height *
                                                       context.metrics.ticks_per_pixel * 2.0f));
    for (size_t i = context.state->data().notes.size(); i > 0; --i) {
        const size_t index = i - 1;
        const note_data& note = context.state->data().notes[index];
        if (note.lane < 0 || note.lane >= context.state->data().meta.key_count) {
            continue;
        }
        if (!note_intersects_tick_range(note, hit_tick - tick_margin, hit_tick + tick_margin)) {
            continue;
        }

        const editor_timeline_note timeline_note = make_timeline_note(note);
        const editor_timeline_note_draw_info info = context.metrics.note_rects(timeline_note);
        if (CheckCollisionPointRec(point, info.head_rect) ||
            (info.has_body && (CheckCollisionPointRec(point, info.body_rect) || CheckCollisionPointRec(point, info.tail_rect)))) {
            return index;
        }
    }

    return std::nullopt;
}

std::vector<size_t> sorted_unique_indices(std::vector<size_t> indices) {
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

bool contains_index(const std::vector<size_t>& indices, size_t index) {
    return std::find(indices.begin(), indices.end(), index) != indices.end();
}

std::optional<size_t> active_note_index(const editor_timeline_result& result) {
    if (!result.selected_note_indices.empty()) {
        return result.selected_note_indices.back();
    }
    return std::nullopt;
}

Rectangle rectangle_from_points(Vector2 start, Vector2 end) {
    const float x = std::min(start.x, end.x);
    const float y = std::min(start.y, end.y);
    return {x, y, std::fabs(end.x - start.x), std::fabs(end.y - start.y)};
}

bool rects_overlap(Rectangle left, Rectangle right) {
    return left.x <= right.x + right.width && right.x <= left.x + left.width &&
           left.y <= right.y + right.height && right.y <= left.y + left.height;
}

std::vector<size_t> notes_in_rectangle(const editor_timeline_context& context, Rectangle rect) {
    std::vector<size_t> indices;
    if (context.state == nullptr) {
        return indices;
    }

    const int min_tick = context.metrics.y_to_tick(rect.y + rect.height);
    const int max_tick = context.metrics.y_to_tick(rect.y);
    const int tick_margin = static_cast<int>(std::ceil(context.metrics.note_head_height *
                                                       context.metrics.ticks_per_pixel * 2.0f));
    for (size_t index = 0; index < context.state->data().notes.size(); ++index) {
        const note_data& note = context.state->data().notes[index];
        if (note.lane < 0 || note.lane >= context.state->data().meta.key_count) {
            continue;
        }
        if (!note_intersects_tick_range(note, min_tick - tick_margin, max_tick + tick_margin)) {
            continue;
        }

        const editor_timeline_note_draw_info info = context.metrics.note_rects(make_timeline_note(note));
        if (rects_overlap(rect, info.head_rect) ||
            (info.has_body && (rects_overlap(rect, info.body_rect) || rects_overlap(rect, info.tail_rect)))) {
            indices.push_back(index);
        }
    }
    return indices;
}

std::optional<editor_timeline_drag_mode> resize_handle_at_position(const editor_timeline_context& context,
                                                                   size_t note_index,
                                                                   Vector2 point) {
    if (context.state == nullptr || note_index >= context.state->data().notes.size()) {
        return std::nullopt;
    }

    const note_data& note = context.state->data().notes[note_index];
    const editor_timeline_note_draw_info info = context.metrics.note_rects(make_timeline_note(note));
    if (note.type == note_type::hold) {
        if (CheckCollisionPointRec(point, info.start_resize_rect)) {
            return editor_timeline_drag_mode::resize_start;
        }
        if (CheckCollisionPointRec(point, info.end_resize_rect)) {
            return editor_timeline_drag_mode::resize_end;
        }
    }
    if (CheckCollisionPointRec(point, info.left_resize_rect)) {
        return editor_timeline_drag_mode::resize_left;
    }
    if (CheckCollisionPointRec(point, info.right_resize_rect)) {
        return editor_timeline_drag_mode::resize_right;
    }
    return std::nullopt;
}

int minimum_hold_tick_gap(const editor_timeline_context& context) {
    if (context.state == nullptr) {
        return 1;
    }
    return std::max(1, context.state->data().meta.resolution / std::max(1, context.snap_division));
}

std::optional<note_data> dragged_note(const editor_timeline_context& context,
                                      const editor_timeline_note_drag_state& drag_state) {
    if (!drag_state.active || drag_state.mode != editor_timeline_drag_mode::create) {
        return std::nullopt;
    }

    note_data note;
    note.lane = std::min(drag_state.lane, drag_state.current_lane);
    note.lane_width = std::abs(drag_state.current_lane - drag_state.lane) + 1;
    note.tick = std::min(drag_state.start_tick, drag_state.current_tick);
    note.end_tick = std::max(drag_state.start_tick, drag_state.current_tick);
    note.type = (note.end_tick - note.tick) >= minimum_hold_tick_gap(context)
        ? note_type::hold
        : note_type::tap;
    if (note.type == note_type::tap) {
        note.end_tick = note.tick;
    }
    if (context.palette.type != note_type::hold) {
        note.type = context.palette.type;
        note.end_tick = note.tick;
    }
    note.is_ray = context.palette.is_ray;

    return note;
}

std::optional<note_data> resized_note(const editor_timeline_context& context,
                                      const editor_timeline_note_drag_state& drag_state) {
    if (!drag_state.active || !drag_state.note_index.has_value() || context.state == nullptr ||
        *drag_state.note_index >= context.state->data().notes.size()) {
        return std::nullopt;
    }

    note_data note = drag_state.original_note;
    if (drag_state.mode == editor_timeline_drag_mode::resize_left) {
        const int last_lane = note_last_lane(drag_state.original_note);
        note.lane = std::clamp(drag_state.current_lane, 0, last_lane);
        note.lane_width = last_lane - note.lane + 1;
    } else if (drag_state.mode == editor_timeline_drag_mode::resize_right) {
        const int key_count = std::max(1, context.state->data().meta.key_count);
        const int last_lane = std::clamp(drag_state.current_lane, note.lane, key_count - 1);
        note.lane_width = last_lane - note.lane + 1;
    } else if (drag_state.mode == editor_timeline_drag_mode::resize_start) {
        if (note.type != note_type::hold) {
            return std::nullopt;
        }
        note.tick = std::clamp(drag_state.current_tick, 0, note.end_tick - minimum_hold_tick_gap(context));
    } else if (drag_state.mode == editor_timeline_drag_mode::resize_end) {
        if (note.type != note_type::hold) {
            return std::nullopt;
        }
        note.end_tick = std::max(note.tick + minimum_hold_tick_gap(context), drag_state.current_tick);
    } else {
        return std::nullopt;
    }
    return note;
}

}  // namespace

editor_timeline_result editor_timeline_controller::update(editor_timing_panel_state& timing_panel,
                                                          const editor_timeline_context& context) {
    editor_timeline_result result;
    result.selected_note_indices = sorted_unique_indices(context.selected_note_indices);
    result.drag_state = context.drag_state;

    if (timing_panel.bar_pick_mode) {
        if (context.left_pressed && context.timeline_hovered && timing_panel.selected_event_index.has_value() &&
            context.state != nullptr && context.meter_map != nullptr &&
            *timing_panel.selected_event_index < context.state->data().timing_events.size()) {
            const int tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
            const editor_meter_map::bar_beat_position position = context.meter_map->bar_beat_at_tick(tick);
            const timing_event& event = context.state->data().timing_events[*timing_panel.selected_event_index];
            const std::string value = std::to_string(position.measure) + ":" + std::to_string(position.beat);
            if (event.type == timing_event_type::bpm) {
                timing_panel.inputs.bpm_bar.value = value;
            } else {
                timing_panel.inputs.meter_bar.value = value;
            }
            timing_panel.bar_pick_mode = false;
            timing_panel.active_input_field = editor_timing_input_field::none;
            result.request_apply_selected_timing = true;
        } else if (context.left_pressed && context.timeline_hovered && timing_panel.selected_scroll_event_index.has_value() &&
            context.state != nullptr && context.meter_map != nullptr &&
            *timing_panel.selected_scroll_event_index < context.state->data().scroll_automation.size()) {
            const int tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
            const editor_meter_map::bar_beat_position position = context.meter_map->bar_beat_at_tick(tick);
            timing_panel.inputs.scroll_start_bar.value =
                std::to_string(position.measure) + ":" + std::to_string(position.beat);
            timing_panel.bar_pick_mode = false;
            timing_panel.active_input_field = editor_timing_input_field::none;
            result.request_apply_selected_scroll = true;
        } else if (context.right_pressed || context.escape_pressed) {
            timing_panel.bar_pick_mode = false;
            timing_panel.active_input_field = editor_timing_input_field::none;
        }
        return result;
    }

    if (context.shift_down && context.timeline_hovered && (context.left_pressed || context.left_down)) {
        result.request_seek = true;
        result.seek_tick = seek_tick(context.metrics.y_to_tick(context.mouse.y));
        result.drag_state.active = false;
        return result;
    }

    if (context.left_pressed) {
        if (!context.timeline_hovered) {
            result.selected_note_indices.clear();
            result.drag_state.active = false;
            return result;
        }

        if (const std::optional<size_t> selected_index = active_note_index(result); selected_index.has_value()) {
            const std::optional<editor_timeline_drag_mode> handle =
                resize_handle_at_position(context, *selected_index, context.mouse);
            if (handle.has_value()) {
                result.drag_state.active = true;
                result.drag_state.mode = *handle;
                result.drag_state.note_index = selected_index;
                result.drag_state.original_note = context.state->data().notes[*selected_index];
                result.drag_state.lane = result.drag_state.original_note.lane;
                result.drag_state.current_lane = lane_at_x_clamped(context, context.mouse.x);
                result.drag_state.start_tick = result.drag_state.original_note.tick;
                result.drag_state.current_tick =
                    *handle == editor_timeline_drag_mode::resize_end
                        ? result.drag_state.original_note.end_tick
                        : result.drag_state.start_tick;
                return result;
            }
        }

        if (const std::optional<size_t> note_index = note_at_position(context, context.mouse); note_index.has_value()) {
            if (context.ctrl_down) {
                if (contains_index(result.selected_note_indices, *note_index)) {
                    result.selected_note_indices.erase(
                        std::remove(result.selected_note_indices.begin(), result.selected_note_indices.end(), *note_index),
                        result.selected_note_indices.end());
                } else {
                    result.selected_note_indices.push_back(*note_index);
                    result.selected_note_indices = sorted_unique_indices(result.selected_note_indices);
                }
                result.drag_state.active = false;
                return result;
            }

            if (!contains_index(result.selected_note_indices, *note_index)) {
                result.selected_note_indices = {*note_index};
            }
            result.drag_state.active = true;
            result.drag_state.mode = editor_timeline_drag_mode::move_notes;
            result.drag_state.note_index = note_index;
            result.drag_state.note_indices = result.selected_note_indices;
            result.drag_state.original_note = context.state->data().notes[*note_index];
            result.drag_state.original_notes.clear();
            result.drag_state.original_notes.reserve(result.drag_state.note_indices.size());
            for (const size_t selected : result.drag_state.note_indices) {
                result.drag_state.original_notes.push_back(context.state->data().notes[selected]);
            }
            result.drag_state.lane = lane_at_x_clamped(context, context.mouse.x);
            result.drag_state.current_lane = result.drag_state.lane;
            result.drag_state.start_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
            result.drag_state.current_tick = result.drag_state.start_tick;
            result.drag_state.start_mouse = context.mouse;
            result.drag_state.current_mouse = context.mouse;
            return result;
        }

        result.drag_state.active = true;
        result.drag_state.mode = editor_timeline_drag_mode::range_select;
        result.drag_state.note_index.reset();
        result.drag_state.note_indices.clear();
        result.drag_state.original_notes.clear();
        result.drag_state.start_mouse = context.mouse;
        result.drag_state.current_mouse = context.mouse;
        result.drag_state.start_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
        result.drag_state.current_tick = result.drag_state.start_tick;
        return result;
    }

    if (!context.timeline_hovered) {
        if (context.left_released || context.right_released) {
            result.drag_state.active = false;
        }
        return result;
    }

    if (context.right_pressed) {
        if (const std::optional<size_t> note_index = note_at_position(context, context.mouse); note_index.has_value()) {
            result.note_to_delete_index = note_index;
            result.selected_note_indices.clear();
            result.drag_state.active = false;
            return result;
        }

        if (const std::optional<int> lane = lane_at_position(context, context.mouse); lane.has_value()) {
            result.drag_state.active = true;
            result.drag_state.mode = editor_timeline_drag_mode::create;
            result.drag_state.note_index.reset();
            result.drag_state.lane = *lane;
            result.drag_state.current_lane = *lane;
            result.drag_state.start_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
            result.drag_state.current_tick = result.drag_state.start_tick;
            result.selected_note_indices.clear();
        }
    }

    if (result.drag_state.active &&
        (context.left_down || context.left_released || context.right_down || context.right_released)) {
        if (result.drag_state.mode == editor_timeline_drag_mode::create) {
            result.drag_state.current_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
            if (const std::optional<int> lane = lane_at_position(context, context.mouse); lane.has_value()) {
                result.drag_state.current_lane = *lane;
            }
        } else if (result.drag_state.mode == editor_timeline_drag_mode::resize_start ||
                   result.drag_state.mode == editor_timeline_drag_mode::resize_end) {
            result.drag_state.current_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
        } else if (result.drag_state.mode == editor_timeline_drag_mode::move_notes) {
            result.drag_state.current_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
            result.drag_state.current_lane = lane_at_x_clamped(context, context.mouse.x);
            result.drag_state.current_mouse = context.mouse;
        } else if (result.drag_state.mode == editor_timeline_drag_mode::range_select) {
            result.drag_state.current_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
            result.drag_state.current_mouse = context.mouse;
        } else {
            result.drag_state.current_lane = lane_at_x_clamped(context, context.mouse.x);
        }
    }

    if (!result.drag_state.active ||
        !(context.left_released || context.right_released) ||
        context.state == nullptr) {
        return result;
    }

    if (result.drag_state.mode == editor_timeline_drag_mode::range_select) {
        const Rectangle selection_rect = rectangle_from_points(result.drag_state.start_mouse, result.drag_state.current_mouse);
        result.selected_note_indices = sorted_unique_indices(notes_in_rectangle(context, selection_rect));
        result.drag_state.active = false;
        return result;
    }

    if (result.drag_state.mode == editor_timeline_drag_mode::move_notes) {
        const int tick_delta = result.drag_state.current_tick - result.drag_state.start_tick;
        const int lane_delta = result.drag_state.current_lane - result.drag_state.lane;
        result.drag_state.active = false;
        if ((tick_delta == 0 && lane_delta == 0) || result.drag_state.note_indices.empty()) {
            return result;
        }

        result.notes_to_modify.reserve(result.drag_state.note_indices.size());
        for (size_t i = 0; i < result.drag_state.note_indices.size(); ++i) {
            note_data moved = result.drag_state.original_notes[i];
            moved.tick = std::max(0, moved.tick + tick_delta);
            if (moved.type == note_type::hold) {
                moved.end_tick = std::max(moved.tick + 1, moved.end_tick + tick_delta);
            } else {
                moved.end_tick = moved.tick;
            }
            moved.lane += lane_delta;
            result.notes_to_modify.push_back({result.drag_state.note_indices[i], moved});
        }
        result.selected_note_indices = result.drag_state.note_indices;
        return result;
    }

    if (result.drag_state.mode != editor_timeline_drag_mode::create) {
        std::optional<note_data> note = resized_note(context, result.drag_state);
        const std::optional<size_t> note_index = result.drag_state.note_index;
        result.drag_state.active = false;
        if (!note.has_value() || !note_index.has_value() ||
            context.state->has_note_overlap(*note, note_index)) {
            return result;
        }
        result.note_to_modify_index = note_index;
        result.note_to_modify = note;
        return result;
    }

    result.drag_state.current_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
    std::optional<note_data> note = dragged_note(context, result.drag_state);
    if (note.has_value() && note->type == note_type::hold &&
        (note->end_tick - note->tick) < minimum_hold_tick_gap(context)) {
        note->type = note_type::tap;
        note->end_tick = note->tick;
    }
    result.drag_state.active = false;

    if (!note.has_value() || context.state->has_note_overlap(*note)) {
        return result;
    }

    result.note_to_add = note;
    return result;
}
