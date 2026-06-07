#pragma once

#include <cstddef>

#include "data_models.h"

namespace chart_rc {

float event_weight_for(float local_difficulty);
float max_rc_for(const chart_data& chart, size_t weighted_event_count);

}  // namespace chart_rc
