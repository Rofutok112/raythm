#pragma once

#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "editor/editor_meter_map.h"
#include "gameplay/timing_engine.h"

namespace song_create::timing_service {

bool event_less(const timing_event& left, const timing_event& right);
void sort_events(std::vector<timing_event>& events);
void ensure_base_events(std::vector<timing_event>& events, float base_bpm);
std::vector<timing_event> validated_events(const std::vector<timing_event>& events,
                                           float base_bpm,
                                           bool& ok);
std::optional<editor_meter_map::bar_beat_position> parse_bar_beat_text(const std::string& text);
editor_meter_map build_meter_map(const std::vector<timing_event>& events, int timing_resolution);
int beat_step_ticks_at(const timing_engine& engine, int tick, int resolution);

}  // namespace song_create::timing_service
