#include <cstdlib>
#include <iostream>
#include <vector>

#include "play_note_draw_queue.h"

int main() {
    play_note_draw_queue draw_queue;

    std::vector<note_state> note_states = {
        {note_data{note_type::tap, 0, 0, 0}, 1000.0, 1000.0, false, judge_result::miss, false},
        {note_data{note_type::tap, 0, 0, 0}, 1200.0, 1200.0, false, judge_result::miss, false},
        {note_data{note_type::hold, 0, 1, 480}, 1300.0, 1500.0, false, judge_result::miss, false},
        {note_data{note_type::tap, 0, 1, 0}, 900.0, 900.0, false, judge_result::miss, false},
    };

    draw_queue.init_from_note_states(4, note_states);
    draw_queue.update_visible_window(note_states, 0.1f, 10.0f, 5.0f, 40.0f, 1000.0);

    const std::vector<size_t>& lane0_active = draw_queue.active_indices_for_lane(0);
    if (lane0_active.size() != 2 || lane0_active[0] != 0 || lane0_active[1] != 1) {
        std::cerr << "Lane 0 activation order failed\n";
        return EXIT_FAILURE;
    }

    const std::vector<size_t>& lane1_active = draw_queue.active_indices_for_lane(1);
    if (lane1_active.size() != 1 || lane1_active[0] != 2) {
        std::cerr << "Lane 1 active filtering failed\n";
        return EXIT_FAILURE;
    }

    if (!draw_queue.has_active_notes()) {
        std::cerr << "Active note detection failed\n";
        return EXIT_FAILURE;
    }

    note_states[2].judged = true;
    note_states[2].holding = true;
    draw_queue.update_visible_window(note_states, 0.1f, 10.0f, 15.0f, 40.0f, 2000.0);
    if (draw_queue.active_indices_for_lane(1).size() != 1) {
        std::cerr << "Holding note should remain active\n";
        return EXIT_FAILURE;
    }

    note_states[2].holding = false;
    draw_queue.update_visible_window(note_states, 0.1f, 10.0f, 15.0f, 40.0f, 2000.0);
    if (!draw_queue.active_indices_for_lane(1).empty()) {
        std::cerr << "Finished hold note should be removed\n";
        return EXIT_FAILURE;
    }

    std::cout << "play_note_draw_queue smoke test passed\n";
    return EXIT_SUCCESS;
}
