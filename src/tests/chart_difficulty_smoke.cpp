#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

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

chart_data make_chart(std::string chart_id, std::vector<note_data> notes) {
    chart_data data;
    data.meta.chart_id = std::move(chart_id);
    data.meta.key_count = 4;
    data.meta.difficulty = "Pattern";
    data.meta.resolution = 480;
    data.meta.format_version = 1;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    data.notes = std::move(notes);
    return data;
}

chart_data make_hold_conflict_chart(bool same_hand) {
    std::vector<note_data> notes = {
        {note_type::hold, 0, 0, 2160},
    };
    const int tap_lane = same_hand ? 1 : 3;
    for (int tick = 120; tick <= 2040; tick += 120) {
        notes.push_back({note_type::tap, tick, tap_lane, 0});
    }
    return make_chart(same_hand ? "same_hand_hold" : "opposite_hand_hold", std::move(notes));
}

chart_data make_chord_shape_chart() {
    std::vector<note_data> notes;
    for (int tick = 0; tick < 3840; tick += 240) {
        notes.push_back({note_type::tap, tick, 0, 0});
        notes.push_back({note_type::tap, tick, 3, 0});
    }
    return make_chart("wide_chords", std::move(notes));
}

chart_data make_split_shape_chart() {
    std::vector<note_data> notes;
    const int lanes[] = {0, 3};
    for (int i = 0; i < 32; ++i) {
        notes.push_back({note_type::tap, i * 120, lanes[i % 2], 0});
    }
    return make_chart("split_taps", std::move(notes));
}

chart_data make_one_hand_stream_chart() {
    std::vector<note_data> notes;
    const int lanes[] = {0, 1};
    for (int i = 0; i < 32; ++i) {
        notes.push_back({note_type::tap, i * 120, lanes[i % 2], 0});
    }
    return make_chart("one_hand_stream", std::move(notes));
}

chart_data make_balanced_stream_chart() {
    std::vector<note_data> notes;
    const int lanes[] = {0, 2, 1, 3};
    for (int i = 0; i < 32; ++i) {
        notes.push_back({note_type::tap, i * 120, lanes[i % 4], 0});
    }
    return make_chart("balanced_stream", std::move(notes));
}

bool approx(float actual, float expected, float tolerance = 0.05f) {
    return std::fabs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    const chart_data easy = make_easy_chart();
    const chart_data hard = make_hard_chart();

    const float easy_level = chart_difficulty::calculate_level(easy);
    const float hard_level = chart_difficulty::calculate_level(hard);

    if (easy_level <= 0) {
        std::cerr << "Expected easy chart to receive a positive auto level\n";
        return EXIT_FAILURE;
    }
    if (hard_level <= easy_level) {
        std::cerr << "Expected hard chart level to exceed easy chart level\n";
        return EXIT_FAILURE;
    }

    const float normal_sample = chart_difficulty::level_from_rating(3837.7f);
    const float hard_sample = chart_difficulty::level_from_rating(18741.8f);
    const float prism_sample = chart_difficulty::level_from_rating(44933.6f);
    const float ray_sample = chart_difficulty::level_from_rating(130846.7f);

    if (!approx(normal_sample, 4.3f) ||
        !approx(hard_sample, 6.3f) ||
        !approx(prism_sample, 7.5f) ||
        !approx(ray_sample, 8.9f)) {
        std::cerr << "Expected known raw ratings to map into the calibrated display range\n";
        return EXIT_FAILURE;
    }
    if (ray_sample - normal_sample < 4.0f) {
        std::cerr << "Expected display levels to keep enough separation across known charts\n";
        return EXIT_FAILURE;
    }
    if (chart_difficulty::level_from_rating(100.0f) >= chart_difficulty::level_from_rating(1000.0f) ||
        chart_difficulty::level_from_rating(1000.0f) >= chart_difficulty::level_from_rating(10000.0f) ||
        chart_difficulty::level_from_rating(10000.0f) >= chart_difficulty::level_from_rating(100000.0f)) {
        std::cerr << "Expected calibrated display levels to stay monotonic across rating decades\n";
        return EXIT_FAILURE;
    }

    chart_data legacy_level = hard;
    legacy_level.meta.level = 55.0f;
    chart_difficulty::apply_auto_level(legacy_level);
    if (legacy_level.meta.level == 55.0f ||
        !approx(legacy_level.meta.level, chart_difficulty::calculate_level(hard))) {
        std::cerr << "Expected legacy rchart level metadata to be replaced with auto level\n";
        return EXIT_FAILURE;
    }

    const float same_hand_hold = chart_difficulty::calculate_rating(make_hold_conflict_chart(true));
    const float opposite_hand_hold = chart_difficulty::calculate_rating(make_hold_conflict_chart(false));
    if (same_hand_hold <= opposite_hand_hold * 1.08f) {
        std::cerr << "Expected same-hand LN responsibility conflict to rate higher than opposite-hand handling: "
                  << same_hand_hold << " <= " << opposite_hand_hold << "\n";
        return EXIT_FAILURE;
    }

    const float wide_chords = chart_difficulty::calculate_rating(make_chord_shape_chart());
    const float split_taps = chart_difficulty::calculate_rating(make_split_shape_chart());
    if (wide_chords <= split_taps) {
        std::cerr << "Expected simultaneous chord shapes to rate higher than split taps with the same note count: "
                  << wide_chords << " <= " << split_taps << "\n";
        return EXIT_FAILURE;
    }

    const float one_hand_stream = chart_difficulty::calculate_rating(make_one_hand_stream_chart());
    const float balanced_stream = chart_difficulty::calculate_rating(make_balanced_stream_chart());
    if (one_hand_stream <= balanced_stream * 0.95f) {
        std::cerr << "Expected one-hand stream burden to stay visible against balanced streams: "
                  << one_hand_stream << " <= " << balanced_stream << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "chart_difficulty smoke test passed\n";
    return EXIT_SUCCESS;
}
