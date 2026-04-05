#pragma once

#include <algorithm>

#include "raylib.h"
#include "ui_draw.h"

class scene_fade final {
public:
    enum class direction {
        in,
        out,
    };

    scene_fade() = default;
    scene_fade(direction mode, float duration_seconds, float max_alpha = 0.65f);

    void restart(direction mode, float duration_seconds, float max_alpha = 0.65f);
    void update(float dt);

    [[nodiscard]] bool active() const;
    [[nodiscard]] bool complete() const;
    [[nodiscard]] float progress() const;
    [[nodiscard]] float alpha_scale() const;
    [[nodiscard]] Color overlay_color() const;

    void draw() const;

private:
    direction mode_ = direction::in;
    float duration_seconds_ = 0.0f;
    float elapsed_seconds_ = 0.0f;
    float max_alpha_ = 0.65f;
};
