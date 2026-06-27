#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include "raylib.h"

namespace song_select::level_filter {

inline constexpr float kMinLevel = 0.0f;
inline constexpr float kUsefulMaxLevel = 15.0f;
inline constexpr float kMaxLevel = 99.0f;
inline constexpr float kUsefulTrack = 0.97f;

inline float t_for_level(float level) {
    const float clamped = std::clamp(level, kMinLevel, kMaxLevel);
    if (clamped <= kUsefulMaxLevel) {
        return ((clamped - kMinLevel) / (kUsefulMaxLevel - kMinLevel)) * kUsefulTrack;
    }
    return 1.0f;
}

inline float level_from_t(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    if (clamped > kUsefulTrack) {
        return kMaxLevel;
    }
    const float level = kMinLevel + (clamped / kUsefulTrack) * (kUsefulMaxLevel - kMinLevel);
    const float rounded = std::round(level * 10.0f) / 10.0f;
    return rounded >= kMaxLevel - 0.5f ? kMaxLevel : rounded;
}

inline Rectangle chip_rect(Rectangle range, float level) {
    const float x = range.x + range.width * t_for_level(level);
    return {x - 24.0f, range.y - 4.0f, 48.0f, 28.0f};
}

inline Rectangle track_rect(Rectangle range) {
    return {range.x, range.y + 5.0f, range.width, 14.0f};
}

inline std::string label(float level) {
    if (level >= kMaxLevel - 0.05f) {
        return "\xE2\x88\x9E";
    }
    return TextFormat("%.1f", level);
}

}  // namespace song_select::level_filter
