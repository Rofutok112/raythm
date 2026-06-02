#include "editor/controller/editor_timeline_screen_controller.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "editor/daw/editor_daw_view.h"
#include "editor/service/editor_note_placement_rules.h"
#include "editor/view/editor_layout.h"

namespace {

Rectangle snap_dropdown_menu_rect() {
    return editor::layout::snap_dropdown_menu_rect(
        static_cast<int>(editor_timeline_viewport::snap_labels().size()));
}

editor_timeline_viewport_model viewport_model(const editor_timeline_screen_controller::context& context) {
    return {&context.state, context.transport.audio_length_tick, context.viewport};
}

editor_timeline_note make_timeline_note(const note_data& note) {
    return {
        note.type == note_type::hold ? editor_timeline_note_type::hold :
        note.type == note_type::decorative_hold ? editor_timeline_note_type::decorative_hold :
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

std::optional<note_data> dragged_note(const editor_timeline_screen_controller::context& context) {
    const editor_timeline_note_drag_state& drag = context.timeline_drag;
    if (!drag.active) {
        return std::nullopt;
    }

    if (drag.mode == editor_timeline_drag_mode::resize_left ||
        drag.mode == editor_timeline_drag_mode::resize_right ||
        drag.mode == editor_timeline_drag_mode::resize_start ||
        drag.mode == editor_timeline_drag_mode::resize_end) {
        if (!drag.note_index.has_value() || *drag.note_index >= context.state.data().notes.size()) {
            return std::nullopt;
        }

        note_data note = drag.original_note;
        if (drag.mode == editor_timeline_drag_mode::resize_left) {
            const int last_lane = note_last_lane(drag.original_note);
            note.lane = std::clamp(drag.current_lane, 0, last_lane);
            note.lane_width = last_lane - note.lane + 1;
        } else if (drag.mode == editor_timeline_drag_mode::resize_right) {
            const int last_lane = std::clamp(drag.current_lane, note.lane, context.state.data().meta.key_count - 1);
            note.lane_width = last_lane - note.lane + 1;
        } else if (drag.mode == editor_timeline_drag_mode::resize_start && note_has_duration(note)) {
            const int min_gap = editor_timeline_viewport::snap_interval(viewport_model(context));
            note.tick = std::clamp(drag.current_tick, 0, note.end_tick - min_gap);
        } else if (note_has_duration(note)) {
            const int min_gap = editor_timeline_viewport::snap_interval(viewport_model(context));
            note.end_tick = std::max(note.tick + min_gap, drag.current_tick);
        }
        return note;
    }

    if (drag.mode != editor_timeline_drag_mode::create) {
        return std::nullopt;
    }

    note_data note;
    note.lane = std::min(drag.lane, drag.current_lane);
    note.lane_width = std::abs(drag.current_lane - drag.lane) + 1;
    note.tick = std::min(drag.start_tick, drag.current_tick);
    note.end_tick = std::max(drag.start_tick, drag.current_tick);
    note.type = (note.end_tick - note.tick) >= editor_timeline_viewport::snap_interval(viewport_model(context))
        ? note_type::hold
        : note_type::tap;
    if (note.type == note_type::tap) {
        note.end_tick = note.tick;
    }
    if (note_type_has_duration(context.note_palette.type)) {
        note.type = context.note_palette.type;
    } else {
        note.type = context.note_palette.type;
        note.end_tick = note.tick;
    }
    note.is_ray = context.note_palette.is_ray;

    return note;
}

std::vector<note_data> dragged_notes(const editor_timeline_screen_controller::context& context) {
    const editor_timeline_note_drag_state& drag = context.timeline_drag;
    if (!drag.active) {
        return {};
    }

    if (drag.mode != editor_timeline_drag_mode::move_notes) {
        if (const std::optional<note_data> note = dragged_note(context); note.has_value()) {
            return {*note};
        }
        return {};
    }

    const int tick_delta = drag.current_tick - drag.start_tick;
    const int lane_delta = drag.current_lane - drag.lane;
    std::vector<note_data> notes;
    notes.reserve(drag.original_notes.size());
    for (note_data note : drag.original_notes) {
        note.tick = std::max(0, note.tick + tick_delta);
        if (note_has_duration(note)) {
            note.end_tick = std::max(note.tick + 1, note.end_tick + tick_delta);
        } else {
            note.end_tick = note.tick;
        }
        note.lane += lane_delta;
        notes.push_back(note);
    }
    return notes;
}

}  // namespace

namespace editor_timeline_screen_controller {

editor_right_panel_view_result draw(const context& context) {
    const std::vector<note_data> preview_notes = dragged_notes(context);
    std::vector<size_t> preview_ignore_indices;
    if (context.timeline_drag.active && context.timeline_drag.mode == editor_timeline_drag_mode::move_notes) {
        preview_ignore_indices = context.timeline_drag.note_indices;
    } else if (context.timeline_drag.active &&
               (context.timeline_drag.mode == editor_timeline_drag_mode::resize_left ||
                context.timeline_drag.mode == editor_timeline_drag_mode::resize_right ||
                context.timeline_drag.mode == editor_timeline_drag_mode::resize_start ||
                context.timeline_drag.mode == editor_timeline_drag_mode::resize_end) &&
               context.timeline_drag.note_index.has_value()) {
        preview_ignore_indices.push_back(*context.timeline_drag.note_index);
    }
    const bool preview_has_overlap = !preview_notes.empty() &&
        (context.state.has_note_overlap(preview_notes, preview_ignore_indices) ||
         editor::note_placement_rules::has_stay_stack(context.state, preview_notes, preview_ignore_indices));

    std::optional<Rectangle> selection_rect;
    if (context.timeline_drag.active && context.timeline_drag.mode == editor_timeline_drag_mode::range_select) {
        selection_rect = {
            std::min(context.timeline_drag.start_mouse.x, context.timeline_drag.current_mouse.x),
            std::min(context.timeline_drag.start_mouse.y, context.timeline_drag.current_mouse.y),
            std::fabs(context.timeline_drag.current_mouse.x - context.timeline_drag.start_mouse.x),
            std::fabs(context.timeline_drag.current_mouse.y - context.timeline_drag.start_mouse.y)
        };
    }

    return editor::daw::draw_timeline({
        context.state,
        context.meter_map,
        &context.waveform_summary,
        context.waveform_visible,
        context.waveform_offset_ms,
        context.transport.audio_loaded,
        context.transport.playback_tick,
        context.selected_note_indices,
        context.timing_panel.selected_scroll_event_index,
        preview_notes,
        preview_ignore_indices,
        preview_has_overlap,
        selection_rect,
        viewport_model(context),
    }, snap_dropdown_menu_rect(), context.snap_dropdown_open);
}

int mouse_cursor(const cursor_context& context) {
    if (context.blocking_modal) {
        return MOUSE_CURSOR_DEFAULT;
    }

    if (context.timeline_drag.active) {
        if (context.timeline_drag.mode == editor_timeline_drag_mode::resize_left ||
            context.timeline_drag.mode == editor_timeline_drag_mode::resize_right) {
            return MOUSE_CURSOR_RESIZE_EW;
        }
        if (context.timeline_drag.mode == editor_timeline_drag_mode::resize_start ||
            context.timeline_drag.mode == editor_timeline_drag_mode::resize_end) {
            return MOUSE_CURSOR_RESIZE_NS;
        }
    }

    if (!CheckCollisionPointRec(context.mouse, context.metrics.content_rect())) {
        return MOUSE_CURSOR_DEFAULT;
    }

    const std::optional<size_t> active_index = context.selected_note_indices.empty()
        ? std::nullopt
        : std::optional<size_t>(context.selected_note_indices.back());
    if (!active_index.has_value() || *active_index >= context.state.data().notes.size()) {
        return MOUSE_CURSOR_DEFAULT;
    }

    const note_data& note = context.state.data().notes[*active_index];
    const editor_timeline_note_draw_info info = context.metrics.note_rects(make_timeline_note(note));
    if (note_has_duration(note) &&
        (CheckCollisionPointRec(context.mouse, info.start_resize_rect) ||
         CheckCollisionPointRec(context.mouse, info.end_resize_rect))) {
        return MOUSE_CURSOR_RESIZE_NS;
    }
    if (CheckCollisionPointRec(context.mouse, info.left_resize_rect) ||
        CheckCollisionPointRec(context.mouse, info.right_resize_rect)) {
        return MOUSE_CURSOR_RESIZE_EW;
    }
    return MOUSE_CURSOR_DEFAULT;
}

}  // namespace editor_timeline_screen_controller
