#include "editor_timeline_controller.h"

#include <algorithm>

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

        const editor_timeline_note timeline_note = {
            note.type == note_type::hold ? editor_timeline_note_type::hold : editor_timeline_note_type::tap,
            note.tick,
            note.lane,
            note.end_tick,
        };
        const editor_timeline_note_draw_info info = context.metrics.note_rects(timeline_note);
        if (CheckCollisionPointRec(point, info.head_rect) ||
            (info.has_body && (CheckCollisionPointRec(point, info.body_rect) || CheckCollisionPointRec(point, info.tail_rect)))) {
            return index;
        }
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
    if (!drag_state.active) {
        return std::nullopt;
    }

    note_data note;
    note.lane = drag_state.lane;
    note.tick = std::min(drag_state.start_tick, drag_state.current_tick);
    note.end_tick = std::max(drag_state.start_tick, drag_state.current_tick);
    note.type = (note.end_tick - note.tick) >= minimum_hold_tick_gap(context)
        ? note_type::hold
        : note_type::tap;
    if (note.type == note_type::tap) {
        note.end_tick = note.tick;
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
        if (const std::optional<int> lane = lane_at_position(context, context.mouse); lane.has_value()) {
            result.drag_state.active = true;
            result.drag_state.lane = *lane;
            result.drag_state.start_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
            result.drag_state.current_tick = result.drag_state.start_tick;
        }
    }

    if (result.drag_state.active && (context.left_down || context.left_released)) {
        result.drag_state.current_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
    }

    if (!result.drag_state.active || !context.left_released || context.state == nullptr) {
        return result;
    }

    result.drag_state.current_tick = snap_tick(context, context.metrics.y_to_tick(context.mouse.y));
    std::optional<note_data> note = dragged_note(context, result.drag_state);
    if (note.has_value() && (note->end_tick - note->tick) < minimum_hold_tick_gap(context)) {
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
