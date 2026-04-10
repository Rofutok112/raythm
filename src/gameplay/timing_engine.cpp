#include "timing_engine.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
double tick_delta_to_ms(int tick_delta, float bpm, int resolution) {
    return static_cast<double>(tick_delta) * 60000.0 / (static_cast<double>(resolution) * static_cast<double>(bpm));
}
}

void timing_engine::init(std::vector<timing_event> events, int resolution, int offset_ms) {
    if (resolution <= 0) {
        throw std::invalid_argument("resolution must be greater than zero");
    }

    resolution_ = resolution;
    offset_ms_ = offset_ms;
    bpm_segments_.clear();
    meter_segments_.clear();

    std::vector<timing_event> bpm_events;
    std::vector<timing_event> meter_events;
    for (const timing_event& event : events) {
        if (event.type == timing_event_type::bpm) {
            bpm_events.push_back(event);
        } else if (event.type == timing_event_type::meter) {
            meter_events.push_back(event);
        }
    }

    std::sort(bpm_events.begin(), bpm_events.end(), [](const timing_event& left, const timing_event& right) {
        if (left.tick != right.tick) {
            return left.tick < right.tick;
        }
        return left.bpm < right.bpm;
    });

    if (bpm_events.empty() || bpm_events.front().tick != 0) {
        bpm_segments_.push_back({0, 0.0, 120.0f});
    }

    for (const timing_event& event : bpm_events) {
        if (event.bpm <= 0.0f) {
            throw std::invalid_argument("bpm must be greater than zero");
        }

        if (!bpm_segments_.empty() && bpm_segments_.back().start_tick == event.tick) {
            bpm_segments_.back().bpm = event.bpm;
            continue;
        }

        bpm_segment segment;
        segment.start_tick = event.tick;
        segment.bpm = event.bpm;

        if (bpm_segments_.empty()) {
            segment.start_ms = 0.0;
        } else {
            const bpm_segment& previous = bpm_segments_.back();
            segment.start_ms = previous.start_ms +
                               tick_delta_to_ms(event.tick - previous.start_tick, previous.bpm, resolution_);
        }

        bpm_segments_.push_back(segment);
    }

    if (bpm_segments_.empty()) {
        bpm_segments_.push_back({0, 0.0, 120.0f});
    }

    std::sort(meter_events.begin(), meter_events.end(), [](const timing_event& left, const timing_event& right) {
        if (left.tick != right.tick) {
            return left.tick < right.tick;
        }
        if (left.numerator != right.numerator) {
            return left.numerator < right.numerator;
        }
        return left.denominator < right.denominator;
    });

    if (meter_events.empty() || meter_events.front().tick != 0) {
        meter_segments_.push_back({0, 4, 4});
    }

    for (const timing_event& event : meter_events) {
        if (event.numerator <= 0 || event.denominator <= 0) {
            throw std::invalid_argument("meter values must be greater than zero");
        }

        if (!meter_segments_.empty() && meter_segments_.back().start_tick == event.tick) {
            meter_segments_.back().numerator = event.numerator;
            meter_segments_.back().denominator = event.denominator;
            continue;
        }

        meter_segments_.push_back({event.tick, event.numerator, event.denominator});
    }

    if (meter_segments_.empty()) {
        meter_segments_.push_back({0, 4, 4});
    }
}

double timing_engine::tick_to_ms(int tick) const {
    if (tick < 0) {
        throw std::invalid_argument("tick must be zero or greater");
    }

    const auto upper = std::upper_bound(
        bpm_segments_.begin(), bpm_segments_.end(), tick,
        [](int target_tick, const bpm_segment& segment) { return target_tick < segment.start_tick; });
    const bpm_segment& segment = upper == bpm_segments_.begin() ? bpm_segments_.front() : *std::prev(upper);

    return segment.start_ms + tick_delta_to_ms(tick - segment.start_tick, segment.bpm, resolution_) +
           static_cast<double>(offset_ms_);
}

int timing_engine::ms_to_tick(double ms) const {
    const double adjusted_ms = ms - static_cast<double>(offset_ms_);

    const auto upper = std::upper_bound(
        bpm_segments_.begin(), bpm_segments_.end(), adjusted_ms,
        [](double target_ms, const bpm_segment& segment) { return target_ms < segment.start_ms; });
    const bpm_segment& segment = upper == bpm_segments_.begin() ? bpm_segments_.front() : *std::prev(upper);

    const double tick_delta = (adjusted_ms - segment.start_ms) * static_cast<double>(resolution_) *
                              static_cast<double>(segment.bpm) / 60000.0;
    return segment.start_tick + static_cast<int>(std::lround(tick_delta));
}

float timing_engine::get_bpm_at(int tick) const {
    if (tick < 0) {
        throw std::invalid_argument("tick must be zero or greater");
    }

    const auto upper = std::upper_bound(
        bpm_segments_.begin(), bpm_segments_.end(), tick,
        [](int target_tick, const bpm_segment& segment) { return target_tick < segment.start_tick; });
    const bpm_segment& segment = upper == bpm_segments_.begin() ? bpm_segments_.front() : *std::prev(upper);
    return segment.bpm;
}

int timing_engine::get_meter_numerator_at(int tick) const {
    if (tick < 0) {
        throw std::invalid_argument("tick must be zero or greater");
    }

    const auto upper = std::upper_bound(
        meter_segments_.begin(), meter_segments_.end(), tick,
        [](int target_tick, const meter_segment& segment) { return target_tick < segment.start_tick; });
    const meter_segment& segment = upper == meter_segments_.begin() ? meter_segments_.front() : *std::prev(upper);
    return segment.numerator;
}

int timing_engine::get_meter_denominator_at(int tick) const {
    if (tick < 0) {
        throw std::invalid_argument("tick must be zero or greater");
    }

    const auto upper = std::upper_bound(
        meter_segments_.begin(), meter_segments_.end(), tick,
        [](int target_tick, const meter_segment& segment) { return target_tick < segment.start_tick; });
    const meter_segment& segment = upper == meter_segments_.begin() ? meter_segments_.front() : *std::prev(upper);
    return segment.denominator;
}
