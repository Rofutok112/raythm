#include <array>
#include <cstdlib>
#include <iostream>

#include "input_handler.h"
#include "platform/windows_input_source.h"

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

    windows_input_source::instance().enable_test_mode();
    handler = input_handler();
    handler.set_key_count(4);
    windows_input_source::instance().set_test_current_time_ms(30.0);
    windows_input_source::instance().push_test_event({KEY_D, input_event_type::press, 10.0, 1});
    windows_input_source::instance().push_test_event({KEY_F, input_event_type::press, 10.5, 2});
    windows_input_source::instance().push_test_event({KEY_D, input_event_type::release, 20.0, 3});
    handler.update(530.0);

    const std::span<const input_event> native_events = handler.events();
    if (native_events.size() != 3 || native_events[0].lane != 0 || native_events[1].lane != 1 ||
        native_events[2].type != input_event_type::release || native_events[2].lane != 0) {
        std::cerr << "Native event ordering failed\n";
        return EXIT_FAILURE;
    }

    if (native_events[0].timestamp_ms != 510.0 || native_events[1].timestamp_ms != 510.5 ||
        native_events[2].timestamp_ms != 520.0) {
        std::cerr << "Native event audio-time conversion failed\n";
        return EXIT_FAILURE;
    }

    if (handler.is_lane_held(0) || !handler.is_lane_held(1) || handler.is_lane_just_pressed(0)) {
        std::cerr << "Native event state tracking failed\n";
        return EXIT_FAILURE;
    }

    handler.set_key_count(6);
    windows_input_source::instance().set_test_current_time_ms(40.0);
    windows_input_source::instance().push_test_event({KEY_L, input_event_type::press, 30.0, 4});
    handler.update(640.0);
    if (handler.events().size() != 1 || handler.events()[0].lane != 5 || !handler.is_lane_held(5) ||
        handler.events()[0].timestamp_ms != 630.0) {
        std::cerr << "Native 6-key mapping failed\n";
        return EXIT_FAILURE;
    }

    handler = input_handler();
    handler.set_key_count(4);
    windows_input_source::instance().set_test_current_time_ms(110.0);
    windows_input_source::instance().push_test_event({KEY_D, input_event_type::press, 100.0, 5});
    windows_input_source::instance().push_test_event({KEY_F, input_event_type::press, 100.0, 6});
    handler.update(610.0);
    if (handler.events().size() != 2 || handler.events()[0].lane != 0 || handler.events()[1].lane != 1 ||
        handler.events()[0].timestamp_ms != 600.0 || handler.events()[1].timestamp_ms != 600.0) {
        std::cerr << "Native simultaneous conversion failed\n";
        return EXIT_FAILURE;
    }
    if (handler.last_update_source() != input_update_source::native_windows ||
        handler.last_update_event_count() != 2) {
        std::cerr << "Input source metadata failed\n";
        return EXIT_FAILURE;
    }

    windows_input_source::instance().shutdown();

    std::cout << "input_handler smoke test passed\n";
    return EXIT_SUCCESS;
}
