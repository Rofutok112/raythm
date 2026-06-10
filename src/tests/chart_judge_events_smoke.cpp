#include <cstdlib>
#include <iostream>
#include <vector>

#include "chart_judge_events.h"

namespace {

bool expect_event(const chart_judge_event& event,
                  int event_index,
                  chart_judge_event_role role,
                  int tick,
                  int lane,
                  int lane_width) {
    return event.event_index == event_index &&
           event.role == role &&
           event.tick == tick &&
           event.lane == lane &&
           event.lane_width == lane_width;
}

}  // namespace

int main() {
    timing_engine engine;
    engine.init({
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    }, 480, 0);

    note_data wide_tap{note_type::tap, 0, 0, 0};
    wide_tap.lane_width = 2;
    note_data hold{note_type::hold, 480, 1, 960};
    hold.lane_width = 2;
    note_data release{note_type::release, 960, 1, 960};
    release.lane_width = 2;
    note_data stay{note_type::stay, 960, 0, 960};
    stay.lane_width = 4;
    note_data ray_hold{note_type::hold, 1440, 3, 1920};
    ray_hold.is_ray = true;
    note_data decorative_hold{note_type::decorative_hold, 480, 0, 1920};
    decorative_hold.lane_width = 2;

    chart_data chart;
    chart.notes = {wide_tap, hold, release, stay, ray_hold, decorative_hold};

    const std::vector<chart_judge_event> events = chart_judge_events::build(chart, engine);
    if (events.size() != 7 || chart_judge_events::count(chart, engine) != 7) {
        std::cerr << "Judge event count should treat long notes as head + tail\n";
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].event_index != static_cast<int>(i)) {
            std::cerr << "Judge event indices should be contiguous and stable\n";
            return EXIT_FAILURE;
        }
    }

    if (!expect_event(events[0], 0, chart_judge_event_role::tap, 0, 0, 2) ||
        !expect_event(events[1], 1, chart_judge_event_role::hold_head, 480, 1, 2) ||
        !expect_event(events[2], 2, chart_judge_event_role::hold_tail, 960, 1, 2) ||
        !expect_event(events[3], 3, chart_judge_event_role::release, 960, 1, 2) ||
        !expect_event(events[4], 4, chart_judge_event_role::stay, 960, 0, 4) ||
        !expect_event(events[5], 5, chart_judge_event_role::hold_head, 1440, 3, 1) ||
        !expect_event(events[6], 6, chart_judge_event_role::hold_tail, 1920, 3, 1)) {
        std::cerr << "Judge events should preserve note roles, timing, and lane spans\n";
        for (const chart_judge_event& event : events) {
            std::cerr << "  index=" << event.event_index
                      << " role=" << static_cast<int>(event.role)
                      << " tick=" << event.tick
                      << " lane=" << event.lane
                      << " width=" << event.lane_width << "\n";
        }
        return EXIT_FAILURE;
    }

    if (!events[5].is_ray || !events[6].is_ray) {
        std::cerr << "Ray long notes should carry ray identity on both judge events\n";
        return EXIT_FAILURE;
    }

    std::cout << "chart_judge_events smoke test passed\n";
    return EXIT_SUCCESS;
}
