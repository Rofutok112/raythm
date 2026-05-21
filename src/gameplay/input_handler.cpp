#include "input_handler.h"

#include <algorithm>
#include <stdexcept>

std::span<const KeyboardKey> key_config::get_lane_keys(int key_count) const {
    if (key_count == 4) {
        return std::span<const KeyboardKey>(keys_4);
    }

    if (key_count == 6) {
        return std::span<const KeyboardKey>(keys_6);
    }

    throw std::invalid_argument("key_count must be 4 or 6");
}

input_handler::input_handler(key_config config) : key_config_(config) {
}

void input_handler::set_key_count(int key_count) {
    if (key_count != 4 && key_count != 6) {
        throw std::invalid_argument("key_count must be 4 or 6");
    }

    key_count_ = key_count;
    tracker_.set_key_count(key_count);
    last_update_source_ = input_update_source::polling;
    last_update_event_count_ = 0;
}

void input_handler::update(double input_event_time_ms) {
    tracker_.begin_frame();

    if (windows_input_source::instance().is_available()) {
        const std::vector<native_key_event> native_events = windows_input_source::instance().drain_events();
        if (!native_events.empty()) {
            apply_native_events(native_events, input_event_time_ms);
        }
        last_update_source_ = input_update_source::native_windows;
        last_update_event_count_ = tracker_.last_update_event_count();
        return;
    }

    if (!IsWindowReady()) {
        last_update_source_ = input_update_source::polling;
        last_update_event_count_ = 0;
        return;
    }

    const std::span<const KeyboardKey> lane_keys = key_config_.get_lane_keys(key_count_);
    std::array<bool, lane_input_tracker::kMaxLanes> next_state = {};
    for (int lane = 0; lane < key_count_; ++lane) {
        const KeyboardKey key = lane_keys[static_cast<size_t>(lane)];
        next_state[static_cast<size_t>(lane)] = IsKeyDown(key);
    }

    tracker_.update_from_lane_states(std::span<const bool>(next_state.data(), static_cast<size_t>(key_count_)),
                                     input_event_time_ms);
    last_update_source_ = input_update_source::polling;
    last_update_event_count_ = tracker_.last_update_event_count();
}

void input_handler::update_from_lane_states(std::span<const bool> lane_states, double input_event_time_ms) {
    tracker_.update_from_lane_states(lane_states, input_event_time_ms);
    last_update_source_ = input_update_source::simulated;
    last_update_event_count_ = tracker_.last_update_event_count();
}

std::span<const input_event> input_handler::events() const {
    return tracker_.events();
}

input_update_source input_handler::last_update_source() const {
    return last_update_source_;
}

int input_handler::last_update_event_count() const {
    return last_update_event_count_;
}

bool input_handler::is_lane_just_pressed(int lane) const {
    return tracker_.is_lane_just_pressed(lane);
}

bool input_handler::is_lane_held(int lane) const {
    return tracker_.is_lane_held(lane);
}

bool input_handler::is_lane_just_released(int lane) const {
    return tracker_.is_lane_just_released(lane);
}

int input_handler::find_lane_for_key(int key) const {
    const std::span<const KeyboardKey> lane_keys = key_config_.get_lane_keys(key_count_);
    for (int lane = 0; lane < key_count_; ++lane) {
        if (static_cast<int>(lane_keys[static_cast<size_t>(lane)]) == key) {
            return lane;
        }
    }
    return -1;
}

void input_handler::apply_native_events(std::span<const native_key_event> native_events, double input_event_time_ms) {
    const double native_now_ms = windows_input_source::instance().current_time_ms();
    const double audio_offset_ms = input_event_time_ms - native_now_ms;

    for (const native_key_event& native_event : native_events) {
        const int lane = find_lane_for_key(native_event.key);
        if (lane < 0) {
            continue;
        }

        tracker_.apply_lane_event(native_event.type, lane, native_event.timestamp_ms + audio_offset_ms);
    }
}
