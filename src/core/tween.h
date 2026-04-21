#pragma once

#include <algorithm>
#include <cmath>

#include "raylib.h"

namespace tween {

inline float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline float lerp(float from, float to, float t) {
    const float clamped = clamp01(t);
    return from + (to - from) * clamped;
}

inline Vector2 lerp(Vector2 from, Vector2 to, float t) {
    const float clamped = clamp01(t);
    return {
        from.x + (to.x - from.x) * clamped,
        from.y + (to.y - from.y) * clamped,
    };
}

inline Rectangle lerp(Rectangle from, Rectangle to, float t) {
    const float clamped = clamp01(t);
    return {
        lerp(from.x, to.x, clamped),
        lerp(from.y, to.y, clamped),
        lerp(from.width, to.width, clamped),
        lerp(from.height, to.height, clamped),
    };
}

inline float ease_out_quad(float t) {
    const float clamped = clamp01(t);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv;
}

inline float ease_out_cubic(float t) {
    const float clamped = clamp01(t);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv * inv;
}

inline float smoothstep(float t) {
    const float clamped = clamp01(t);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

inline float remap_clamped(float value, float input_start, float input_end) {
    const float delta = input_end - input_start;
    if (std::fabs(delta) < 0.0001f) {
        return value >= input_end ? 1.0f : 0.0f;
    }
    return clamp01((value - input_start) / delta);
}

inline float damp(float current, float target, float dt, float speed, float snap_epsilon = 0.0f) {
    const float next = current + (target - current) * std::min(1.0f, dt * speed);
    if (snap_epsilon > 0.0f && std::fabs(next - target) < snap_epsilon) {
        return target;
    }
    return next;
}

inline float advance(float current, float dt, float speed, float max_value = 1.0f) {
    return std::min(max_value, current + dt * speed);
}

inline float retreat(float current, float dt, float speed, float min_value = 0.0f) {
    return std::max(min_value, current - dt * speed);
}

}  // namespace tween
