#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "judge_system.h"
#include "platform/windows_input_source.h"

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
    if (!tap_judge->play_hitsound) {
        std::cerr << "Tap judges should still play hitsounds\n";
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
    if (hold_release_judge->offset_ms != -400.0) {
        std::cerr << "Hold release miss should use end timing offset\n";
        return EXIT_FAILURE;
    }
    if (hold_release_judge->play_hitsound) {
        std::cerr << "Hold release judges should not play hitsounds\n";
        return EXIT_FAILURE;
    }

    judge_system hold_release_window_judge;
    hold_release_window_judge.init({note_data{note_type::hold, 960, 1, 1440}}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, true, false, false}, 1000.0);
    hold_release_window_judge.update(1000.0, input);

    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 1390.0);
    hold_release_window_judge.update(1390.0, input);
    const std::optional<judge_event> hold_release_bad = hold_release_window_judge.get_last_judge();
    if (!hold_release_bad.has_value() || hold_release_bad->result != judge_result::bad ||
        hold_release_bad->offset_ms != -110.0) {
        std::cerr << "Early hold release should grade within the shared window\n";
        return EXIT_FAILURE;
    }
    if (hold_release_bad->play_hitsound) {
        std::cerr << "Graded hold release should not play hitsounds\n";
        return EXIT_FAILURE;
    }

    judge_system hold_release_success_judge;
    hold_release_success_judge.init({note_data{note_type::hold, 960, 1, 1440}}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, true, false, false}, 1000.0);
    hold_release_success_judge.update(1000.0, input);
    input.update_from_lane_states(std::array<bool, 4>{false, true, false, false}, 1510.0);
    hold_release_success_judge.update(1510.0, input);
    if (hold_release_success_judge.get_last_judge().has_value()) {
        std::cerr << "Holding through the end should not emit an extra judge\n";
        return EXIT_FAILURE;
    }
    if (hold_release_success_judge.note_states().front().holding) {
        std::cerr << "Hold state should finish once the end timing has passed\n";
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

    windows_input_source::instance().enable_test_mode();
    judge_system native_audio_time_judge;
    native_audio_time_judge.init({note_data{note_type::tap, 480, 0, 480}}, engine);
    input = input_handler();
    input.set_key_count(4);
    windows_input_source::instance().set_test_current_time_ms(30.0);
    windows_input_source::instance().push_test_event({KEY_D, input_event_type::press, 10.0, 1});
    input.update(520.0);
    native_audio_time_judge.update(520.0, input);
    const std::optional<judge_event> native_audio_time_event = native_audio_time_judge.get_last_judge();
    if (!native_audio_time_event.has_value() || native_audio_time_event->result != judge_result::perfect ||
        native_audio_time_event->offset_ms != 0.0) {
        std::cerr << "Native input should be mapped onto audio time\n";
        return EXIT_FAILURE;
    }

    judge_system native_simultaneous_judge;
    native_simultaneous_judge.init({note_data{note_type::tap, 960, 1, 960}, note_data{note_type::tap, 960, 2, 960}}, engine);
    input = input_handler();
    input.set_key_count(4);
    windows_input_source::instance().set_test_current_time_ms(210.0);
    windows_input_source::instance().push_test_event({KEY_F, input_event_type::press, 200.0, 2});
    windows_input_source::instance().push_test_event({KEY_J, input_event_type::press, 200.0, 3});
    input.update(1010.0);
    native_simultaneous_judge.update(1010.0, input);
    const std::vector<judge_event>& native_simultaneous_events = native_simultaneous_judge.get_judge_events();
    if (native_simultaneous_events.size() != 2 || native_simultaneous_events[0].lane != 1 ||
        native_simultaneous_events[1].lane != 2 || native_simultaneous_events[0].offset_ms != 0.0 ||
        native_simultaneous_events[1].offset_ms != 0.0) {
        std::cerr << "Native simultaneous judge failed\n";
        return EXIT_FAILURE;
    }
    windows_input_source::instance().shutdown();

    std::cout << "judge_system smoke test passed\n";
    return EXIT_SUCCESS;
}
