#pragma once

#include <optional>
#include <string>
#include <vector>

#include "data_models.h"

class editor_meter_map {
public:
    struct grid_line {
        int tick = 0;
        bool major = false;
        int measure = 1;
        int beat = 1;
    };

    struct bar_beat_position {
        int measure = 1;
        int beat = 1;
    };

    void rebuild(const chart_data& data);

    std::vector<grid_line> visible_grid_lines(int min_tick, int max_tick) const;
    double beat_number_at_tick(int tick) const;
    bar_beat_position bar_beat_at_tick(int tick) const;
    std::optional<int> tick_from_bar_beat(int measure, int beat) const;
    std::string bar_beat_label(int tick) const;

private:
    struct meter_segment {
        int start_tick = 0;
        int numerator = 4;
        int denominator = 4;
        int beat_index_offset = 0;
        int start_measure = 1;
        int start_beat = 1;
    };

    const meter_segment* segment_at_tick(int tick) const;

    std::vector<meter_segment> meter_segments_;
    int resolution_ = 480;
};
