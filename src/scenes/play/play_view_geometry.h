#pragma once

#include <optional>

#include "raylib.h"

namespace play_view_geometry {

struct lane_view {
    float lane_start_z = 0.0f;
    float judgement_z = 0.0f;
    float lane_end_z = 0.0f;
    float lane_width = 0.0f;
};

Camera3D make_camera(float camera_angle_degrees);
std::optional<lane_view> resolve_lane_view(const Camera3D& camera,
                                           int key_count,
                                           float camera_angle_degrees,
                                           float setting_lane_width);
float lane_width_for_bottom_edge(const Camera3D& camera,
                                 float lane_start_z,
                                 int key_count,
                                 float setting_lane_width);

}  // namespace play_view_geometry
