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
    if (!tap_judge->apply_gameplay_effects) {
        std::cerr << "Tap judges should still affect gameplay\n";
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
    if (!judge.note_states()[1].is_judged() || judge.note_states()[1].is_completed() ||
        !judge.note_states()[1].is_holding()) {
        std::cerr << "Hold head judge should keep the note active until completion\n";
        return EXIT_FAILURE;
    }
    if (judge.get_judge_events().size() != 2) {
        std::cerr << "Hold head judge should not immediately emit an extra miss\n";
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
    if (!judge.note_states()[1].is_completed() || judge.note_states()[1].is_holding()) {
        std::cerr << "Released hold should be marked completed\n";
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
    if (!hold_release_judge->apply_gameplay_effects) {
        std::cerr << "Early hold release miss should still affect gameplay\n";
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
    if (!hold_release_bad.has_value() || hold_release_bad->result != judge_result::good ||
        hold_release_bad->offset_ms != -110.0) {
        std::cerr << "Early hold release should grade within the shared window\n";
        return EXIT_FAILURE;
    }
    if (hold_release_bad->play_hitsound) {
        std::cerr << "Graded hold release should not play hitsounds\n";
        return EXIT_FAILURE;
    }
    if (!hold_release_bad->apply_gameplay_effects) {
        std::cerr << "Graded hold release should still affect gameplay\n";
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
    const std::optional<judge_event> hold_release_success = hold_release_success_judge.get_last_judge();
    if (!hold_release_success.has_value() || hold_release_success->result != judge_result::perfect) {
        std::cerr << "Holding through the end should emit a perfect display judge\n";
        return EXIT_FAILURE;
    }
    if (hold_release_success->play_hitsound || !hold_release_success->apply_gameplay_effects ||
        !hold_release_success->show_feedback) {
        std::cerr << "Hold completion judge should score without replaying hitsounds\n";
        return EXIT_FAILURE;
    }
    if (!hold_release_success_judge.note_states().front().is_completed()) {
        std::cerr << "Successful hold should be marked completed at the end\n";
        return EXIT_FAILURE;
    }
    if (hold_release_success_judge.note_states().front().is_holding()) {
        std::cerr << "Hold state should finish once the end timing has passed\n";
        return EXIT_FAILURE;
    }

    judge_system hold_release_after_end_judge;
    hold_release_after_end_judge.init({note_data{note_type::hold, 960, 1, 1440}}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, true, false, false}, 1000.0);
    hold_release_after_end_judge.update(1000.0, input);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 1510.0);
    hold_release_after_end_judge.update(1510.0, input);
    if (!hold_release_after_end_judge.note_states().front().is_completed() ||
        hold_release_after_end_judge.note_states().front().is_holding()) {
        std::cerr << "Releasing after hold end should still complete the note\n";
        return EXIT_FAILURE;
    }
    const std::optional<judge_event> hold_release_after_end = hold_release_after_end_judge.get_last_judge();
    if (!hold_release_after_end.has_value() || hold_release_after_end->play_hitsound ||
        !hold_release_after_end->apply_gameplay_effects || hold_release_after_end->show_feedback) {
        std::cerr << "Releasing after hold end should still award the tail without extra feedback\n";
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

    judge_system hold_auto_miss_judge;
    hold_auto_miss_judge.init({note_data{note_type::hold, 480, 0, 960}}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 1200.0);
    hold_auto_miss_judge.update(700.0, input);
    const std::vector<judge_event>& hold_auto_miss_events = hold_auto_miss_judge.get_judge_events();
    if (hold_auto_miss_events.size() != 2 ||
        hold_auto_miss_events[0].event_index != 0 ||
        hold_auto_miss_events[1].event_index != 1 ||
        hold_auto_miss_events[0].result != judge_result::miss ||
        hold_auto_miss_events[1].result != judge_result::miss) {
        std::cerr << "Hold automatic miss should emit head and tail misses\n";
        return EXIT_FAILURE;
    }
    if (!hold_auto_miss_events[0].show_feedback || hold_auto_miss_events[1].show_feedback) {
        std::cerr << "Hold automatic tail miss should score without replacing feedback\n";
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

    judge_system lane_progression_judge;
    lane_progression_judge.init({note_data{note_type::tap, 480, 0, 480}, note_data{note_type::tap, 960, 0, 960}}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 1000.0);
    lane_progression_judge.update(1000.0, input);
    const std::vector<note_state>& lane_progression_states = lane_progression_judge.note_states();
    if (!lane_progression_states[1].is_completed() ||
        lane_progression_states[1].result != judge_result::perfect) {
        std::cerr << "Lane-based candidate search should still reach the next in-window note\n";
        return EXIT_FAILURE;
    }

    judge_system adjacent_hold_judge;
    adjacent_hold_judge.init({note_data{note_type::hold, 960, 1, 1440}, note_data{note_type::hold, 1440, 1, 1920}}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, true, false, false}, 1000.0);
    adjacent_hold_judge.update(1000.0, input);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 1460.0);
    adjacent_hold_judge.update(1460.0, input);
    input.update_from_lane_states(std::array<bool, 4>{false, true, false, false}, 1520.0);
    adjacent_hold_judge.update(1520.0, input);
    if (!adjacent_hold_judge.note_states()[0].is_completed() || !adjacent_hold_judge.note_states()[1].is_holding() ||
        adjacent_hold_judge.note_states()[1].is_completed()) {
        std::cerr << "Next same-lane hold should stay active after previous hold ends\n";
        return EXIT_FAILURE;
    }
    if (adjacent_hold_judge.note_states()[1].result != judge_result::perfect) {
        std::cerr << "Next same-lane hold should still receive its graded head judge: actual="
                  << static_cast<int>(adjacent_hold_judge.note_states()[1].result) << "\n";
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

    judge_system native_flick_release_judge;
    native_flick_release_judge.init({note_data{note_type::release, 960, 0, 960}}, engine);
    input = input_handler();
    input.set_key_count(4);
    windows_input_source::instance().set_test_current_time_ms(1010.0);
    windows_input_source::instance().push_test_event({KEY_D, input_event_type::press, 900.0, 7});
    windows_input_source::instance().push_test_event({KEY_D, input_event_type::release, 1000.0, 8});
    input.update(1010.0);
    native_flick_release_judge.update(1010.0, input);
    const std::optional<judge_event> native_flick_release_event = native_flick_release_judge.get_last_judge();
    if (!native_flick_release_event.has_value() ||
        native_flick_release_event->result != judge_result::perfect ||
        native_flick_release_event->hitsound_type != note_type::release) {
        std::cerr << "Native same-frame flick release should arm before release judgement\n";
        return EXIT_FAILURE;
    }
    windows_input_source::instance().shutdown();

    judge_system release_note_judge;
    release_note_judge.init({note_data{note_type::release, 960, 0, 960}}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 900.0);
    release_note_judge.update(900.0, input);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 1080.0);
    release_note_judge.update(1080.0, input);
    if (!release_note_judge.get_last_judge().has_value() ||
        release_note_judge.get_last_judge()->result != judge_result::perfect) {
        std::cerr << "Release note should award perfect anywhere inside the bad window\n";
        return EXIT_FAILURE;
    }
    if (release_note_judge.get_last_judge()->hitsound_type != note_type::release) {
        std::cerr << "Release note should request a release hitsound\n";
        return EXIT_FAILURE;
    }

    judge_system tap_release_absorb_judge;
    tap_release_absorb_judge.init({
        note_data{note_type::tap, 840, 0, 840},
        note_data{note_type::release, 960, 0, 960},
    }, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 880.0);
    tap_release_absorb_judge.update(880.0, input);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 940.0);
    tap_release_absorb_judge.update(940.0, input);
    if (tap_release_absorb_judge.note_states()[1].is_completed()) {
        std::cerr << "Tap release should not arm a standalone release note\n";
        return EXIT_FAILURE;
    }
    tap_release_absorb_judge.update(1170.0, input);
    if (!tap_release_absorb_judge.note_states()[1].is_completed() ||
        tap_release_absorb_judge.note_states()[1].result != judge_result::miss) {
        std::cerr << "Unarmed standalone release should miss after its window\n";
        return EXIT_FAILURE;
    }

    judge_system stay_held_judge;
    stay_held_judge.init({note_data{note_type::stay, 960, 1, 960, true}}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, true, false, false}, 900.0);
    stay_held_judge.update(900.0, input);
    stay_held_judge.update(1000.0, input);
    if (!stay_held_judge.get_last_judge().has_value() ||
        stay_held_judge.get_last_judge()->result != judge_result::perfect) {
        std::cerr << "Stay note should perfect when held through the target\n";
        return EXIT_FAILURE;
    }
    if (stay_held_judge.get_last_judge()->hitsound_type != note_type::stay ||
        !stay_held_judge.get_last_judge()->is_ray) {
        std::cerr << "Stay note should request a ray stay hitsound\n";
        return EXIT_FAILURE;
    }

    judge_system tap_stay_judge;
    tap_stay_judge.init({
        note_data{note_type::tap, 960, 0, 960},
        note_data{note_type::stay, 960, 0, 960},
    }, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 1000.0);
    tap_stay_judge.update(1000.0, input);
    if (tap_stay_judge.get_judge_events().size() != 2 ||
        !tap_stay_judge.note_states()[0].is_completed() ||
        !tap_stay_judge.note_states()[1].is_completed()) {
        std::cerr << "Tap and stay on the same tick should both judge from one press\n";
        return EXIT_FAILURE;
    }
    if (!tap_stay_judge.get_judge_events()[0].play_hitsound ||
        tap_stay_judge.get_judge_events()[1].play_hitsound) {
        std::cerr << "Stacked press notes should only play one hitsound\n";
        return EXIT_FAILURE;
    }

    judge_system hold_head_stay_judge;
    hold_head_stay_judge.init({
        note_data{note_type::hold, 960, 1, 1440},
        note_data{note_type::stay, 960, 1, 960},
    }, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, true, false, false}, 1000.0);
    hold_head_stay_judge.update(1000.0, input);
    if (hold_head_stay_judge.get_judge_events().size() != 2 ||
        !hold_head_stay_judge.note_states()[0].is_holding() ||
        !hold_head_stay_judge.note_states()[1].is_completed()) {
        std::cerr << "Hold head and stay on the same tick should both judge from one press\n";
        return EXIT_FAILURE;
    }

    judge_system stay_release_judge;
    stay_release_judge.init({
        note_data{note_type::stay, 960, 2, 960},
        note_data{note_type::release, 960, 2, 960},
    }, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, false, true, false}, 900.0);
    stay_release_judge.update(900.0, input);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 950.0);
    stay_release_judge.update(950.0, input);
    if (stay_release_judge.get_judge_events().size() != 2 ||
        !stay_release_judge.note_states()[0].is_completed() ||
        !stay_release_judge.note_states()[1].is_completed()) {
        std::cerr << "Stay and release should both judge from one release\n";
        return EXIT_FAILURE;
    }

    judge_system replaced_tail_judge;
    replaced_tail_judge.init({
        note_data{note_type::hold, 480, 0, 960},
        note_data{note_type::release, 960, 0, 960},
    }, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 500.0);
    replaced_tail_judge.update(500.0, input);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 1000.0);
    replaced_tail_judge.update(1000.0, input);
    if (replaced_tail_judge.get_judge_events().size() != 2 ||
        !replaced_tail_judge.note_states()[0].is_completed() ||
        !replaced_tail_judge.note_states()[1].is_completed()) {
        std::cerr << "Release note should stack with overlapping hold tail judgement\n";
        return EXIT_FAILURE;
    }
    if (replaced_tail_judge.get_judge_events()[0].event_index ==
        replaced_tail_judge.get_judge_events()[1].event_index) {
        std::cerr << "Stacked hold tail and release should emit separate judge events\n";
        return EXIT_FAILURE;
    }
    int replaced_tail_hitsounds = 0;
    int replaced_tail_feedback = 0;
    for (const judge_event& event : replaced_tail_judge.get_judge_events()) {
        if (event.play_hitsound) {
            ++replaced_tail_hitsounds;
        }
        if (event.show_feedback) {
            ++replaced_tail_feedback;
        }
    }
    if (replaced_tail_hitsounds != 1 || replaced_tail_feedback != 1 ||
        replaced_tail_judge.get_last_judge()->hitsound_type != note_type::release) {
        std::cerr << "Stacked hold tail and release should present the release note once\n";
        return EXIT_FAILURE;
    }

    note_data wide_tap{note_type::tap, 960, 1, 960};
    wide_tap.lane_width = 3;
    judge_system wide_tap_judge;
    wide_tap_judge.init({wide_tap}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, false, true, false}, 1000.0);
    wide_tap_judge.update(1000.0, input);
    if (!wide_tap_judge.get_last_judge().has_value() ||
        wide_tap_judge.get_last_judge()->result != judge_result::perfect ||
        !wide_tap_judge.note_states().front().is_completed()) {
        std::cerr << "Wide tap should judge from any covered lane\n";
        return EXIT_FAILURE;
    }
    if (wide_tap_judge.get_last_judge()->lane != 1 ||
        wide_tap_judge.get_last_judge()->lane_width != 3) {
        std::cerr << "Wide tap judge event should preserve its full lane span\n";
        return EXIT_FAILURE;
    }

    note_data wide_hold{note_type::hold, 960, 0, 1440};
    wide_hold.lane_width = 2;
    judge_system wide_hold_judge;
    wide_hold_judge.init({wide_hold}, engine);
    input = input_handler();
    input.set_key_count(4);
    input.update_from_lane_states(std::array<bool, 4>{false, true, false, false}, 1000.0);
    wide_hold_judge.update(1000.0, input);
    input.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 1390.0);
    wide_hold_judge.update(1390.0, input);
    if (!wide_hold_judge.get_last_judge().has_value() ||
        wide_hold_judge.get_last_judge()->result != judge_result::good ||
        !wide_hold_judge.note_states().front().is_completed()) {
        std::cerr << "Wide hold should track releases from any covered lane\n";
        return EXIT_FAILURE;
    }
    if (wide_hold_judge.get_last_judge()->lane != 0 ||
        wide_hold_judge.get_last_judge()->lane_width != 2) {
        std::cerr << "Wide hold release judge event should preserve its full lane span\n";
        return EXIT_FAILURE;
    }

    std::cout << "judge_system smoke test passed\n";
    return EXIT_SUCCESS;
}
