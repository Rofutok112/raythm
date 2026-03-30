#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "judge_system.h"

int main() {
    timing_engine engine;
    engine.init({timing_event{timing_event_type::bpm, 0, 120.0f, 4, 4}}, 480);

    std::vector<note_data> notes = {
        {note_type::tap, 480, 0, 480},
        {note_type::hold, 960, 1, 1440},
        {note_type::tap, 960, 2, 960},
    };

    judge_system judge;
    judge.init(notes, engine);

    input_handler input;
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 0.0);

    input.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 500.0);
    judge.update(500.0, input);
    const std::optional<judge_event> tap_judge = judge.get_last_judge();
    if (!tap_judge.has_value() || tap_judge->result != judge_result::perfect || tap_judge->lane != 0) {
        std::cerr << "Tap perfect judge failed\n";
        return EXIT_FAILURE;
    }
    if (tap_judge->offset_ms != 0.0) {
        std::cerr << "Tap judge should use event timestamp\n";
        return EXIT_FAILURE;
    }

    input.update_from_lane_states(std::array<bool, 4>{false, true, true, false}, 1000.0);
    judge.update(1000.0, input);
    const std::vector<judge_event>& simultaneous_judges = judge.get_judge_events();
    if (simultaneous_judges.size() != 2 || simultaneous_judges[0].lane != 1 || simultaneous_judges[1].lane != 2) {
        std::cerr << "Simultaneous press judges failed\n";
        return EXIT_FAILURE;
    }
    const std::optional<judge_event> simultaneous_judge = judge.get_last_judge();
    if (!simultaneous_judge.has_value() || simultaneous_judge->lane != 2) {
        std::cerr << "Last simultaneous judge failed\n";
        return EXIT_FAILURE;
    }

    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 1100.0);
    judge.update(1100.0, input);
    const std::optional<judge_event> hold_release_judge = judge.get_last_judge();
    if (!hold_release_judge.has_value() || hold_release_judge->result != judge_result::miss ||
        hold_release_judge->lane != 1) {
        std::cerr << "Hold release miss failed\n";
        return EXIT_FAILURE;
    }

    judge_system miss_judge;
    miss_judge.init({note_data{note_type::tap, 480, 0, 480}}, engine);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 1200.0);
    miss_judge.update(700.0, input);
    const std::optional<judge_event> auto_miss = miss_judge.get_last_judge();
    if (!auto_miss.has_value() || auto_miss->result != judge_result::miss) {
        std::cerr << "Automatic miss failed\n";
        return EXIT_FAILURE;
    }

    judge_system timestamp_judge;
    timestamp_judge.init({note_data{note_type::tap, 480, 0, 480}}, engine);
    input.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 515.0);
    timestamp_judge.update(590.0, input);
    const std::optional<judge_event> timestamp_event = timestamp_judge.get_last_judge();
    if (!timestamp_event.has_value() || timestamp_event->result != judge_result::perfect ||
        timestamp_event->offset_ms != 15.0) {
        std::cerr << "Judgement should be based on input event timestamp\n";
        return EXIT_FAILURE;
    }

    judge_system judge_6k;
    judge_6k.init({note_data{note_type::tap, 480, 5, 480}}, engine);
    input.set_key_count(6);
    input.update_from_lane_states(std::array<bool, 6>{false, false, false, false, false, true}, 500.0);
    judge_6k.update(500.0, input);
    const std::optional<judge_event> six_key_judge = judge_6k.get_last_judge();
    if (!six_key_judge.has_value() || six_key_judge->result != judge_result::perfect ||
        six_key_judge->lane != 5) {
        std::cerr << "6-key judge path failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "judge_system smoke test passed\n";
    return EXIT_SUCCESS;
}
