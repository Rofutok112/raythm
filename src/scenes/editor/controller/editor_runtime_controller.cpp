#include "editor_runtime_controller.h"

#include <algorithm>

#include "editor/editor_timeline_controller.h"
#include "editor/service/editor_note_edit_service.h"
#include "editor/service/editor_transport_service.h"

namespace {

bool has_active_metadata_input(const metadata_panel_state& metadata_panel) {
    return metadata_panel.difficulty_input.active || metadata_panel.chart_author_input.active;
}

}  // namespace

editor_shortcut_result editor_runtime_controller::handle_shortcuts(const editor_shortcut_context& context) {
    editor_shortcut_result result;
    if (context.blocking_modal) {
        return result;
    }

    const bool metadata_input_active = has_active_metadata_input(context.metadata_panel);
    const bool timing_input_active = context.timing_panel.active_input_field != editor_timing_input_field::none;
    const bool editing_blocked = metadata_input_active ||
        timing_input_active ||
        context.mv_script_editor_active ||
        context.timing_panel.bar_pick_mode ||
        context.timeline_drag.active;

    if (!editing_blocked && context.space_pressed) {
        result.restore_scroll_tick = editor_transport_service::toggle_playback(
            context.transport,
            &context.state,
            context.space_playback_start_tick,
            context.hitsound_path,
            context.hitsounds);
    }

    if (context.ctrl_down && context.z_pressed && !context.mv_script_editor_active) {
        if (context.shift_down) {
            result.history_changed = context.state.redo();
        } else {
            result.history_changed = context.state.undo();
        }
        editor_scene_sync::sync_after_history_change(context.sync_context);
        editor_transport_service::sync(context.transport, &context.state, context.hitsound_path, context.hitsounds, true);
    }

    if (context.ctrl_down && context.y_pressed && !context.mv_script_editor_active) {
        result.history_changed = context.state.redo();
        editor_scene_sync::sync_after_history_change(context.sync_context);
        editor_transport_service::sync(context.transport, &context.state, context.hitsound_path, context.hitsounds, true);
    }

    if (!editing_blocked && context.ctrl_down && context.c_pressed) {
        context.clipboard_notes =
            editor_note_edit_service::notes_for_selection(context.state, context.selected_note_indices);
    }

    if (!editing_blocked && context.ctrl_down && (context.v_pressed || context.d_pressed)) {
        const editor_note_edit_result edit_result = editor_note_edit_service::paste_or_duplicate(
            context.state,
            context.selected_note_indices,
            context.clipboard_notes,
            context.d_pressed);
        if (edit_result.changed) {
            context.selected_note_indices = edit_result.selected_note_indices;
            result.history_changed = true;
        }
    }

    if (!editing_blocked && context.left_bracket_pressed) {
        context.transport.loop_start_tick = context.state.snap_tick(context.transport.playback_tick, 4);
        if (context.transport.loop_end_tick <= context.transport.loop_start_tick) {
            context.transport.loop_end_tick = context.transport.loop_start_tick + context.state.data().meta.resolution * 4;
        }
        context.transport.loop_enabled = true;
    }

    if (!editing_blocked && context.right_bracket_pressed) {
        context.transport.loop_end_tick = std::max(
            context.transport.loop_start_tick + 1,
            context.state.snap_tick(context.transport.playback_tick, 4));
        context.transport.loop_enabled = true;
    }

    if (!editing_blocked && context.l_pressed &&
        context.transport.loop_end_tick > context.transport.loop_start_tick) {
        context.transport.loop_enabled = !context.transport.loop_enabled;
    }

    if (!editing_blocked && context.delete_pressed) {
        const editor_note_edit_result edit_result =
            editor_note_edit_service::delete_selection(context.state, context.selected_note_indices);
        if (edit_result.changed) {
            context.selected_note_indices = edit_result.selected_note_indices;
            result.history_changed = true;
        }
    }

    context.selected_note_indices.erase(
        std::remove_if(context.selected_note_indices.begin(), context.selected_note_indices.end(), [&](size_t index) {
            return index >= context.state.data().notes.size();
        }),
        context.selected_note_indices.end());

    return result;
}

editor_runtime_timeline_result editor_runtime_controller::handle_timeline_interaction(
    const editor_runtime_timeline_context& context) {
    const editor_timeline_result timeline_result = editor_timeline_controller::update(context.timing_panel, {
        &context.state,
        &context.meter_map,
        context.metrics,
        context.mouse,
        context.timeline_hovered,
        context.left_pressed,
        context.left_down,
        context.left_released,
        context.right_pressed,
        context.escape_pressed,
        context.shift_down,
        context.snap_division,
        context.drag_state,
        context.palette,
        context.selected_note_indices,
        context.ctrl_down,
        context.right_down,
        context.right_released,
    });

    context.selected_note_indices = timeline_result.selected_note_indices;
    context.drag_state = timeline_result.drag_state;

    editor_runtime_timeline_result result;
    result.request_apply_selected_timing = timeline_result.request_apply_selected_timing;
    result.request_apply_selected_scroll = timeline_result.request_apply_selected_scroll;
    result.selected_scroll_event_index = timeline_result.selected_scroll_event_index;

    if (timeline_result.request_seek) {
        const bool was_playing = context.transport.audio_playing;
        if (was_playing) {
            editor_transport_service::pause_for_seek(
                context.transport,
                &context.state,
                context.space_playback_start_tick,
                context.hitsound_path,
                context.hitsounds);
        }
        editor_transport_service::seek_to_tick(
            context.transport,
            &context.state,
            timeline_result.seek_tick,
            context.hitsound_path,
            context.hitsounds);
    }

    const editor_note_edit_result edit_result =
        editor_note_edit_service::apply_timeline_notes(context.state, timeline_result);
    if (edit_result.changed) {
        context.selected_note_indices = edit_result.selected_note_indices;
    }

    return result;
}
