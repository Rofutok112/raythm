#include "chart_rc_calculator.h"

#include <algorithm>
#include <cmath>

#include "chart_difficulty.h"

namespace chart_rc {

float event_weight_for(float local_difficulty) {
    if (local_difficulty <= 0.0f) {
        return 0.0f;
    }
    return std::pow(std::max(local_difficulty, 0.001f), 1.12f);
}

float max_rc_for(const chart_data& chart, size_t weighted_event_count) {
    const float level = chart.meta.level > 0.0f
                            ? chart.meta.level
                            : chart_difficulty::calculate_level(chart);
    if (level <= 0.0f || weighted_event_count == 0) {
        return 0.0f;
    }

    return std::round(std::clamp(level, 0.1f, 99.0f) * 100.0f);
}

}  // namespace chart_rc
