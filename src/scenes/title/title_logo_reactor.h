#pragma once

#include "raylib.h"

class title_logo_reactor final {
public:
    void reset();
    void update(float dt);

    [[nodiscard]] int transform_font_size(int base_font_size) const;
    [[nodiscard]] Rectangle transform_rect(const Rectangle& base_rect) const;

private:
    float pulse_ = 0.0f;
};
