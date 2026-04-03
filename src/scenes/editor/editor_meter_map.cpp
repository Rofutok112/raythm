#include "editor_meter_map.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
std::vector<timing_event> sorted_meter_events(const chart_data& data) {
    std::vector<timing_event> meter_events;
    for (const timing_event& event : data.timing_events) {
        if (event.type == timing_event_type::meter) {
            meter_events.push_back(event);
        }
    }

    std::sort(meter_events.begin(), meter_events.end(), [](const timing_event& left, const timing_event& right) {
        return left.tick < right.tick;
    });

    return meter_events;
}
}

void editor_meter_map::rebuild(const chart_data& data) {
    meter_segments_.clear();
    resolution_ = std::max(1, data.meta.resolution);

    const std::vector<timing_event> events = sorted_meter_events(data);
    if (events.empty() || events.front().tick != 0) {
        meter_segments_.push_back({0, 4, 4, 0, 0});
    }

    for (const timing_event& event : events) {
        if (!meter_segments_.empty() && meter_segments_.back().start_tick == event.tick) {
            meter_segments_.back().numerator = event.numerator;
            meter_segments_.back().denominator = event.denominator;
            continue;
        }

        meter_segment segment;
        segment.start_tick = event.tick;
        segment.numerator = event.numerator;
        segment.denominator = event.denominator;

        if (meter_segments_.empty()) {
            segment.beat_index_offset = 0;
            segment.measure_index_offset = 0;
        } else {
            const meter_segment& previous = meter_segments_.back();
            const int beat_ticks = resolution_ * 4 / std::max(previous.denominator, 1);
            const int measure_ticks = beat_ticks * std::max(previous.numerator, 1);
            const int beat_count = std::max(0, (segment.start_tick - previous.start_tick) / std::max(1, beat_ticks));
            const int measure_count = std::max(0, (segment.start_tick - previous.start_tick) / std::max(1, measure_ticks));
            segment.beat_index_offset = previous.beat_index_offset + beat_count;
            segment.measure_index_offset = previous.measure_index_offset + measure_count;
        }

        meter_segments_.push_back(segment);
    }
}

std::vector<editor_meter_map::grid_line> editor_meter_map::visible_grid_lines(int min_tick, int max_tick) const {
    std::vector<grid_line> lines;
    if (max_tick < min_tick) {
        return lines;
    }

    for (size_t i = 0; i < meter_segments_.size(); ++i) {
        const meter_segment& segment = meter_segments_[i];
        const int next_tick = i + 1 < meter_segments_.size() ? meter_segments_[i + 1].start_tick : max_tick + resolution_ * 16;
        const int beat_ticks = std::max(1, resolution_ * 4 / std::max(segment.denominator, 1));
        const int start_tick = std::max(min_tick, segment.start_tick);
        const int end_tick = std::min(max_tick, next_tick - (i + 1 < meter_segments_.size() ? 1 : 0));
        int tick = segment.start_tick;
        if (tick < start_tick) {
            const int delta = start_tick - tick;
            tick += (delta / beat_ticks) * beat_ticks;
            while (tick < start_tick) {
                tick += beat_ticks;
            }
        }

        for (; tick <= end_tick; tick += beat_ticks) {
            const int relative_tick = tick - segment.start_tick;
            const int local_beat_index = relative_tick / beat_ticks;
            const int beat = local_beat_index % std::max(segment.numerator, 1) + 1;
            const int measure = segment.measure_index_offset + local_beat_index / std::max(segment.numerator, 1) + 1;
            lines.push_back({tick, beat == 1, measure, beat});
        }
    }

    return lines;
}

double editor_meter_map::beat_number_at_tick(int tick) const {
    const meter_segment* segment = segment_at_tick(tick);
    if (segment == nullptr) {
        return 1.0;
    }

    const int beat_ticks = std::max(1, resolution_ * 4 / std::max(segment->denominator, 1));
    const double local_beats = static_cast<double>(tick - segment->start_tick) / static_cast<double>(beat_ticks);
    return static_cast<double>(segment->beat_index_offset) + local_beats + 1.0;
}

editor_meter_map::bar_beat_position editor_meter_map::bar_beat_at_tick(int tick) const {
    const meter_segment* segment = segment_at_tick(tick);
    if (segment == nullptr) {
        return {};
    }

    const int numerator = std::max(segment->numerator, 1);
    const int beat_ticks = std::max(1, resolution_ * 4 / std::max(segment->denominator, 1));
    const int local_beat_index = std::max(0, static_cast<int>(std::llround(
        static_cast<double>(tick - segment->start_tick) / static_cast<double>(beat_ticks))));
    return {
        segment->measure_index_offset + local_beat_index / numerator + 1,
        local_beat_index % numerator + 1
    };
}

std::optional<int> editor_meter_map::tick_from_bar_beat(int measure, int beat) const {
    if (measure <= 0 || beat <= 0 || meter_segments_.empty()) {
        return std::nullopt;
    }

    for (size_t i = 0; i < meter_segments_.size(); ++i) {
        const meter_segment& segment = meter_segments_[i];
        const int first_measure = segment.measure_index_offset + 1;
        const int next_measure = i + 1 < meter_segments_.size()
            ? meter_segments_[i + 1].measure_index_offset + 1
            : std::numeric_limits<int>::max();
        if (measure < first_measure || measure >= next_measure) {
            continue;
        }

        const int numerator = std::max(segment.numerator, 1);
        if (beat > numerator) {
            return std::nullopt;
        }

        const int beat_ticks = std::max(1, resolution_ * 4 / std::max(segment.denominator, 1));
        const int measure_ticks = beat_ticks * numerator;
        return segment.start_tick + (measure - first_measure) * measure_ticks + (beat - 1) * beat_ticks;
    }

    const meter_segment& segment = meter_segments_.back();
    const int first_measure = segment.measure_index_offset + 1;
    const int numerator = std::max(segment.numerator, 1);
    if (measure < first_measure || beat > numerator) {
        return std::nullopt;
    }

    const int beat_ticks = std::max(1, resolution_ * 4 / std::max(segment.denominator, 1));
    const int measure_ticks = beat_ticks * numerator;
    return segment.start_tick + (measure - first_measure) * measure_ticks + (beat - 1) * beat_ticks;
}

std::string editor_meter_map::bar_beat_label(int tick) const {
    const bar_beat_position position = bar_beat_at_tick(tick);
    return std::to_string(position.measure) + ":" + std::to_string(position.beat);
}

const editor_meter_map::meter_segment* editor_meter_map::segment_at_tick(int tick) const {
    if (meter_segments_.empty()) {
        return nullptr;
    }

    const auto it = std::upper_bound(meter_segments_.begin(), meter_segments_.end(), tick,
                                     [](int value, const meter_segment& segment) {
                                         return value < segment.start_tick;
                                     });
    return it == meter_segments_.begin() ? &meter_segments_.front() : &*std::prev(it);
}
