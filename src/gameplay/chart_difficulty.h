#pragma once

#include "data_models.h"

namespace chart_difficulty {

float calculate_rating(const chart_data& data);
float level_from_rating(float raw_rating);
float calculate_level(const chart_data& data);
chart_data with_auto_level(chart_data data);
void apply_auto_level(chart_data& data);

}  // namespace chart_difficulty
