#pragma once

#include <array>
#include <span>
#include <vector>

#include "data_models.h"

class lane_input_tracker {
public:
    static constexpr int kMaxLanes = 6;

    void set_key_count(int key_count);
    void reset();
    void begin_frame();
    void update_from_lane_states(std::span<const bool> lane_states, double input_event_time_ms = 0.0);
    bool apply_lane_event(input_event_type type, int lane, double input_event_time_ms);

    [[nodiscard]] std::span<const input_event> events() const;
    [[nodiscard]] int last_update_event_count() const;
    [[nodiscard]] bool is_lane_just_pressed(int lane) const;
    [[nodiscard]] bool is_lane_held(int lane) const;
    [[nodiscard]] bool is_lane_just_released(int lane) const;

private:
    int key_count_ = 4;
    std::array<bool, kMaxLanes> prev_state_ = {};
    std::array<bool, kMaxLanes> curr_state_ = {};
    std::vector<input_event> events_;
};
