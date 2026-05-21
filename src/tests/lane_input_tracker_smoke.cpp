#include <array>
#include <cassert>
#include <iostream>

#include "lane_input_tracker.h"

int main() {
    lane_input_tracker tracker;
    tracker.set_key_count(4);

    tracker.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 0.0);
    tracker.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 12.0);
    assert(tracker.is_lane_just_pressed(0));
    assert(tracker.is_lane_held(0));
    assert(!tracker.is_lane_just_released(0));
    assert(tracker.events().size() == 1);
    assert(tracker.events()[0].type == input_event_type::press);
    assert(tracker.events()[0].timestamp_ms == 12.0);

    tracker.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 16.0);
    assert(!tracker.is_lane_just_pressed(0));
    assert(tracker.is_lane_held(0));
    assert(tracker.events().empty());

    tracker.update_from_lane_states(std::array<bool, 4>{false, false, false, false}, 20.0);
    assert(tracker.is_lane_just_released(0));
    assert(tracker.events().size() == 1);
    assert(tracker.events()[0].type == input_event_type::release);

    tracker.set_key_count(6);
    tracker.begin_frame();
    assert(tracker.apply_lane_event(input_event_type::press, 5, 30.0));
    assert(!tracker.apply_lane_event(input_event_type::press, 5, 31.0));
    assert(tracker.is_lane_held(5));
    assert(tracker.events().size() == 1);

    std::cout << "lane_input_tracker smoke test passed\n";
    return 0;
}
