#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "editor_state.h"

namespace {
bool nearly_equal(double left, double right) {
    return std::fabs(left - right) < 0.001;
}

chart_data make_chart() {
    chart_data data;
    data.meta.chart_id = "editor-state-smoke";
    data.meta.key_count = 4;
    data.meta.difficulty = "Normal";
    data.meta.level = 5;
    data.meta.chart_author = "Codex";
    data.meta.format_version = 1;
    data.meta.resolution = 480;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    data.notes = {
        {note_type::tap, 0, 0, 0},
        {note_type::hold, 480, 2, 960},
    };
    return data;
}
}

int main() {
    editor_state state(make_chart(), "assets/charts/editor_state.chart");

    if (state.is_dirty()) {
        std::cerr << "freshly loaded state should not be dirty\n";
        return EXIT_FAILURE;
    }

    if (!nearly_equal(state.engine().tick_to_ms(480), 500.0)) {
        std::cerr << "initial timing engine state is invalid\n";
        return EXIT_FAILURE;
    }

    if (state.snap_tick(119, 16) != 120 || state.snap_tick(421, 8) != 480) {
        std::cerr << "snap_tick did not round to the expected grid\n";
        return EXIT_FAILURE;
    }

    if (!state.has_note_overlap({note_type::tap, 0, 0, 0}) ||
        !state.has_note_overlap({note_type::tap, 700, 2, 700}) ||
        state.has_note_overlap({note_type::tap, 240, 1, 240})) {
        std::cerr << "has_note_overlap returned an unexpected result\n";
        return EXIT_FAILURE;
    }

    state.add_note({note_type::tap, 240, 1, 240});
    if (state.data().notes.size() != 3 || !state.can_undo() || state.can_redo() || !state.is_dirty()) {
        std::cerr << "add_note did not update history correctly\n";
        return EXIT_FAILURE;
    }

    if (!state.modify_note(0, {note_type::tap, 120, 3, 120})) {
        std::cerr << "modify_note should succeed\n";
        return EXIT_FAILURE;
    }

    if (state.modify_note(99, {note_type::tap, 999, 0, 999})) {
        std::cerr << "modify_note should fail for an invalid index\n";
        return EXIT_FAILURE;
    }

    if (state.data().notes[0].tick != 120 || state.data().notes[0].lane != 3) {
        std::cerr << "modify_note did not update note data\n";
        return EXIT_FAILURE;
    }

    if (!state.remove_note(1) || state.data().notes.size() != 2) {
        std::cerr << "remove_note did not remove the selected note\n";
        return EXIT_FAILURE;
    }

    state.add_timing_event({timing_event_type::bpm, 480, 240.0f, 4, 4});
    if (!nearly_equal(state.engine().tick_to_ms(960), 750.0)) {
        std::cerr << "timing engine was not rebuilt after add_timing_event\n";
        return EXIT_FAILURE;
    }

    if (!state.modify_timing_event(0, {timing_event_type::bpm, 0, 150.0f, 4, 4})) {
        std::cerr << "modify_timing_event should succeed\n";
        return EXIT_FAILURE;
    }

    if (state.remove_timing_event(99)) {
        std::cerr << "remove_timing_event should fail for an invalid index\n";
        return EXIT_FAILURE;
    }

    if (!nearly_equal(state.engine().tick_to_ms(480), 400.0)) {
        std::cerr << "timing engine was not rebuilt after modify_timing_event\n";
        return EXIT_FAILURE;
    }

    chart_meta updated_meta = state.data().meta;
    updated_meta.resolution = 960;
    updated_meta.level = 7;
    state.modify_metadata(updated_meta);
    if (state.data().meta.resolution != 960 || state.data().meta.level != 7) {
        std::cerr << "modify_metadata did not update metadata\n";
        return EXIT_FAILURE;
    }

    if (!nearly_equal(state.engine().tick_to_ms(480), 200.0)) {
        std::cerr << "timing engine was not rebuilt after metadata change\n";
        return EXIT_FAILURE;
    }

    state.mark_saved("assets/charts/editor_state_saved.chart");
    if (state.is_dirty() || state.file_path() != "assets/charts/editor_state_saved.chart") {
        std::cerr << "mark_saved did not update save state\n";
        return EXIT_FAILURE;
    }

    state.add_note({note_type::tap, 720, 0, 720});
    if (!state.undo()) {
        std::cerr << "undo should succeed after additional edit\n";
        return EXIT_FAILURE;
    }

    if (!state.can_redo()) {
        std::cerr << "redo should be available after undo\n";
        return EXIT_FAILURE;
    }

    if (!state.redo()) {
        std::cerr << "redo should succeed\n";
        return EXIT_FAILURE;
    }

    if (!state.undo()) {
        std::cerr << "undo should succeed\n";
        return EXIT_FAILURE;
    }

    if (state.data().notes.size() != 2) {
        std::cerr << "undo did not revert last note addition\n";
        return EXIT_FAILURE;
    }

    if (state.is_dirty()) {
        std::cerr << "state should return to clean after undoing back to the saved point\n";
        return EXIT_FAILURE;
    }

    state.add_note({note_type::tap, 840, 1, 840});
    if (state.can_redo()) {
        std::cerr << "redo stack should be cleared by a new command after undo\n";
        return EXIT_FAILURE;
    }

    if (!state.is_dirty()) {
        std::cerr << "state should become dirty after adding a replacement command\n";
        return EXIT_FAILURE;
    }

    if (!state.undo()) {
        std::cerr << "undo should succeed for the replacement command\n";
        return EXIT_FAILURE;
    }

    if (state.is_dirty()) {
        std::cerr << "state should return to clean after undoing to the saved point\n";
        return EXIT_FAILURE;
    }

    std::cout << "editor_state smoke test passed\n";
    return EXIT_SUCCESS;
}
