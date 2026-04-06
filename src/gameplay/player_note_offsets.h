#pragma once

#include <string>
#include <unordered_map>

using player_chart_offset_map = std::unordered_map<std::string, int>;

player_chart_offset_map load_player_chart_offsets();
int load_player_chart_offset(const std::string& chart_id);
bool save_player_chart_offset(const std::string& chart_id, int offset_ms);
