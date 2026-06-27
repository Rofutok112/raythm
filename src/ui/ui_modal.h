#pragma once

#include <algorithm>

#include "raylib.h"
#include "tween.h"
#include "ui_hit.h"

namespace ui {

struct modal_transition_options {
    float closed_scale = 0.94f;
    float open_scale = 1.0f;
};

inline Rectangle animated_modal_rect(Rectangle target, float open_anim,
                                     modal_transition_options options = {}) {
    const float t = tween::ease_out_cubic(std::clamp(open_anim, 0.0f, 1.0f));
    const float scale = options.closed_scale + (options.open_scale - options.closed_scale) * t;
    const Vector2 center{
        target.x + target.width * 0.5f,
        target.y + target.height * 0.5f,
    };
    return {
        center.x - target.width * scale * 0.5f,
        center.y - target.height * scale * 0.5f,
        target.width * scale,
        target.height * scale,
    };
}

inline bool modal_outside_released(Rectangle modal, Vector2 point, int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonReleased(mouse_button) && !contains_point(modal, point);
}

}  // namespace ui
