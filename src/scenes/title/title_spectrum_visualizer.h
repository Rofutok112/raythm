#pragma once

#include <array>

#include "raylib.h"

class title_spectrum_visualizer final {
public:
    static constexpr int kBarCount = 48;

    void reset();
    void update();
    void draw(const Rectangle& rect) const;

private:
    std::array<float, kBarCount> bars_ = {};
    std::array<float, kBarCount> peaks_ = {};
    std::array<float, kBarCount> peak_velocities_ = {};
    std::array<float, kBarCount> peak_hold_timers_ = {};
    float dynamic_peak_ = 0.12f;
};
