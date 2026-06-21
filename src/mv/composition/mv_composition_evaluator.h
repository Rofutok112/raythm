#pragma once

#include "mv/composition/mv_composition.h"

namespace mv::composition {

float evaluate_track(const keyframe_track& track, double time_ms, float fallback);
transform evaluate_transform(const layer& layer, double time_ms);
void upsert_keyframe(keyframe_track& track, keyframe point);
keyframe_track& ensure_keyframe_track(layer& layer, const std::string& target);
bool is_transform_keyframe_target(const std::string& target);
int count_transform_keyframes_near(const layer& layer, double time_ms, double tolerance_ms);
int erase_transform_keyframes_near(layer& layer, double time_ms, double tolerance_ms);

}  // namespace mv::composition
