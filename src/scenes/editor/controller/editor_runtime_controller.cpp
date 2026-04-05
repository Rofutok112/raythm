#include "editor_runtime_controller.h"

#include "editor/editor_timeline_controller.h"
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

    if (!metadata_input_active &&
        !timing_input_active &&
        !context.timing_panel.bar_pick_mode &&
        !context.timeline_drag.active &&
        context.space_pressed) {
        result.restore_scroll_tick = editor_transport_service::toggle_playback(
            context.transport,
            &context.state,
            context.space_playback_start_tick,
            context.hitsound_path);
    }

    if (context.ctrl_down && context.z_pressed) {
        if (context.shift_down) {
            context.state.redo();
        } else {
            context.state.undo();
        }
        editor_scene_sync::sync_after_history_change(context.sync_context);
        editor_transport_service::sync(context.transport, &context.state, context.hitsound_path, true);
    }

    if (context.ctrl_down && context.y_pressed) {
        context.state.redo();
        editor_scene_sync::sync_after_history_change(context.sync_context);
        editor_transport_service::sync(context.transport, &context.state, context.hitsound_path, true);
    }

    if (!metadata_input_active &&
        !timing_input_active &&
        context.delete_pressed &&
        context.selected_note_index.has_value()) {
        const size_t selected_index = *context.selected_note_index;
        if (context.state.remove_note(selected_index)) {
            context.selected_note_index.reset();
        }
    }

    if (context.selected_note_index.has_value() &&
        *context.selected_note_index >= context.state.data().notes.size()) {
        context.selected_note_index.reset();
    }

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
        context.alt_down,
        context.snap_division,
        context.selected_note_index,
        context.drag_state,
    });

    context.selected_note_index = timeline_result.selected_note_index;
    context.drag_state = timeline_result.drag_state;

    editor_runtime_timeline_result result;
    result.request_apply_selected_timing = timeline_result.request_apply_selected_timing;

    if (timeline_result.request_seek) {
        const bool was_playing = context.transport.audio_playing;
        if (was_playing) {
            editor_transport_service::pause_for_seek(
                context.transport,
                &context.state,
                context.space_playback_start_tick,
                context.hitsound_path);
        }
        editor_transport_service::seek_to_tick(
            context.transport,
            &context.state,
            timeline_result.seek_tick,
            context.hitsound_path);
        if (was_playing || timeline_result.scroll_seek_if_paused) {
            result.scroll_to_tick = context.transport.playback_tick;
        }
    }

    if (timeline_result.note_to_delete_index.has_value()) {
        if (context.state.remove_note(*timeline_result.note_to_delete_index)) {
            context.selected_note_index.reset();
        }
    }

    if (timeline_result.note_to_add.has_value()) {
        context.state.add_note(*timeline_result.note_to_add);
        context.selected_note_index = context.state.data().notes.empty()
            ? std::nullopt
            : std::optional<size_t>(context.state.data().notes.size() - 1);
    }

    return result;
}
