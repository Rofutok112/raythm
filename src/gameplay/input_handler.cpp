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
    prev_state_.fill(false);
    curr_state_.fill(false);
    events_.clear();
}

void input_handler::update(double timestamp_ms) {
    const std::span<const KeyboardKey> lane_keys = key_config_.get_lane_keys(key_count_);
    events_.clear();
    prev_state_ = curr_state_;

    if (windows_input_source::instance().is_available()) {
        const std::vector<native_key_event> native_events = windows_input_source::instance().drain_events();
        if (!native_events.empty()) {
            apply_native_events(native_events);
            return;
        }
    }

    std::array<bool, kMaxLanes> next_state = {};
    for (int lane = 0; lane < key_count_; ++lane) {
        const KeyboardKey key = lane_keys[static_cast<size_t>(lane)];
        if (IsKeyPressed(key)) {
            events_.push_back({input_event_type::press, lane, timestamp_ms});
        }
        if (IsKeyReleased(key)) {
            events_.push_back({input_event_type::release, lane, timestamp_ms});
        }
        next_state[static_cast<size_t>(lane)] = IsKeyDown(key);
    }

    curr_state_ = next_state;
}

void input_handler::update_from_lane_states(std::span<const bool> lane_states, double timestamp_ms) {
    if (lane_states.size() != static_cast<size_t>(key_count_)) {
        throw std::invalid_argument("lane_states size must match key_count");
    }

    std::array<bool, kMaxLanes> next_state = {};
    std::copy(lane_states.begin(), lane_states.end(), next_state.begin());
    events_.clear();
    prev_state_ = curr_state_;

    for (int lane = 0; lane < key_count_; ++lane) {
        const size_t index = static_cast<size_t>(lane);
        if (next_state[index] && !prev_state_[index]) {
            events_.push_back({input_event_type::press, lane, timestamp_ms});
        } else if (!next_state[index] && prev_state_[index]) {
            events_.push_back({input_event_type::release, lane, timestamp_ms});
        }
    }

    curr_state_ = next_state;
}

std::span<const input_event> input_handler::events() const {
    return std::span<const input_event>(events_);
}

bool input_handler::is_lane_just_pressed(int lane) const {
    if (lane < 0 || lane >= key_count_) {
        return false;
    }

    const size_t index = static_cast<size_t>(lane);
    return curr_state_[index] && !prev_state_[index];
}

bool input_handler::is_lane_held(int lane) const {
    if (lane < 0 || lane >= key_count_) {
        return false;
    }

    return curr_state_[static_cast<size_t>(lane)];
}

bool input_handler::is_lane_just_released(int lane) const {
    if (lane < 0 || lane >= key_count_) {
        return false;
    }

    const size_t index = static_cast<size_t>(lane);
    return !curr_state_[index] && prev_state_[index];
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

void input_handler::apply_native_events(std::span<const native_key_event> native_events) {
    for (const native_key_event& native_event : native_events) {
        const int lane = find_lane_for_key(native_event.key);
        if (lane < 0) {
            continue;
        }

        const size_t index = static_cast<size_t>(lane);
        const bool next_pressed = native_event.type == input_event_type::press;
        if (curr_state_[index] == next_pressed) {
            continue;
        }

        curr_state_[index] = next_pressed;
        events_.push_back({native_event.type, lane, native_event.timestamp_ms});
    }
}
