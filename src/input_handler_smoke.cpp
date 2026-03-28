#include <array>
#include <cstdlib>
#include <iostream>

#include "input_handler.h"

int main() {
    input_handler handler;

    handler.update_from_lane_states(std::array<bool, 4>{false, false, false, false});
    handler.update_from_lane_states(std::array<bool, 4>{true, false, false, false});
    if (!handler.is_lane_just_pressed(0) || !handler.is_lane_held(0) || handler.is_lane_just_released(0)) {
        std::cerr << "4-key press detection failed\n";
        return EXIT_FAILURE;
    }

    handler.update_from_lane_states(std::array<bool, 4>{true, false, false, false});
    if (handler.is_lane_just_pressed(0) || !handler.is_lane_held(0) || handler.is_lane_just_released(0)) {
        std::cerr << "Hold detection failed\n";
        return EXIT_FAILURE;
    }

    handler.update_from_lane_states(std::array<bool, 4>{false, false, false, false});
    if (handler.is_lane_just_pressed(0) || handler.is_lane_held(0) || !handler.is_lane_just_released(0)) {
        std::cerr << "Release detection failed\n";
        return EXIT_FAILURE;
    }

    handler.set_key_count(6);
    handler.update_from_lane_states(std::array<bool, 6>{false, false, false, false, false, true});
    if (!handler.is_lane_just_pressed(5) || !handler.is_lane_held(5) || handler.is_lane_just_released(5)) {
        std::cerr << "6-key mode detection failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "input_handler smoke test passed\n";
    return EXIT_SUCCESS;
}
