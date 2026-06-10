#include "editor/service/editor_note_edit_service.h"

#include <algorithm>

#include "editor/service/editor_note_placement_rules.h"

namespace {

std::vector<size_t> normalized_selection(std::vector<size_t> indices) {
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

std::vector<note_data> shifted_notes(std::vector<note_data> notes, int tick_delta) {
    for (note_data& note : notes) {
        note.tick = std::max(0, note.tick + tick_delta);
        note.end_tick = note_has_duration(note)
            ? std::max(note.tick + 1, note.end_tick + tick_delta)
            : note.tick;
    }
    return notes;
}

bool can_place_notes(const editor_state& state, const std::vector<note_data>& notes) {
    return !notes.empty() &&
        !state.has_note_overlap(notes) &&
        !editor::note_placement_rules::has_stay_stack(state, notes);
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

std::vector<size_t> appended_note_indices(const editor_state& state, size_t added_count) {
    std::vector<size_t> indices;
    if (added_count == 0 || added_count > state.data().notes.size()) {
        return indices;
    }

    const size_t first = state.data().notes.size() - added_count;
    for (size_t index = first; index < state.data().notes.size(); ++index) {
        indices.push_back(index);
    }
    return indices;
}

}  // namespace

std::vector<note_data> editor_note_edit_service::notes_for_selection(
    const editor_state& state,
    std::vector<size_t> selected_note_indices) {
    std::vector<size_t> selection = normalized_selection(std::move(selected_note_indices));
    std::vector<note_data> notes;
    notes.reserve(selection.size());
    for (const size_t index : selection) {
        if (index < state.data().notes.size()) {
            notes.push_back(state.data().notes[index]);
        }
    }
    return notes;
}

editor_note_edit_result editor_note_edit_service::paste_or_duplicate(
    editor_state& state,
    const std::vector<size_t>& selected_note_indices,
    const std::vector<note_data>& clipboard_notes,
    bool duplicate_selection) {
    editor_note_edit_result result;
    const std::vector<note_data> source = duplicate_selection
        ? notes_for_selection(state, selected_note_indices)
        : clipboard_notes;
    std::vector<note_data> notes = shifted_notes_to_open_slot(state, source);
    if (!can_place_notes(state, notes)) {
        return result;
    }

    const size_t added_count = notes.size();
    state.add_notes(std::move(notes));
    result.changed = true;
    result.selected_note_indices = appended_note_indices(state, added_count);
    return result;
}

editor_note_edit_result editor_note_edit_service::delete_selection(
    editor_state& state,
    const std::vector<size_t>& selected_note_indices) {
    editor_note_edit_result result;
    std::vector<size_t> selection = normalized_selection(selected_note_indices);
    if (selection.empty() || !state.remove_notes(std::move(selection))) {
        return result;
    }
    result.changed = true;
    return result;
}

editor_note_edit_result editor_note_edit_service::apply_timeline_notes(
    editor_state& state,
    const editor_timeline_result& timeline_result) {
    editor_note_edit_result result;
    result.selected_note_indices = timeline_result.selected_note_indices;

    if (timeline_result.note_to_delete_index.has_value()) {
        if (state.remove_note(*timeline_result.note_to_delete_index)) {
            result.changed = true;
            result.selected_note_indices.clear();
        }
        return result;
    }

    if (timeline_result.note_to_add.has_value() &&
        !editor::note_placement_rules::has_stay_stack(state, *timeline_result.note_to_add)) {
        state.add_note(*timeline_result.note_to_add);
        result.changed = true;
        result.selected_note_indices = state.data().notes.empty()
            ? std::vector<size_t>{}
            : std::vector<size_t>{state.data().notes.size() - 1};
        return result;
    }

    if (timeline_result.note_to_modify_index.has_value() && timeline_result.note_to_modify.has_value() &&
        !editor::note_placement_rules::has_stay_stack(
            state, *timeline_result.note_to_modify, timeline_result.note_to_modify_index)) {
        if (state.modify_note(*timeline_result.note_to_modify_index, *timeline_result.note_to_modify)) {
            result.changed = true;
            result.selected_note_indices = {*timeline_result.note_to_modify_index};
        }
        return result;
    }

    if (timeline_result.notes_to_modify.empty()) {
        return result;
    }

    std::vector<size_t> ignore_indices;
    std::vector<note_data> updated_notes;
    ignore_indices.reserve(timeline_result.notes_to_modify.size());
    updated_notes.reserve(timeline_result.notes_to_modify.size());
    for (const auto& update : timeline_result.notes_to_modify) {
        ignore_indices.push_back(update.first);
        updated_notes.push_back(update.second);
    }

    if (!state.has_note_overlap(updated_notes, ignore_indices) &&
        !editor::note_placement_rules::has_stay_stack(state, updated_notes, ignore_indices) &&
        state.modify_notes(timeline_result.notes_to_modify)) {
        result.changed = true;
        result.selected_note_indices = timeline_result.selected_note_indices;
    }
    return result;
}
