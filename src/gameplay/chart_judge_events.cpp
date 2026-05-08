#include "chart_judge_events.h"

#include <algorithm>

namespace chart_judge_events {

std::vector<chart_judge_event> build(const chart_data& chart, const timing_engine& engine) {
    std::vector<chart_judge_event> events;
    events.reserve(chart.notes.size() * 2);

    int event_index = 0;
    for (size_t note_index = 0; note_index < chart.notes.size(); ++note_index) {
        const note_data& note = chart.notes[note_index];

        switch (note.type) {
            case note_type::tap:
                events.push_back({
                    event_index++,
                    note_index,
                    chart_judge_event_kind::press,
                    chart_judge_event_role::tap,
                    engine.tick_to_ms(note.tick),
                    note.tick,
                    note.lane,
                    note_lane_width(note),
                    note.is_ray,
                });
                break;

            case note_type::hold:
                events.push_back({
                    event_index++,
                    note_index,
                    chart_judge_event_kind::press,
                    chart_judge_event_role::hold_head,
                    engine.tick_to_ms(note.tick),
                    note.tick,
                    note.lane,
                    note_lane_width(note),
                    note.is_ray,
                });
                events.push_back({
                    event_index++,
                    note_index,
                    chart_judge_event_kind::release,
                    chart_judge_event_role::hold_tail,
                    engine.tick_to_ms(note.end_tick),
                    note.end_tick,
                    note.lane,
                    note_lane_width(note),
                    note.is_ray,
                });
                break;

            case note_type::release:
                events.push_back({
                    event_index++,
                    note_index,
                    chart_judge_event_kind::release,
                    chart_judge_event_role::release,
                    engine.tick_to_ms(note.tick),
                    note.tick,
                    note.lane,
                    note_lane_width(note),
                    note.is_ray,
                });
                break;

            case note_type::stay:
                events.push_back({
                    event_index++,
                    note_index,
                    chart_judge_event_kind::stay,
                    chart_judge_event_role::stay,
                    engine.tick_to_ms(note.tick),
                    note.tick,
                    note.lane,
                    note_lane_width(note),
                    note.is_ray,
                });
                break;
        }
    }

    std::stable_sort(events.begin(), events.end(), [](const chart_judge_event& left, const chart_judge_event& right) {
        if (left.tick != right.tick) {
            return left.tick < right.tick;
        }
        const int left_center_lane2 = left.lane * 2 + left.lane_width - 1;
        const int right_center_lane2 = right.lane * 2 + right.lane_width - 1;
        if (left_center_lane2 != right_center_lane2) {
            return left_center_lane2 < right_center_lane2;
        }
        return left.event_index < right.event_index;
    });

    for (size_t i = 0; i < events.size(); ++i) {
        events[i].event_index = static_cast<int>(i);
    }

    return events;
}

int count(const chart_data& chart, const timing_engine& engine) {
    return static_cast<int>(build(chart, engine).size());
}

}  // namespace chart_judge_events
