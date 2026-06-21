#include "mv/composition/mv_composition_evaluator.h"

#include <algorithm>
#include <cmath>

namespace mv::composition {

namespace {

constexpr double kPi = 3.14159265358979323846;

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

void apply_fade_effect(const layer& layer, const effect& effect, double time_ms, transform& result) {
    const double local_ms = std::max(0.0, time_ms - layer.start_ms);
    const double duration_ms = layer.duration_ms <= 0.0 ? 0.0 : std::max(1.0, layer.duration_ms);
    const double fade_ms = std::clamp(static_cast<double>(effect.amount <= 0.0f ? 500.0f : effect.amount),
                                      1.0, duration_ms > 0.0 ? duration_ms * 0.5 : 30000.0);
    float opacity = 1.0f;
    if (local_ms < fade_ms) {
        opacity *= static_cast<float>(local_ms / fade_ms);
    }
    if (duration_ms > 0.0 && duration_ms - local_ms < fade_ms) {
        opacity *= static_cast<float>(std::max(0.0, duration_ms - local_ms) / fade_ms);
    }
    result.opacity = clamp01(result.opacity * opacity);
}

void apply_pulse_effect(const layer& layer, const effect& effect, double time_ms, transform& result) {
    const double local_ms = std::max(0.0, time_ms - layer.start_ms);
    const float amount = std::clamp(effect.amount <= 0.0f ? 0.08f : effect.amount, 0.0f, 1.0f);
    const float wave = static_cast<float>((std::sin((local_ms / 500.0) * kPi * 2.0) + 1.0) * 0.5);
    const float multiplier = 1.0f + amount * wave;
    if (effect.target == "transform.opacity" || effect.target == "opacity") {
        result.opacity = clamp01(result.opacity * (1.0f - amount + amount * wave));
    } else {
        result.scale_x *= multiplier;
        result.scale_y *= multiplier;
    }
}

void apply_flash_effect(const layer& layer, const effect& effect, double time_ms, transform& result) {
    const double local_ms = std::max(0.0, time_ms - layer.start_ms);
    const double cycle_ms = 500.0;
    const double phase = std::fmod(local_ms, cycle_ms) / cycle_ms;
    const float amount = std::clamp(effect.amount <= 0.0f ? 0.35f : effect.amount, 0.0f, 1.0f);
    const float flash = phase < 0.18
        ? static_cast<float>(1.0 - phase / 0.18)
        : 0.0f;
    result.opacity = clamp01(result.opacity + (1.0f - result.opacity) * amount * flash);
}

void apply_shake_effect(const layer& layer, const effect& effect, double time_ms, transform& result) {
    const double local_ms = std::max(0.0, time_ms - layer.start_ms);
    const float amount = std::clamp(effect.amount <= 0.0f ? 18.0f : effect.amount, 0.0f, 120.0f);
    const float phase = static_cast<float>(local_ms / 1000.0);
    const float x = std::sin(phase * 83.0f + 0.3f) * 0.65f +
                    std::sin(phase * 151.0f + 1.7f) * 0.35f;
    const float y = std::sin(phase * 97.0f + 2.1f) * 0.62f +
                    std::sin(phase * 137.0f + 0.4f) * 0.38f;
    result.position_x += x * amount;
    result.position_y += y * amount;
}

}  // namespace

float evaluate_track(const keyframe_track& track, double time_ms, float fallback) {
    if (track.points.empty()) {
        return fallback;
    }
    if (time_ms <= track.points.front().time_ms) {
        return track.points.front().value;
    }
    if (time_ms >= track.points.back().time_ms) {
        return track.points.back().value;
    }
    for (std::size_t i = 1; i < track.points.size(); ++i) {
        const keyframe& right = track.points[i];
        if (time_ms > right.time_ms) {
            continue;
        }
        const keyframe& left = track.points[i - 1];
        const double span = std::max(1.0, right.time_ms - left.time_ms);
        const float t = static_cast<float>((time_ms - left.time_ms) / span);
        return lerp(left.value, right.value, std::clamp(t, 0.0f, 1.0f));
    }
    return fallback;
}

transform evaluate_transform(const layer& layer, double time_ms) {
    transform result = layer.transform_data;
    for (const keyframe_track& track : layer.keyframes) {
        if (track.target == "transform.position.x") {
            result.position_x = evaluate_track(track, time_ms, result.position_x);
        } else if (track.target == "transform.position.y") {
            result.position_y = evaluate_track(track, time_ms, result.position_y);
        } else if (track.target == "transform.scale.x") {
            result.scale_x = evaluate_track(track, time_ms, result.scale_x);
        } else if (track.target == "transform.scale.y") {
            result.scale_y = evaluate_track(track, time_ms, result.scale_y);
        } else if (track.target == "transform.rotationDeg") {
            result.rotation_deg = evaluate_track(track, time_ms, result.rotation_deg);
        } else if (track.target == "transform.opacity") {
            result.opacity = evaluate_track(track, time_ms, result.opacity);
        }
    }
    for (const effect& current : layer.effects) {
        if (current.type == "fade") {
            apply_fade_effect(layer, current, time_ms, result);
        } else if (current.type == "pulse" || current.type == "beatPulse") {
            apply_pulse_effect(layer, current, time_ms, result);
        } else if (current.type == "flash") {
            apply_flash_effect(layer, current, time_ms, result);
        } else if (current.type == "shake") {
            apply_shake_effect(layer, current, time_ms, result);
        }
    }
    return result;
}

void upsert_keyframe(keyframe_track& track, keyframe point) {
    if (point.easing.empty()) {
        point.easing = "linear";
    }
    auto it = std::lower_bound(track.points.begin(), track.points.end(), point.time_ms,
                               [](const keyframe& existing, double time_ms) {
                                   return existing.time_ms < time_ms;
                               });
    if (it != track.points.end() && it->time_ms == point.time_ms) {
        *it = point;
        return;
    }
    track.points.insert(it, point);
}

keyframe_track& ensure_keyframe_track(layer& layer, const std::string& target) {
    auto it = std::find_if(layer.keyframes.begin(), layer.keyframes.end(), [&](const keyframe_track& track) {
        return track.target == target;
    });
    if (it != layer.keyframes.end()) {
        return *it;
    }
    layer.keyframes.push_back({target, {}});
    return layer.keyframes.back();
}

bool is_transform_keyframe_target(const std::string& target) {
    return target == "transform.position.x" ||
           target == "transform.position.y" ||
           target == "transform.scale.x" ||
           target == "transform.scale.y" ||
           target == "transform.rotationDeg" ||
           target == "transform.opacity";
}

int count_transform_keyframes_near(const layer& layer, double time_ms, double tolerance_ms) {
    int count = 0;
    for (const keyframe_track& track : layer.keyframes) {
        if (!is_transform_keyframe_target(track.target)) {
            continue;
        }
        for (const keyframe& point : track.points) {
            if (std::abs(point.time_ms - time_ms) <= tolerance_ms) {
                ++count;
            }
        }
    }
    return count;
}

int erase_transform_keyframes_near(layer& layer, double time_ms, double tolerance_ms) {
    int removed = 0;
    for (keyframe_track& track : layer.keyframes) {
        if (!is_transform_keyframe_target(track.target)) {
            continue;
        }
        const auto before = track.points.size();
        track.points.erase(std::remove_if(track.points.begin(), track.points.end(), [&](const auto& point) {
                               return std::abs(point.time_ms - time_ms) <= tolerance_ms;
                           }),
                           track.points.end());
        removed += static_cast<int>(before - track.points.size());
    }
    layer.keyframes.erase(std::remove_if(layer.keyframes.begin(), layer.keyframes.end(),
                                         [](const keyframe_track& track) {
                                             return is_transform_keyframe_target(track.target) &&
                                                    track.points.empty();
                                         }),
                          layer.keyframes.end());
    return removed;
}

}  // namespace mv::composition
