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

std::optional<size_t> note_at_position(const editor_timeline_context& context, Vector2 point) {
    const Rectangle content = context.metrics.content_rect();
    if (!CheckCollisionPointRec(point, content) || context.state == nullptr) {
        return std::nullopt;
    }

    for (size_t i = context.state->data().notes.size(); i > 0; --i) {
        const size_t index = i - 1;
        const note_data& note = context.state->data().notes[index];
        if (note.lane < 0 || note.lane >= context.state->data().meta.key_count) {
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

std::optional<editor_timeline_drag_mode> resize_handle_at_position(const editor_timeline_context& context,
                                                                   size_t note_index,
                                                                   Vector2 point) {
    if (context.state == nullptr || note_index >= context.state->data().notes.size()) {
        return std::nullopt;
    }

    const note_data& note = context.state->data().notes[note_index];
    const editor_timeline_note_draw_info info = context.metrics.note_rects(make_timeline_note(note));
    if (note.type == note_type::hold && CheckCollisionPointRec(point, info.end_resize_rect)) {
        return editor_timeline_drag_mode::resize_end;
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
    result.selected_note_index = context.selected_note_index;
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
        } else if (context.right_pressed || context.escape_pressed) {
            timing_panel.bar_pick_mode = false;
            timing_panel.active_input_field = editor_timing_input_field::none;
        }
        return result;
    }

    if (context.right_pressed) {
        result.note_to_delete_index = context.timeline_hovered ? note_at_position(context, context.mouse) : std::nullopt;
        if (result.note_to_delete_index.has_value() &&
            result.selected_note_index.has_value() &&
            *result.selected_note_index == *result.note_to_delete_index) {
            result.selected_note_index.reset();
        }
        return result;
    }

    if (!context.timeline_hovered) {
        if (context.left_released) {
            result.drag_state.active = false;
        }
        return result;
    }

    if (context.alt_down && context.left_pressed) {
        result.request_seek = true;
        result.seek_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
        result.scroll_seek_if_paused = true;
        result.drag_state.active = false;
        return result;
    }

    if (context.left_pressed) {
        if (result.selected_note_index.has_value()) {
            const std::optional<editor_timeline_drag_mode> handle =
                resize_handle_at_position(context, *result.selected_note_index, context.mouse);
            if (handle.has_value()) {
                result.drag_state.active = true;
                result.drag_state.mode = *handle;
                result.drag_state.note_index = result.selected_note_index;
                result.drag_state.original_note = context.state->data().notes[*result.selected_note_index];
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
            result.selected_note_index = note_index;
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
        }
    }

    if (result.drag_state.active && (context.left_down || context.left_released)) {
        if (result.drag_state.mode == editor_timeline_drag_mode::create) {
            result.drag_state.current_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
            if (const std::optional<int> lane = lane_at_position(context, context.mouse); lane.has_value()) {
                result.drag_state.current_lane = *lane;
            }
        } else if (result.drag_state.mode == editor_timeline_drag_mode::resize_end) {
            result.drag_state.current_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
        } else {
            result.drag_state.current_lane = lane_at_x_clamped(context, context.mouse.x);
        }
    }

    if (!result.drag_state.active || !context.left_released || context.state == nullptr) {
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
        result.selected_note_index = note_index;
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
