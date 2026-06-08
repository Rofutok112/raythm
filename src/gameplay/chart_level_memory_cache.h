#pragma once

#include <optional>
#include <string>

#include "data_models.h"

namespace chart_level_memory_cache {

std::optional<float> find_level(const std::string& chart_path);
std::optional<float> find_level(const std::string& chart_path, const std::string& content_signature);
void remember_level(const std::string& chart_path, float level);
void remember_level(const std::string& chart_path, const std::string& content_signature, float level);
float get_or_calculate(const std::string& chart_path, const chart_data& chart);
float get_or_calculate(const std::string& chart_path, const std::string& content_signature, const chart_data& chart);
float calculate_and_store(const std::string& chart_path, const chart_data& chart);
float calculate_and_store(const std::string& chart_path, const std::string& content_signature, const chart_data& chart);
void clear();

}  // namespace chart_level_memory_cache
