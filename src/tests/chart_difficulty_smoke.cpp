#include <cstdlib>
#include <iostream>

#include "chart_difficulty.h"

namespace {

chart_data make_easy_chart() {
    chart_data data;
    data.meta.chart_id = "easy";
    data.meta.key_count = 4;
    data.meta.difficulty = "Easy";
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

chart_data make_hard_chart() {
    chart_data data = make_easy_chart();
    data.meta.chart_id = "hard";
    data.meta.difficulty = "Hard";
    data.notes = {
        {note_type::tap, 0, 0, 0},
        {note_type::tap, 120, 3, 0},
        {note_type::hold, 240, 1, 720},
        {note_type::tap, 300, 2, 0},
        {note_type::tap, 360, 0, 0},
        {note_type::tap, 420, 3, 0},
        {note_type::hold, 480, 2, 960},
        {note_type::tap, 540, 1, 0},
        {note_type::tap, 600, 3, 0},
        {note_type::tap, 660, 0, 0},
        {note_type::tap, 720, 2, 0},
        {note_type::tap, 780, 1, 0},
        {note_type::tap, 840, 3, 0},
        {note_type::tap, 900, 0, 0},
        {note_type::hold, 960, 0, 1440},
        {note_type::tap, 1020, 3, 0},
        {note_type::tap, 1080, 2, 0},
        {note_type::tap, 1140, 1, 0},
        {note_type::tap, 1200, 0, 0},
        {note_type::tap, 1260, 2, 0},
        {note_type::tap, 1320, 3, 0},
        {note_type::tap, 1380, 1, 0},
        {note_type::tap, 1440, 0, 0},
    };
    return data;
}

}  // namespace

int main() {
    const chart_data easy = make_easy_chart();
    const chart_data hard = make_hard_chart();

    const int easy_level = chart_difficulty::calculate_level(easy);
    const int hard_level = chart_difficulty::calculate_level(hard);

    if (easy_level <= 0) {
        std::cerr << "Expected easy chart to receive a positive auto level\n";
        return EXIT_FAILURE;
    }
    if (hard_level <= easy_level) {
        std::cerr << "Expected hard chart level to exceed easy chart level\n";
        return EXIT_FAILURE;
    }

    std::cout << "chart_difficulty smoke test passed\n";
    return EXIT_SUCCESS;
}
