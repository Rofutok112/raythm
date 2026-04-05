#include <cstdlib>
#include <iostream>

#include "play/play_chart_filter.h"

namespace {

chart_data make_chart() {
    chart_data chart;
    chart.meta.chart_id = "play-chart-filter-smoke";
    chart.meta.song_id = "song";
    chart.meta.key_count = 4;
    chart.meta.difficulty = "Normal";
    chart.meta.chart_author = "Codex";
    chart.meta.format_version = 1;
    chart.meta.resolution = 480;
    chart.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
    };
    chart.notes = {
        {note_type::tap, 240, 0, 240},
        {note_type::hold, 480, 1, 960},
        {note_type::tap, 960, 2, 960},
    };
    return chart;
}

}

int main() {
    const chart_data source = make_chart();

    const chart_data unchanged = play_chart_filter::prepare_chart_for_playback(source, 0);
    if (unchanged.notes.size() != 3) {
        std::cerr << "Start tick 0 should keep all notes\n";
        return EXIT_FAILURE;
    }

    const chart_data filtered = play_chart_filter::prepare_chart_for_playback(source, 600);
    if (filtered.notes.size() != 1 || filtered.notes.front().tick != 960 || filtered.notes.front().lane != 2) {
        std::cerr << "Filtering should discard notes before the playtest start tick\n";
        return EXIT_FAILURE;
    }

    if (filtered.timing_events.size() != source.timing_events.size() ||
        filtered.meta.chart_id != source.meta.chart_id) {
        std::cerr << "Filtering should preserve chart metadata and timing events\n";
        return EXIT_FAILURE;
    }

    std::cout << "play_chart_filter smoke test passed\n";
    return EXIT_SUCCESS;
}
