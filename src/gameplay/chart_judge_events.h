#pragma once

#include <vector>

#include "data_models.h"
#include "timing_engine.h"

enum class chart_judge_event_kind {
    press,
    release,
    stay,
};

enum class chart_judge_event_role {
    tap,
    hold_head,
    hold_tail,
    release,
    stay,
};

struct chart_judge_event {
    int event_index = -1;
    size_t note_index = 0;
    chart_judge_event_kind kind = chart_judge_event_kind::press;
    chart_judge_event_role role = chart_judge_event_role::tap;
    double time_ms = 0.0;
    int tick = 0;
    int lane = 0;
    int lane_width = 1;
    bool is_ray = false;
};

namespace chart_judge_events {

std::vector<chart_judge_event> build(const chart_data& chart, const timing_engine& engine);
int count(const chart_data& chart, const timing_engine& engine);

}  // namespace chart_judge_events
