#pragma once

#include "data_models.h"
#include "timing_engine.h"

#include <vector>

namespace chart_difficulty {

struct event_difficulty {
    int event_index = -1;
    double time_ms = 0.0;
    float local_difficulty = 0.0f;
};

float calculate_rating(const chart_data& data);
std::vector<event_difficulty> calculate_event_difficulties(const chart_data& data,
                                                           const timing_engine& engine);
float level_from_rating(float raw_rating);
float calculate_level(const chart_data& data);
chart_data with_auto_level(chart_data data);
void apply_auto_level(chart_data& data);

}  // namespace chart_difficulty
