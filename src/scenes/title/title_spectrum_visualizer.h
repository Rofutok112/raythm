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
};
