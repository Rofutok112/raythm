#include <cmath>
#include <cstdlib>
#include <iostream>

#include "chart_difficulty.h"
#include "chart_rc_calculator.h"

namespace {

bool approx(float actual, float expected, float tolerance = 0.05f) {
    return std::fabs(actual - expected) <= tolerance;
}

chart_data make_chart(float level) {
    chart_data data;
    data.meta.chart_id = "rc-calculator";
    data.meta.key_count = 4;
    data.meta.difficulty = "RC";
    data.meta.level = level;
    data.meta.resolution = 480;
    data.meta.format_version = 1;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    data.notes = {
        {note_type::tap, 0, 0, 0},
        {note_type::tap, 480, 1, 0},
        {note_type::tap, 960, 2, 0},
        {note_type::tap, 1440, 3, 0},
    };
    return data;
}

}  // namespace

int main() {
    const chart_data level_six = make_chart(6.0f);
    if (!approx(chart_rc::max_rc_for(level_six, 4), 600.0f, 0.001f)) {
        std::cerr << "Expected max RC to use a readable level x 100 scale\n";
        return EXIT_FAILURE;
    }

    if (chart_rc::max_rc_for(level_six, 0) != 0.0f) {
        std::cerr << "Expected charts with no weighted judge events to have zero max RC\n";
        return EXIT_FAILURE;
    }

    chart_data auto_level = make_chart(0.0f);
    const float calculated_level = chart_difficulty::calculate_level(auto_level);
    if (!approx(chart_rc::max_rc_for(auto_level, 4),
                std::round(calculated_level * 100.0f),
                0.001f)) {
        std::cerr << "Expected missing chart level to fall back to the current auto level calculation\n";
        return EXIT_FAILURE;
    }

    const chart_data level_twelve_point_five = make_chart(12.5f);
    if (!approx(chart_rc::max_rc_for(level_twelve_point_five, 4), 1250.0f, 0.001f)) {
        std::cerr << "Expected decimal levels to remain readable as integer RC values\n";
        return EXIT_FAILURE;
    }

    if (chart_rc::event_weight_for(0.0f) != 0.0f ||
        chart_rc::event_weight_for(-1.0f) != 0.0f ||
        chart_rc::event_weight_for(2.0f) <= chart_rc::event_weight_for(1.0f)) {
        std::cerr << "Expected RC event weights to ignore zero strain and stay monotonic\n";
        return EXIT_FAILURE;
    }

    std::cout << "chart_rc_calculator smoke test passed\n";
    return EXIT_SUCCESS;
}
