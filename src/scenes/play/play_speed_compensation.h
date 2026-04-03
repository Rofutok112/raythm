#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace play_speed_compensation {

inline constexpr float kReferenceCameraAngleDegrees = 45.0f;
inline constexpr float kCameraFovYDegrees = 42.0f;
inline constexpr float kJudgementLineScreenRatioFromBottom = 0.10f;

inline float judge_line_projection_scale(float camera_angle_degrees) {
    const float clamped_angle = std::clamp(camera_angle_degrees, 5.0f, 90.0f);
    const float angle_rad = clamped_angle * std::numbers::pi_v<float> / 180.0f;
    const float half_fov_rad = kCameraFovYDegrees * std::numbers::pi_v<float> / 360.0f;
    const float judge_line_ndc_y = (kJudgementLineScreenRatioFromBottom - 0.5f) * 2.0f;
    const float perspective_offset = judge_line_ndc_y * std::tan(half_fov_rad);
    const float scale = std::sin(angle_rad) - perspective_offset * std::cos(angle_rad);
    return std::max(scale * scale, 0.0001f);
}

inline float compensated_lane_speed(float note_speed, float camera_angle_degrees) {
    static const float kReferenceScale = judge_line_projection_scale(kReferenceCameraAngleDegrees);
    return note_speed * (kReferenceScale / judge_line_projection_scale(camera_angle_degrees));
}

}  // namespace play_speed_compensation
