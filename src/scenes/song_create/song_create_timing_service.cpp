#include "song_create/song_create_timing_service.h"

#include <algorithm>
#include <cctype>

namespace song_create::timing_service {
namespace {

bool parse_int(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    size_t parsed = 0;
    try {
        const int result = std::stoi(text, &parsed);
        if (parsed != text.size()) {
            return false;
        }
        value = result;
        return true;
    } catch (...) {
        return false;
    }
}

std::string trim_copy(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [is_space](unsigned char ch) {
        return !is_space(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [is_space](unsigned char ch) {
        return !is_space(ch);
    }).base(), value.end());
    return value;
}

}  // namespace

bool event_less(const timing_event& left, const timing_event& right) {
    if (left.tick != right.tick) {
        return left.tick < right.tick;
    }
    return left.type == timing_event_type::bpm && right.type == timing_event_type::meter;
}

void sort_events(std::vector<timing_event>& events) {
    std::stable_sort(events.begin(), events.end(), event_less);
}

void ensure_base_events(std::vector<timing_event>& events, float base_bpm) {
    const float bpm = base_bpm > 0.0f ? base_bpm : 120.0f;
    bool found_tick_zero_bpm = false;
    bool found_tick_zero_meter = false;
    for (timing_event& event : events) {
        if (event.tick != 0) {
            continue;
        }
        if (event.type == timing_event_type::bpm) {
            event.bpm = bpm;
            found_tick_zero_bpm = true;
        } else if (event.type == timing_event_type::meter) {
            found_tick_zero_meter = true;
        }
    }
    if (!found_tick_zero_bpm) {
        events.insert(events.begin(), {timing_event_type::bpm, 0, bpm, 4, 4});
    }
    if (!found_tick_zero_meter) {
        events.push_back({timing_event_type::meter, 0, 0.0f, 4, 4});
    }
    sort_events(events);
}

std::vector<timing_event> validated_events(const std::vector<timing_event>& events,
                                           float base_bpm,
                                           bool& ok) {
    ok = true;
    std::vector<timing_event> result = events;
    ensure_base_events(result, base_bpm);
    for (const timing_event& event : result) {
        if (event.tick < 0 ||
            (event.type == timing_event_type::bpm && event.bpm <= 0.0f) ||
            (event.type == timing_event_type::meter && (event.numerator <= 0 || event.denominator <= 0))) {
            ok = false;
            return {};
        }
    }
    return result;
}

std::optional<editor_meter_map::bar_beat_position> parse_bar_beat_text(const std::string& text) {
    const std::string trimmed = trim_copy(text);
    const size_t colon = trimmed.find(':');
    int measure = 0;
    int beat = 1;
    if (colon == std::string::npos) {
        if (!parse_int(trimmed, measure)) {
            return std::nullopt;
        }
    } else if (!parse_int(trimmed.substr(0, colon), measure) ||
               !parse_int(trimmed.substr(colon + 1), beat)) {
        return std::nullopt;
    }
    if (measure <= 0 || beat <= 0) {
        return std::nullopt;
    }
    return editor_meter_map::bar_beat_position{measure, beat};
}

editor_meter_map build_meter_map(const std::vector<timing_event>& events, int timing_resolution) {
    chart_data chart;
    chart.meta.resolution = timing_resolution > 0 ? timing_resolution : 480;
    chart.timing_events = events;
    editor_meter_map meter_map;
    meter_map.rebuild(chart);
    return meter_map;
}

int beat_step_ticks_at(const timing_engine& engine, int tick, int resolution) {
    const int denominator = std::max(1, engine.get_meter_denominator_at(std::max(0, tick)));
    return std::max(1, resolution * 4 / denominator);
}

}  // namespace song_create::timing_service
