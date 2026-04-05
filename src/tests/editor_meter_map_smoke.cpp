#include <cstdlib>
#include <iostream>

#include "editor/editor_meter_map.h"

namespace {

chart_data make_chart_with_mid_measure_meter_resume() {
    chart_data data;
    data.meta.chart_id = "editor-meter-map-smoke";
    data.meta.key_count = 4;
    data.meta.difficulty = "Normal";
    data.meta.level = 5;
    data.meta.chart_author = "Codex";
    data.meta.format_version = 1;
    data.meta.resolution = 480;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
        {timing_event_type::meter, 5760, 0.0f, 6, 4},
        {timing_event_type::meter, 9600, 0.0f, 4, 4},
    };
    return data;
}

chart_data make_chart_without_meter_detour() {
    chart_data data = make_chart_with_mid_measure_meter_resume();
    data.timing_events[2] = {timing_event_type::meter, 5760, 0.0f, 4, 4};
    return data;
}

bool expect_position(const editor_meter_map::bar_beat_position& actual, int measure, int beat, const char* label) {
    if (actual.measure == measure && actual.beat == beat) {
        return true;
    }
    std::cerr << label << " expected " << measure << ":" << beat
              << " but got " << actual.measure << ":" << actual.beat << '\n';
    return false;
}

}  // namespace

int main() {
    bool ok = true;

    {
        editor_meter_map meter_map;
        meter_map.rebuild(make_chart_with_mid_measure_meter_resume());

        ok &= expect_position(meter_map.bar_beat_at_tick(5760), 4, 1, "6/4 segment start");
        ok &= expect_position(meter_map.bar_beat_at_tick(9600), 5, 3, "resumed 4/4 segment start");
        if (meter_map.tick_from_bar_beat(5, 3) != 9600) {
            std::cerr << "tick_from_bar_beat should resolve resumed segment start to tick 9600\n";
            ok = false;
        }
        if (meter_map.tick_from_bar_beat(6, 1) != 10560) {
            std::cerr << "tick_from_bar_beat should keep later measures aligned after resumed segment\n";
            ok = false;
        }
    }

    {
        editor_meter_map meter_map;
        meter_map.rebuild(make_chart_without_meter_detour());

        ok &= expect_position(meter_map.bar_beat_at_tick(9600), 6, 1, "restored 4/4 segment start");
        if (meter_map.tick_from_bar_beat(6, 1) != 9600) {
            std::cerr << "restored 4/4 layout should map 6:1 back to tick 9600\n";
            ok = false;
        }
    }

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "editor_meter_map smoke test passed\n";
    return EXIT_SUCCESS;
}
