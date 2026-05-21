#include "lane_input_tracker.h"

#include <algorithm>
#include <stdexcept>

void lane_input_tracker::set_key_count(int key_count) {
    if (key_count != 4 && key_count != 6) {
        throw std::invalid_argument("key_count must be 4 or 6");
    }

    key_count_ = key_count;
    reset();
}

void lane_input_tracker::reset() {
    prev_state_.fill(false);
    curr_state_.fill(false);
    events_.clear();
}

void lane_input_tracker::begin_frame() {
    events_.clear();
    prev_state_ = curr_state_;
}

void lane_input_tracker::update_from_lane_states(std::span<const bool> lane_states,
                                                 double input_event_time_ms) {
    if (lane_states.size() != static_cast<size_t>(key_count_)) {
        throw std::invalid_argument("lane_states size must match key_count");
    }

    std::array<bool, kMaxLanes> next_state = {};
    std::copy(lane_states.begin(), lane_states.end(), next_state.begin());
    begin_frame();

    for (int lane = 0; lane < key_count_; ++lane) {
        const size_t index = static_cast<size_t>(lane);
        if (next_state[index] && !prev_state_[index]) {
            events_.push_back({input_event_type::press, lane, input_event_time_ms});
        } else if (!next_state[index] && prev_state_[index]) {
            events_.push_back({input_event_type::release, lane, input_event_time_ms});
        }
    }

    curr_state_ = next_state;
}

bool lane_input_tracker::apply_lane_event(input_event_type type, int lane, double input_event_time_ms) {
    if (lane < 0 || lane >= key_count_) {
        return false;
    }

    const size_t index = static_cast<size_t>(lane);
    const bool next_pressed = type == input_event_type::press;
    if (curr_state_[index] == next_pressed) {
        return false;
    }

    curr_state_[index] = next_pressed;
    events_.push_back({type, lane, input_event_time_ms});
    return true;
}

std::span<const input_event> lane_input_tracker::events() const {
    return std::span<const input_event>(events_);
}

int lane_input_tracker::last_update_event_count() const {
    return static_cast<int>(events_.size());
}

bool lane_input_tracker::is_lane_just_pressed(int lane) const {
    if (lane < 0 || lane >= key_count_) {
        return false;
    }

    const size_t index = static_cast<size_t>(lane);
    return curr_state_[index] && !prev_state_[index];
}

bool lane_input_tracker::is_lane_held(int lane) const {
    if (lane < 0 || lane >= key_count_) {
        return false;
    }

    return curr_state_[static_cast<size_t>(lane)];
}

bool lane_input_tracker::is_lane_just_released(int lane) const {
    if (lane < 0 || lane >= key_count_) {
        return false;
    }

    const size_t index = static_cast<size_t>(lane);
    return !curr_state_[index] && prev_state_[index];
}
