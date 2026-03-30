#include <array>
#include <cstdlib>
#include <iostream>

#include "input_handler.h"

int main() {
    input_handler handler;

    handler.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 0.0);
    handler.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 123.0);
    if (!handler.is_lane_just_pressed(0) || !handler.is_lane_held(0) || handler.is_lane_just_released(0)) {
        std::cerr << "4-key press detection failed\n";
        return EXIT_FAILURE;
    }
    const std::span<const input_event> press_events = handler.events();
    if (press_events.size() != 1 || press_events[0].type != input_event_type::press ||
        press_events[0].lane != 0 || press_events[0].timestamp_ms != 123.0) {
        std::cerr << "Press event capture failed\n";
        return EXIT_FAILURE;
    }

    handler.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 140.0);
    if (handler.is_lane_just_pressed(0) || !handler.is_lane_held(0) || handler.is_lane_just_released(0)) {
        std::cerr << "Hold detection failed\n";
        return EXIT_FAILURE;
    }
    if (!handler.events().empty()) {
        std::cerr << "Hold should not emit events\n";
        return EXIT_FAILURE;
    }

    handler.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 222.0);
    if (handler.is_lane_just_pressed(0) || handler.is_lane_held(0) || !handler.is_lane_just_released(0)) {
        std::cerr << "Release detection failed\n";
        return EXIT_FAILURE;
    }
    const std::span<const input_event> release_events = handler.events();
    if (release_events.size() != 1 || release_events[0].type != input_event_type::release ||
        release_events[0].lane != 0 || release_events[0].timestamp_ms != 222.0) {
        std::cerr << "Release event capture failed\n";
        return EXIT_FAILURE;
    }

    handler.set_key_count(6);
    handler.update_from_lane_states(std::array<bool, 6>{false, false, false, false, false, true}, 300.0);
    if (!handler.is_lane_just_pressed(5) || !handler.is_lane_held(5) || handler.is_lane_just_released(5)) {
        std::cerr << "6-key mode detection failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "input_handler smoke test passed\n";
    return EXIT_SUCCESS;
}
