#include "editor/service/editor_timing_edit_service.h"

#include <algorithm>
#include <string>

namespace {
bool try_parse_int(const std::string& text, int& out_value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const int value = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        out_value = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_float(const std::string& text, float& out_value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const float value = std::stof(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        out_value = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_bar_beat(const std::string& text, editor_meter_map::bar_beat_position& out_value) {
    if (text.empty()) {
        return false;
    }

    const size_t colon = text.find(':');
    int measure = 0;
    int beat = 1;
    if (colon == std::string::npos) {
        if (!try_parse_int(text, measure)) {
            return false;
        }
    } else {
        if (!try_parse_int(text.substr(0, colon), measure) ||
            !try_parse_int(text.substr(colon + 1), beat)) {
            return false;
        }
    }

    if (measure <= 0 || beat <= 0) {
        return false;
    }

    out_value = {measure, beat};
    return true;
}
}

bool editor_timing_edit_service::can_delete_selected(const editor_timing_delete_query& query) {
    if (!query.timing_panel.selected_event_index.has_value() ||
        *query.timing_panel.selected_event_index >= query.state.data().timing_events.size()) {
        return false;
    }
    const timing_event& event = query.state.data().timing_events[*query.timing_panel.selected_event_index];
    return !(event.type == timing_event_type::bpm && event.tick == 0);
}

bool editor_timing_edit_service::can_delete_selected_scroll(const editor_timing_delete_query& query) {
    return query.timing_panel.selected_scroll_event_index.has_value() &&
           *query.timing_panel.selected_scroll_event_index < query.state.data().scroll_automation.size();
}

editor_timing_edit_result editor_timing_edit_service::apply_selected(editor_timing_edit_context context) {
    editor_timing_edit_result result;
    if (!context.timing_panel.selected_event_index.has_value()) {
        context.timing_panel.input_error = "Select a timing event first.";
        return result;
    }

    const size_t index = *context.timing_panel.selected_event_index;
    if (index >= context.state.data().timing_events.size()) {
        context.timing_panel.input_error = "Selected timing event is out of range.";
        return result;
    }

    timing_event updated = context.state.data().timing_events[index];
    if (updated.type == timing_event_type::bpm) {
        editor_meter_map::bar_beat_position position;
        float bpm = 0.0f;
        if (!try_parse_bar_beat(context.timing_panel.inputs.bpm_bar.value, position)) {
            context.timing_panel.input_error = "Bar must be in M:B format.";
            return result;
        }
        if (!try_parse_float(context.timing_panel.inputs.bpm_value.value, bpm) || bpm <= 0.0f) {
            context.timing_panel.input_error = "BPM must be greater than zero.";
            return result;
        }
        const std::optional<int> tick = context.meter_map.tick_from_bar_beat(position.measure, position.beat);
        if (!tick.has_value()) {
            context.timing_panel.input_error = "Bar is outside the current meter layout.";
            return result;
        }
        updated.tick = *tick;
        updated.bpm = bpm;
    } else {
        editor_meter_map::bar_beat_position position;
        int numerator = 0;
        int denominator = 0;
        if (!try_parse_bar_beat(context.timing_panel.inputs.meter_bar.value, position)) {
            context.timing_panel.input_error = "Bar must be in M:B format.";
            return result;
        }
        if (!try_parse_int(context.timing_panel.inputs.meter_numerator.value, numerator) || numerator <= 0) {
            context.timing_panel.input_error = "Numerator must be 1 or greater.";
            return result;
        }
        if (!try_parse_int(context.timing_panel.inputs.meter_denominator.value, denominator) || denominator <= 0) {
            context.timing_panel.input_error = "Denominator must be 1 or greater.";
            return result;
        }
        const std::optional<int> tick = context.meter_map.tick_from_bar_beat(position.measure, position.beat);
        if (!tick.has_value()) {
            context.timing_panel.input_error = "Bar is outside the current meter layout.";
            return result;
        }
        updated.tick = *tick;
        updated.numerator = numerator;
        updated.denominator = denominator;
    }

    if (updated.type == timing_event_type::bpm &&
        context.state.data().timing_events[index].tick == 0 &&
        updated.tick != 0) {
        context.timing_panel.input_error = "The BPM event at tick 0 must stay at tick 0.";
        return result;
    }

    if (!context.state.modify_timing_event(index, updated)) {
        context.timing_panel.input_error = "Failed to update the timing event.";
        return result;
    }

    result.success = true;
    result.scroll_to_tick = updated.tick;
    return result;
}

editor_timing_edit_result editor_timing_edit_service::apply_selected_scroll(editor_timing_edit_context context) {
    editor_timing_edit_result result;
    if (!context.timing_panel.selected_scroll_event_index.has_value()) {
        context.timing_panel.input_error = "Select a scroll automation point first.";
        return result;
    }

    const size_t index = *context.timing_panel.selected_scroll_event_index;
    if (index >= context.state.data().scroll_automation.size()) {
        context.timing_panel.input_error = "Selected scroll automation point is out of range.";
        return result;
    }

    scroll_automation_point updated = context.state.data().scroll_automation[index];
    editor_meter_map::bar_beat_position position;
    if (!try_parse_bar_beat(context.timing_panel.inputs.scroll_start_bar.value, position)) {
        context.timing_panel.input_error = "Start must be in M:B format.";
        return result;
    }

    const std::optional<int> tick = context.meter_map.tick_from_bar_beat(position.measure, position.beat);
    if (!tick.has_value()) {
        context.timing_panel.input_error = "Start is outside the current meter layout.";
        return result;
    }

    updated.tick = *tick;
    float multiplier = 0.0f;
    if (!try_parse_float(context.timing_panel.inputs.scroll_multiplier.value, multiplier) || multiplier < 0.0f) {
        context.timing_panel.input_error = "Rate must be zero or greater.";
        return result;
    }
    updated.multiplier = multiplier;

    if (!context.state.modify_scroll_automation_point(index, updated)) {
        context.timing_panel.input_error = "Failed to update the scroll automation point.";
        return result;
    }

    result.success = true;
    result.scroll_to_tick = updated.tick;
    return result;
}

editor_timing_edit_result editor_timing_edit_service::add_event(editor_timing_edit_context context, timing_event_type type) {
    editor_timing_edit_result result;
    timing_event event;
    event.type = type;
    event.tick = context.default_timing_event_tick;
    if (type == timing_event_type::bpm) {
        event.bpm = context.state.engine().get_bpm_at(event.tick);
        event.numerator = 4;
        event.denominator = 4;
    } else {
        const editor_meter_map::bar_beat_position position = context.meter_map.bar_beat_at_tick(event.tick);
        const std::optional<int> snapped_tick = context.meter_map.tick_from_bar_beat(position.measure, 1);
        event.tick = snapped_tick.value_or(event.tick);
        event.bpm = 0.0f;
        event.numerator = 4;
        event.denominator = 4;
    }

    context.state.add_timing_event(event);
    result.success = true;
    result.selected_event_index = context.state.data().timing_events.size() - 1;
    return result;
}

editor_timing_edit_result editor_timing_edit_service::add_scroll_event(editor_timing_edit_context context) {
    editor_timing_edit_result result;
    scroll_automation_point point;
    point.tick = context.default_timing_event_tick;
    point.multiplier = 1.0f;
    point.curve_to_next = scroll_automation_curve::linear;

    if (context.state.add_scroll_automation_point(point)) {
        result.success = true;
        result.selected_scroll_event_index = context.state.data().scroll_automation.size() - 1;
        result.scroll_to_tick = point.tick;
    }
    return result;
}

editor_timing_edit_result editor_timing_edit_service::delete_selected(editor_timing_edit_context context) {
    editor_timing_edit_result result;
    if (!context.timing_panel.selected_event_index.has_value()) {
        context.timing_panel.input_error = "Select a timing event first.";
        return result;
    }
    if (!can_delete_selected({context.state, context.timing_panel})) {
        context.timing_panel.input_error = "The BPM event at tick 0 cannot be deleted.";
        return result;
    }

    const size_t index = *context.timing_panel.selected_event_index;
    if (!context.state.remove_timing_event(index)) {
        context.timing_panel.input_error = "Failed to delete the timing event.";
        return result;
    }

    result.success = true;
    return result;
}

editor_timing_edit_result editor_timing_edit_service::delete_selected_scroll(editor_timing_edit_context context) {
    editor_timing_edit_result result;
    if (!context.timing_panel.selected_scroll_event_index.has_value()) {
        context.timing_panel.input_error = "Select a scroll automation point first.";
        return result;
    }

    const size_t index = *context.timing_panel.selected_scroll_event_index;
    if (!context.state.remove_scroll_automation_point(index)) {
        context.timing_panel.input_error = "Failed to delete the scroll automation point.";
        return result;
    }

    result.success = true;
    return result;
}
