#include "editor_runtime_controller.h"

#include <algorithm>

#include "editor/editor_timeline_controller.h"
#include "editor/service/editor_note_placement_rules.h"
#include "editor/service/editor_transport_service.h"

namespace {

bool has_active_metadata_input(const metadata_panel_state& metadata_panel) {
    return metadata_panel.difficulty_input.active || metadata_panel.chart_author_input.active;
}

std::vector<size_t> normalized_selection(std::vector<size_t> indices, std::optional<size_t> fallback) {
    if (indices.empty() && fallback.has_value()) {
        indices.push_back(*fallback);
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

std::vector<note_data> notes_for_indices(const editor_state& state, const std::vector<size_t>& indices) {
    std::vector<note_data> notes;
    notes.reserve(indices.size());
    for (const size_t index : indices) {
        if (index < state.data().notes.size()) {
            notes.push_back(state.data().notes[index]);
        }
    }
    return notes;
}

std::vector<note_data> shifted_notes(std::vector<note_data> notes, int tick_delta) {
    for (note_data& note : notes) {
        note.tick = std::max(0, note.tick + tick_delta);
        note.end_tick = note.type == note_type::hold
            ? std::max(note.tick + 1, note.end_tick + tick_delta)
            : note.tick;
    }
    return notes;
}

bool can_place_notes(const editor_state& state, const std::vector<note_data>& notes) {
    return !notes.empty() &&
        !state.has_note_overlap(notes) &&
        !editor::note_placement_rules::has_stay_stack(state.data(), notes);
}

std::vector<note_data> shifted_notes_to_open_slot(const editor_state& state, const std::vector<note_data>& source) {
    const int tick_step = std::max(1, state.data().meta.resolution);
    for (int step = 1; step <= 64; ++step) {
        std::vector<note_data> notes = shifted_notes(source, tick_step * step);
        if (can_place_notes(state, notes)) {
            return notes;
        }
    }
    return {};
}

void select_appended_notes(const editor_state& state,
                           size_t added_count,
                           std::optional<size_t>& selected_note_index,
                           std::vector<size_t>& selected_note_indices) {
    selected_note_indices.clear();
    if (added_count == 0 || added_count > state.data().notes.size()) {
        selected_note_index.reset();
        return;
    }

    const size_t first = state.data().notes.size() - added_count;
    for (size_t index = first; index < state.data().notes.size(); ++index) {
        selected_note_indices.push_back(index);
    }
    selected_note_index = selected_note_indices.back();
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
        const std::vector<size_t> selection = normalized_selection(context.selected_note_indices, context.selected_note_index);
        context.clipboard_notes = notes_for_indices(context.state, selection);
    }

    if (!editing_blocked && context.ctrl_down && (context.v_pressed || context.d_pressed)) {
        const std::vector<size_t> selection = normalized_selection(context.selected_note_indices, context.selected_note_index);
        std::vector<note_data> notes = context.d_pressed
            ? notes_for_indices(context.state, selection)
            : context.clipboard_notes;
        notes = shifted_notes_to_open_slot(context.state, notes);
        if (can_place_notes(context.state, notes)) {
            const size_t added_count = notes.size();
            context.state.add_notes(std::move(notes));
            select_appended_notes(context.state, added_count, context.selected_note_index, context.selected_note_indices);
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
        std::vector<size_t> selection = normalized_selection(context.selected_note_indices, context.selected_note_index);
        if (!selection.empty() && context.state.remove_notes(selection)) {
            context.selected_note_index.reset();
            context.selected_note_indices.clear();
            result.history_changed = true;
        }
    }

    if (context.selected_note_index.has_value() &&
        *context.selected_note_index >= context.state.data().notes.size()) {
        context.selected_note_index.reset();
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
        context.alt_down,
        context.snap_division,
        context.selected_note_index,
        context.drag_state,
        context.palette,
        context.selected_note_indices,
        context.ctrl_down,
        context.shift_down,
    });

    context.selected_note_index = timeline_result.selected_note_index;
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
        if (was_playing || timeline_result.scroll_seek_if_paused) {
            result.scroll_to_tick = context.transport.playback_tick;
        }
    }

    if (timeline_result.note_to_delete_index.has_value()) {
        if (context.state.remove_note(*timeline_result.note_to_delete_index)) {
            context.selected_note_index.reset();
            context.selected_note_indices.clear();
        }
    }

    if (!timeline_result.notes_to_delete_indices.empty()) {
        if (context.state.remove_notes(timeline_result.notes_to_delete_indices)) {
            context.selected_note_index.reset();
            context.selected_note_indices.clear();
        }
    }

    if (timeline_result.note_to_add.has_value() &&
        !editor::note_placement_rules::has_stay_stack(context.state.data(), *timeline_result.note_to_add)) {
        context.state.add_note(*timeline_result.note_to_add);
        context.selected_note_index = context.state.data().notes.empty()
            ? std::nullopt
            : std::optional<size_t>(context.state.data().notes.size() - 1);
        context.selected_note_indices = context.selected_note_index.has_value()
            ? std::vector<size_t>{*context.selected_note_index}
            : std::vector<size_t>{};
    }

    if (timeline_result.note_to_modify_index.has_value() && timeline_result.note_to_modify.has_value() &&
        !editor::note_placement_rules::has_stay_stack(
            context.state.data(), *timeline_result.note_to_modify, timeline_result.note_to_modify_index)) {
        if (context.state.modify_note(*timeline_result.note_to_modify_index, *timeline_result.note_to_modify)) {
            context.selected_note_index = timeline_result.note_to_modify_index;
            context.selected_note_indices = context.selected_note_index.has_value()
                ? std::vector<size_t>{*context.selected_note_index}
                : std::vector<size_t>{};
        }
    }

    if (!timeline_result.notes_to_modify.empty()) {
        std::vector<size_t> ignore_indices;
        std::vector<note_data> updated_notes;
        ignore_indices.reserve(timeline_result.notes_to_modify.size());
        updated_notes.reserve(timeline_result.notes_to_modify.size());
        for (const auto& update : timeline_result.notes_to_modify) {
            ignore_indices.push_back(update.first);
            updated_notes.push_back(update.second);
        }
        if (!context.state.has_note_overlap(updated_notes, ignore_indices) &&
            !editor::note_placement_rules::has_stay_stack(context.state.data(), updated_notes, ignore_indices) &&
            context.state.modify_notes(timeline_result.notes_to_modify)) {
            context.selected_note_indices = timeline_result.selected_note_indices;
            context.selected_note_index = timeline_result.selected_note_index;
        }
    }

    if (timeline_result.scroll_event_to_modify_index.has_value() &&
        timeline_result.scroll_event_to_modify.has_value()) {
        if (context.state.modify_scroll_event(*timeline_result.scroll_event_to_modify_index,
                                              *timeline_result.scroll_event_to_modify)) {
            result.selected_scroll_event_index = timeline_result.scroll_event_to_modify_index;
            result.scroll_event_modified = true;
            result.scroll_to_tick = timeline_result.scroll_event_to_modify->tick;
        }
    }

    return result;
}
