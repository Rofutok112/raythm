#include "play/play_view_geometry.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "game_settings.h"
#include "raymath.h"
#include "scene_common.h"

namespace {

constexpr float kJudgementLineScreenRatioFromBottom = 0.10f;
constexpr float kCameraHeight = 42.0f;
constexpr float kCameraFovY = 42.0f;
constexpr float kJudgeLineWorldZ = 12.0f;
constexpr float kMaxGroundDistance = 1000.0f;
constexpr float kMinResolvedLaneWidth = 0.05f;

Vector3 build_camera_forward(float camera_angle_degrees) {
    const float angle_rad = std::clamp(camera_angle_degrees, 5.0f, 90.0f) * DEG2RAD;
    return Vector3{0.0f, -std::sin(angle_rad), std::cos(angle_rad)};
}

Vector3 choose_camera_up(Vector3 forward) {
    return std::fabs(Vector3DotProduct(forward, Vector3{0.0f, 1.0f, 0.0f})) > 0.98f
               ? Vector3{0.0f, 0.0f, 1.0f}
               : Vector3{0.0f, 1.0f, 0.0f};
}

std::optional<float> ground_z_offset(float height, float angle_rad, float half_fov_rad, float screen_ndc_y) {
    const float k = screen_ndc_y * std::tan(half_fov_rad);
    const float sin_a = std::sin(angle_rad);
    const float cos_a = std::cos(angle_rad);
    const float denominator = sin_a - k * cos_a;
    if (denominator <= 0.0001f) {
        return std::nullopt;
    }
    return height * (cos_a + k * sin_a) / denominator;
}

}  // namespace

namespace play_view_geometry {

Camera3D make_camera(float camera_angle_degrees) {
    const Vector3 forward = build_camera_forward(camera_angle_degrees);
    const Vector3 up = choose_camera_up(forward);
    const float angle_rad = std::clamp(camera_angle_degrees, 5.0f, 90.0f) * DEG2RAD;
    constexpr float half_fov_rad = kCameraFovY * DEG2RAD * 0.5f;
    constexpr float judge_ndc_y = (kJudgementLineScreenRatioFromBottom - 0.5f) * 2.0f;
    const std::optional<float> judge_offset = ground_z_offset(kCameraHeight, angle_rad, half_fov_rad, judge_ndc_y);
    const float camera_z = judge_offset.has_value() ? (kJudgeLineWorldZ - *judge_offset) : 0.0f;

    Camera3D camera = {};
    camera.position = {0.0f, kCameraHeight, camera_z};
    camera.target = Vector3Add(camera.position, forward);
    camera.up = up;
    camera.fovy = kCameraFovY;
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

std::optional<lane_view> resolve_lane_view(const Camera3D& camera,
                                           int key_count,
                                           float camera_angle_degrees,
                                           float setting_lane_width) {
    const float angle_rad = std::clamp(camera_angle_degrees, 5.0f, 90.0f) * DEG2RAD;
    constexpr float half_fov_rad = kCameraFovY * DEG2RAD * 0.5f;

    const std::optional<float> near_offset = ground_z_offset(kCameraHeight, angle_rad, half_fov_rad, -1.0f);
    if (!near_offset.has_value()) {
        return std::nullopt;
    }

    lane_view view;
    view.judgement_z = kJudgeLineWorldZ;
    view.lane_start_z = camera.position.z + *near_offset;

    const std::optional<float> far_offset = ground_z_offset(kCameraHeight, angle_rad, half_fov_rad, 1.0f);
    if (far_offset.has_value()) {
        view.lane_end_z = std::min(camera.position.z + *far_offset, camera.position.z + kMaxGroundDistance);
    } else {
        view.lane_end_z = camera.position.z + kMaxGroundDistance;
    }

    if (view.lane_end_z <= view.lane_start_z) {
        return std::nullopt;
    }

    view.lane_start_z = std::min(view.lane_start_z, view.judgement_z - 0.5f);
    view.lane_end_z = std::max(view.lane_end_z, view.judgement_z + 8.0f);
    view.lane_width = lane_width_for_bottom_edge(camera, view.lane_start_z, key_count, setting_lane_width);
    return view;
}

float lane_width_for_bottom_edge(const Camera3D& camera,
                                 float lane_start_z,
                                 int key_count,
                                 float setting_lane_width) {
    key_count = std::max(1, key_count);
    const float clamped_setting_width = std::clamp(setting_lane_width, kMinLaneWidth, kMaxLaneWidth);
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 bottom_center = {0.0f, 0.0f, lane_start_z};
    const float depth = Vector3DotProduct(Vector3Subtract(bottom_center, camera.position), forward);
    if (depth <= 0.001f || camera.fovy <= 0.0f) {
        return clamped_setting_width;
    }

    const float half_fov_y = camera.fovy * DEG2RAD * 0.5f;
    const float aspect = static_cast<float>(kScreenWidth) / static_cast<float>(kScreenHeight);
    const float half_fov_x = std::atan(std::tan(half_fov_y) * aspect);
    const float bottom_world_width = depth * std::tan(half_fov_x) * 2.0f;
    const float desired_total_width = bottom_world_width * (clamped_setting_width / kMaxLaneWidth);
    return std::max(kMinResolvedLaneWidth, desired_total_width / static_cast<float>(key_count));
}

}  // namespace play_view_geometry
