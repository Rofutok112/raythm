#include <cstdlib>
#include <iostream>
#include <vector>

#include "performance_system.h"

namespace {

chart_data make_chart() {
    chart_data data;
    data.meta.chart_id = "performance";
    data.meta.key_count = 4;
    data.meta.difficulty = "Performance";
    data.meta.level = 6.0f;
    data.meta.resolution = 480;
    data.meta.format_version = 1;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };

    data.notes = {
        {note_type::tap, 0, 0, 0},
        {note_type::tap, 960, 1, 0},
        {note_type::tap, 1920, 2, 0},
    };

    const int burst_lanes[] = {0, 3, 1, 2, 0, 3, 1, 2, 0, 3, 1, 2};
    for (int i = 0; i < 12; ++i) {
        data.notes.push_back({note_type::tap, 2880 + i * 60, burst_lanes[i], 0});
    }
    return data;
}

chart_data make_stay_weight_chart() {
    chart_data data;
    data.meta.chart_id = "performance-stay";
    data.meta.key_count = 4;
    data.meta.difficulty = "Performance";
    data.meta.level = 6.0f;
    data.meta.resolution = 480;
    data.meta.format_version = 1;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    data.notes = {
        {note_type::tap, 0, 0, 0},
        {note_type::stay, 480, 1, 0},
        {note_type::tap, 960, 2, 0},
    };
    return data;
}

judge_event perfect_event(int event_index) {
    return judge_event{
        .result = judge_result::perfect,
        .offset_ms = 0.0,
        .lane = 0,
        .play_hitsound = true,
        .apply_gameplay_effects = true,
        .show_feedback = true,
        .event_index = event_index,
    };
}

}  // namespace

int main() {
    const chart_data chart = make_chart();
    timing_engine engine;
    engine.init(chart.timing_events, chart.meta.resolution, chart.meta.offset);

    performance_system performance;
    performance.init(chart, engine);

    if (performance.max_rc() <= 0.0f) {
        std::cerr << "Expected chart max RC to be positive\n";
        return EXIT_FAILURE;
    }

    performance.on_judge(perfect_event(0));
    const float first_delta = performance.current_rc();
    performance.on_judge(perfect_event(1));
    performance.on_judge(perfect_event(2));
    const float before_burst = performance.current_rc();
    performance.on_judge(perfect_event(3));
    const float burst_delta = performance.current_rc() - before_burst;

    if (first_delta <= 0.0f || burst_delta <= first_delta * 1.5f) {
        std::cerr << "Expected burst event to award substantially more live RC than an easy opener: "
                  << burst_delta << " <= " << first_delta << "\n";
        return EXIT_FAILURE;
    }

    performance.on_judge(judge_event{
        .result = judge_result::miss,
        .offset_ms = 200.0,
        .lane = 1,
        .play_hitsound = false,
        .apply_gameplay_effects = true,
        .show_feedback = true,
        .event_index = 4,
    });
    const float after_miss = performance.current_rc();
    performance.on_judge(perfect_event(5));
    if (performance.current_rc() <= after_miss) {
        std::cerr << "Expected later clean hits to keep increasing live RC after a miss\n";
        return EXIT_FAILURE;
    }

    const chart_data stay_chart = make_stay_weight_chart();
    timing_engine stay_engine;
    stay_engine.init(stay_chart.timing_events, stay_chart.meta.resolution, stay_chart.meta.offset);
    performance_system stay_performance;
    stay_performance.init(stay_chart, stay_engine);
    stay_performance.on_judge(perfect_event(0));
    const float before_stay = stay_performance.current_rc();
    stay_performance.on_judge(perfect_event(1));
    if (stay_performance.current_rc() != before_stay) {
        std::cerr << "Expected stay note to have zero live RC weight\n";
        return EXIT_FAILURE;
    }
    stay_performance.on_judge(perfect_event(2));
    if (stay_performance.current_rc() <= before_stay) {
        std::cerr << "Expected later non-stay note to keep increasing live RC\n";
        return EXIT_FAILURE;
    }

    std::cout << "performance_system smoke test passed\n";
    return EXIT_SUCCESS;
}
