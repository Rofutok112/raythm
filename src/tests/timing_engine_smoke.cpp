#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "timing_engine.h"

namespace {
bool nearly_equal(double left, double right) {
    return std::fabs(left - right) < 0.001;
}
}

int main() {
    timing_engine engine;

    std::vector<timing_event> events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
        {timing_event_type::bpm, 480, 240.0f, 4, 4},
        {timing_event_type::bpm, 960, 60.0f, 4, 4},
    };

    engine.init(events, 480);

    if (!nearly_equal(engine.tick_to_ms(0), 0.0)) {
        std::cerr << "tick_to_ms at 0 failed\n";
        return EXIT_FAILURE;
    }

    if (!nearly_equal(engine.tick_to_ms(480), 500.0)) {
        std::cerr << "tick_to_ms at first bpm change failed\n";
        return EXIT_FAILURE;
    }

    if (!nearly_equal(engine.tick_to_ms(960), 750.0)) {
        std::cerr << "tick_to_ms at second bpm change failed\n";
        return EXIT_FAILURE;
    }

    if (!nearly_equal(engine.tick_to_ms(1440), 1750.0)) {
        std::cerr << "tick_to_ms after second bpm change failed\n";
        return EXIT_FAILURE;
    }

    if (engine.ms_to_tick(500.0) != 480) {
        std::cerr << "ms_to_tick at first bpm change failed\n";
        return EXIT_FAILURE;
    }

    if (engine.ms_to_tick(750.0) != 960) {
        std::cerr << "ms_to_tick at second bpm change failed\n";
        return EXIT_FAILURE;
    }

    if (engine.ms_to_tick(1750.0) != 1440) {
        std::cerr << "ms_to_tick after second bpm change failed\n";
        return EXIT_FAILURE;
    }

    engine.init(events, 480, 120);

    if (!nearly_equal(engine.tick_to_ms(0), 120.0)) {
        std::cerr << "tick_to_ms with offset failed\n";
        return EXIT_FAILURE;
    }

    if (!nearly_equal(engine.tick_to_ms(480), 620.0)) {
        std::cerr << "tick_to_ms at first bpm change with offset failed\n";
        return EXIT_FAILURE;
    }

    if (engine.ms_to_tick(120.0) != 0) {
        std::cerr << "ms_to_tick at offset origin failed\n";
        return EXIT_FAILURE;
    }

    if (engine.ms_to_tick(0.0) != -115) {
        std::cerr << "ms_to_tick before offset failed\n";
        return EXIT_FAILURE;
    }

    if (engine.get_bpm_at(0) != 120.0f || engine.get_bpm_at(479) != 120.0f) {
        std::cerr << "get_bpm_at before change failed\n";
        return EXIT_FAILURE;
    }

    if (engine.get_bpm_at(480) != 240.0f || engine.get_bpm_at(959) != 240.0f) {
        std::cerr << "get_bpm_at first change failed\n";
        return EXIT_FAILURE;
    }

    if (engine.get_bpm_at(960) != 60.0f) {
        std::cerr << "get_bpm_at second change failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "timing_engine smoke test passed\n";
    return EXIT_SUCCESS;
}
