#include <cstdlib>
#include <cmath>
#include <iostream>
#include <vector>

#include "play/play_note_draw_queue.h"
#include "play/play_scroll_map.h"
#include "play/play_speed_compensation.h"
#include "timing_engine.h"

int main() {
    {
        constexpr float kNoteSpeed = 0.045f;
        const float reference_speed =
            play_speed_compensation::compensated_lane_speed(kNoteSpeed, 45.0f);
        if (std::fabs(reference_speed - kNoteSpeed) > 0.0001f) {
            std::cerr << "Reference camera angle compensation failed\n";
            return EXIT_FAILURE;
        }

        const float shallow_speed =
            play_speed_compensation::compensated_lane_speed(kNoteSpeed, 20.0f);
        const float steep_speed =
            play_speed_compensation::compensated_lane_speed(kNoteSpeed, 85.0f);
        if (!(shallow_speed > reference_speed) || !(steep_speed < reference_speed)) {
            std::cerr << "Angle-based note speed compensation direction failed\n";
            return EXIT_FAILURE;
        }

        const float shallow_screen_speed =
            shallow_speed * play_speed_compensation::judge_line_projection_scale(20.0f);
        const float reference_screen_speed =
            reference_speed * play_speed_compensation::judge_line_projection_scale(45.0f);
        if (std::fabs(shallow_screen_speed - reference_screen_speed) > 0.0001f) {
            std::cerr << "Angle-based note speed compensation normalization failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        chart_data chart;
        chart.meta.resolution = 480;
        chart.timing_events = {
            {.type = timing_event_type::bpm, .tick = 0, .bpm = 120.0f, .numerator = 4, .denominator = 4},
        };
        chart.scroll_events = {
            {.type = scroll_event_type::speed, .tick = 480, .duration = 480, .multiplier = 2.0f},
            {.type = scroll_event_type::stop, .tick = 1440, .duration = 240, .multiplier = 0.0f},
        };
        timing_engine timing;
        timing.init(chart.timing_events, chart.meta.resolution, 0);
        play_scroll_map scroll_map;
        scroll_map.init(chart, timing);
        const double before = scroll_map.visual_ms_at(timing.tick_to_ms(480));
        const double after_fast = scroll_map.visual_ms_at(timing.tick_to_ms(960));
        const double before_stop = scroll_map.visual_ms_at(timing.tick_to_ms(1440));
        const double after_stop = scroll_map.visual_ms_at(timing.tick_to_ms(1680));
        if (std::fabs((after_fast - before) - 1000.0) > 0.0001 ||
            std::fabs(after_stop - before_stop) > 0.0001) {
            std::cerr << "Scroll map speed/stop conversion failed\n";
            return EXIT_FAILURE;
        }
    }

    play_note_draw_queue draw_queue;

    std::vector<note_state> note_states = {
        {.note_ref = note_data{note_type::tap, 0, 0, 0}, .target_ms = 1000.0, .end_target_ms = 1000.0},
        {.note_ref = note_data{note_type::tap, 0, 0, 0}, .target_ms = 1200.0, .end_target_ms = 1200.0},
        {.note_ref = note_data{note_type::hold, 0, 1, 480}, .target_ms = 1300.0, .end_target_ms = 1500.0},
        {.note_ref = note_data{note_type::tap, 0, 1, 0}, .target_ms = 900.0, .end_target_ms = 900.0},
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

    note_states[2].progress = note_progress_state::holding;
    draw_queue.update_visible_window(note_states, 0.1f, 10.0f, 15.0f, 40.0f, 2000.0);
    if (draw_queue.active_indices_for_lane(1).size() != 1) {
        std::cerr << "Holding note should remain active\n";
        return EXIT_FAILURE;
    }

    note_states[2].progress = note_progress_state::pending;
    draw_queue.update_visible_window(note_states, 0.1f, 10.0f, 15.0f, 40.0f, 2000.0);
    if (draw_queue.active_indices_for_lane(1).size() != 1) {
        std::cerr << "Unfinished hold should remain active after passing the judge line\n";
        return EXIT_FAILURE;
    }

    note_states[2].progress = note_progress_state::holding;
    draw_queue.update_visible_window(note_states, 0.1f, 10.0f, 15.0f, 40.0f, 2000.0);
    if (draw_queue.active_indices_for_lane(1).size() != 1) {
        std::cerr << "Head-judged hold should remain active until completion\n";
        return EXIT_FAILURE;
    }

    note_states[2].progress = note_progress_state::completed;
    draw_queue.update_visible_window(note_states, 0.1f, 10.0f, 15.0f, 40.0f, 2000.0);
    if (!draw_queue.active_indices_for_lane(1).empty()) {
        std::cerr << "Finished hold note should be removed\n";
        return EXIT_FAILURE;
    }

    std::cout << "play_note_draw_queue smoke test passed\n";
    return EXIT_SUCCESS;
}
