#pragma once

#include "raylib.h"

class title_logo_reactor final {
public:
    void reset();
    void update(float dt);

    [[nodiscard]] Rectangle transform_rect(const Rectangle& base_rect) const;

private:
    float pulse_ = 0.0f;
    float time_ = 0.0f;
};
