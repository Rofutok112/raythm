#include <cstdlib>
#include <iostream>
#include <vector>

#include "editor/service/editor_note_edit_service.h"

namespace {

chart_data make_chart() {
    chart_data data;
    data.meta.chart_id = "editor-note-edit-service-smoke";
    data.meta.song_id = "song";
    data.meta.key_count = 4;
    data.meta.difficulty = "Normal";
    data.meta.chart_author = "Codex";
    data.meta.format_version = 1;
    data.meta.resolution = 480;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    data.notes = {
        {note_type::tap, 0, 0, 0},
        {note_type::tap, 480, 1, 480},
    };
    return data;
}

}  // namespace

int main() {
    editor_state state(make_chart(), "");

    std::vector<note_data> copied =
        editor_note_edit_service::notes_for_selection(state, {1, 1, 99});
    if (copied.size() != 1 || copied.front().tick != 480) {
        std::cerr << "notes_for_selection should normalize selection and ignore invalid indices\n";
        return EXIT_FAILURE;
    }

    editor_note_edit_result duplicate =
        editor_note_edit_service::paste_or_duplicate(state, {1}, {}, true);
    if (!duplicate.changed || duplicate.selected_note_indices.size() != 1 ||
        state.data().notes.size() != 3 || state.data().notes.back().tick != 960) {
        std::cerr << "duplicate should add a shifted copy as one note operation\n";
        return EXIT_FAILURE;
    }
    if (!state.undo() || state.data().notes.size() != 2) {
        std::cerr << "duplicate should be undoable as one command\n";
        return EXIT_FAILURE;
    }

    editor_note_edit_result pasted =
        editor_note_edit_service::paste_or_duplicate(state, {}, copied, false);
    if (!pasted.changed || pasted.selected_note_indices.empty() ||
        state.data().notes.back().tick != 960) {
        std::cerr << "paste should find the next open slot\n";
        return EXIT_FAILURE;
    }

    editor_timeline_result add_result;
    add_result.selected_note_indices = pasted.selected_note_indices;
    add_result.note_to_add = note_data{note_type::tap, 240, 2, 240};
    const editor_note_edit_result added =
        editor_note_edit_service::apply_timeline_notes(state, add_result);
    if (!added.changed || added.selected_note_indices.size() != 1 ||
        state.data().notes.back().tick != 240) {
        std::cerr << "timeline add should be applied through note edit service\n";
        return EXIT_FAILURE;
    }

    editor_timeline_result modify_result;
    modify_result.selected_note_indices = added.selected_note_indices;
    modify_result.note_to_modify_index = *added.selected_note_indices.begin();
    modify_result.note_to_modify = note_data{note_type::tap, 360, 3, 360};
    const editor_note_edit_result modified =
        editor_note_edit_service::apply_timeline_notes(state, modify_result);
    if (!modified.changed || state.data().notes[*modified.selected_note_indices.begin()].tick != 360) {
        std::cerr << "timeline modify should keep selection on modified note\n";
        return EXIT_FAILURE;
    }

    const editor_note_edit_result deleted =
        editor_note_edit_service::delete_selection(state, modified.selected_note_indices);
    if (!deleted.changed || !deleted.selected_note_indices.empty() ||
        state.data().notes.size() != 3) {
        std::cerr << "delete selection should clear selection and remove notes\n";
        return EXIT_FAILURE;
    }

    if (!state.undo() || state.data().notes.size() != 4) {
        std::cerr << "delete selection should be undoable\n";
        return EXIT_FAILURE;
    }

    std::cout << "editor_note_edit_service smoke test passed\n";
    return EXIT_SUCCESS;
}
