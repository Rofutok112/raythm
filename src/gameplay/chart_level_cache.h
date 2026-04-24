#pragma once

#include <optional>
#include <string>

#include "data_models.h"

namespace chart_level_cache {

std::optional<float> find_level(const std::string& chart_path);
float get_or_calculate(const std::string& chart_path, const chart_data& chart);
float calculate_and_store(const std::string& chart_path, const chart_data& chart);
void clear();

}  // namespace chart_level_cache
