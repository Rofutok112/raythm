#pragma once

#include "data_models.h"

namespace chart_difficulty {

float calculate_rating(const chart_data& data);
float calculate_level(const chart_data& data);

}  // namespace chart_difficulty
