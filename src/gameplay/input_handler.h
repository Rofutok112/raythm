#pragma once

#include <array>
#include <span>
#include <vector>

#include "data_models.h"
#include "raylib.h"

struct key_config {
    std::array<KeyboardKey, 4> keys_4 = {KEY_D, KEY_F, KEY_J, KEY_K};
    std::array<KeyboardKey, 6> keys_6 = {KEY_S, KEY_D, KEY_F, KEY_J, KEY_K, KEY_L};

    std::span<const KeyboardKey> get_lane_keys(int key_count) const;
};

class input_handler {
public:
    explicit input_handler(key_config config = {});

    void set_key_count(int key_count);
    void update(double timestamp_ms);
    void update_from_lane_states(std::span<const bool> lane_states, double timestamp_ms = 0.0);

    std::span<const input_event> events() const;

    bool is_lane_just_pressed(int lane) const;
    bool is_lane_held(int lane) const;
    bool is_lane_just_released(int lane) const;

private:
    static constexpr int kMaxLanes = 6;

    key_config key_config_;
    int key_count_ = 4;
    std::array<bool, kMaxLanes> prev_state_ = {};
    std::array<bool, kMaxLanes> curr_state_ = {};
    std::vector<input_event> events_;
};
