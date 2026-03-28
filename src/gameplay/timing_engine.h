#pragma once

#include <vector>

#include "data_models.h"

class timing_engine {
public:
    void init(std::vector<timing_event> events, int resolution);
    double tick_to_ms(int tick) const;
    int ms_to_tick(double ms) const;
    float get_bpm_at(int tick) const;

private:
    struct bpm_segment {
        int start_tick = 0;
        double start_ms = 0.0;
        float bpm = 120.0f;
    };

    std::vector<bpm_segment> bpm_segments_;
    int resolution_ = 480;
};
