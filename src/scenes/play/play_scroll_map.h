#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "data_models.h"
#include "timing_engine.h"

class play_scroll_map final {
public:
    void init(const chart_data& chart, const timing_engine& timing) {
        entries_.clear();
        entries_.reserve(chart.scroll_events.size());
        for (const scroll_event& event : chart.scroll_events) {
            if (event.duration <= 0) {
                continue;
            }

            const double start_ms = timing.tick_to_ms(event.tick);
            const double end_ms = timing.tick_to_ms(event.tick + event.duration);
            entries_.push_back({
                std::min(start_ms, end_ms),
                std::max(start_ms, end_ms),
                event.type == scroll_event_type::stop ? 0.0f : std::max(0.0f, event.multiplier),
            });
        }
    }

    [[nodiscard]] double visual_ms_at(double source_ms) const {
        double visual_ms = source_ms;
        for (const entry& event : entries_) {
            if (source_ms <= event.start_ms) {
                continue;
            }

            const double overlap_ms = std::clamp(source_ms, event.start_ms, event.end_ms) - event.start_ms;
            if (overlap_ms > 0.0) {
                visual_ms += overlap_ms * (static_cast<double>(event.multiplier) - 1.0);
            }
        }
        return visual_ms;
    }

private:
    struct entry {
        double start_ms = 0.0;
        double end_ms = 0.0;
        float multiplier = 1.0f;
    };

    std::vector<entry> entries_;
};
