#pragma once

#include <array>

#include "raylib.h"

class title_spectrum_visualizer final {
public:
    static constexpr int kBarCount = 64;
    enum class source {
        bgm,
        preview,
    };

    void reset();
    void update(source input_source);
    void draw(const Rectangle& rect, float alpha_scale = 1.0f) const;

private:
    std::array<float, kBarCount> smoothed_levels_ = {};
    std::array<float, kBarCount> bars_ = {};
    std::array<float, kBarCount> peaks_ = {};
    std::array<float, kBarCount> peak_velocities_ = {};
};
