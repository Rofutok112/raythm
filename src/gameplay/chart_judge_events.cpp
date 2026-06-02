#include "chart_judge_events.h"

#include <algorithm>

namespace {

int center_lane2(const chart_judge_event& event) {
    return event.lane * 2 + event.lane_width - 1;
}

void append_event(std::vector<chart_judge_event>& events,
                  const timing_engine& engine,
                  size_t note_index,
                  const note_data& note,
                  chart_judge_event_kind kind,
                  chart_judge_event_role role,
                  int tick) {
    events.push_back({
        -1,
        note_index,
        kind,
        role,
        engine.tick_to_ms(tick),
        tick,
        note.lane,
        note_lane_width(note),
        note.is_ray,
    });
}

void append_note_events(std::vector<chart_judge_event>& events,
                        const timing_engine& engine,
                        size_t note_index,
                        const note_data& note) {
    switch (note.type) {
        case note_type::tap:
            append_event(events, engine, note_index, note, chart_judge_event_kind::press,
                         chart_judge_event_role::tap, note.tick);
            break;

        case note_type::hold:
            append_event(events, engine, note_index, note, chart_judge_event_kind::press,
                         chart_judge_event_role::hold_head, note.tick);
            append_event(events, engine, note_index, note, chart_judge_event_kind::release,
                         chart_judge_event_role::hold_tail, note.end_tick);
            break;

        case note_type::release:
            append_event(events, engine, note_index, note, chart_judge_event_kind::release,
                         chart_judge_event_role::release, note.tick);
            break;

        case note_type::stay:
            append_event(events, engine, note_index, note, chart_judge_event_kind::stay,
                         chart_judge_event_role::stay, note.tick);
            break;

        case note_type::decorative_hold:
            break;
    }
}

void sort_and_assign_event_indices(std::vector<chart_judge_event>& events) {
    std::stable_sort(events.begin(), events.end(), [](const chart_judge_event& left, const chart_judge_event& right) {
        if (left.tick != right.tick) {
            return left.tick < right.tick;
        }
        return center_lane2(left) < center_lane2(right);
    });

    for (size_t i = 0; i < events.size(); ++i) {
        events[i].event_index = static_cast<int>(i);
    }
}

}  // namespace

namespace chart_judge_events {

std::vector<chart_judge_event> build(const chart_data& chart, const timing_engine& engine) {
    std::vector<chart_judge_event> events;
    events.reserve(chart.notes.size() * 2);

    for (size_t note_index = 0; note_index < chart.notes.size(); ++note_index) {
        append_note_events(events, engine, note_index, chart.notes[note_index]);
    }

    sort_and_assign_event_indices(events);
    return events;
}

int count(const chart_data& chart, const timing_engine& engine) {
    return static_cast<int>(build(chart, engine).size());
}

}  // namespace chart_judge_events
