#include "mv/composition/mv_composition_event_evaluator.h"

#include <algorithm>
#include <cstdlib>

namespace mv::composition {

namespace {

float parse_float(const std::string& value, float fallback) {
    if (value.empty()) {
        return fallback;
    }
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    return end != nullptr && *end == '\0' ? parsed : fallback;
}

bool parse_bool(const std::string& value, bool fallback) {
    if (value == "true" || value == "1" || value == "yes") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no") {
        return false;
    }
    return fallback;
}

effect* find_effect(layer& target, const std::string& effect_id) {
    const auto it = std::find_if(target.effects.begin(), target.effects.end(), [&](const effect& current) {
        return current.id == effect_id;
    });
    return it == target.effects.end() ? nullptr : &*it;
}

bool apply_set_property(layer& target, const event_action& action) {
    if (action.property == "visible") {
        target.visible = parse_bool(action.value, target.visible);
        return true;
    }
    if (action.property == "source.text" || action.property == "text") {
        target.source_data.text = action.value;
        return true;
    }
    if (action.property == "source.fill" || action.property == "fill") {
        target.source_data.fill = action.value;
        return true;
    }
    if (action.property == "transform.opacity" || action.property == "opacity") {
        target.transform_data.opacity = std::clamp(parse_float(action.value, target.transform_data.opacity), 0.0f, 1.0f);
        return true;
    }
    if (action.property == "transform.position.x") {
        target.transform_data.position_x = parse_float(action.value, target.transform_data.position_x);
        return true;
    }
    if (action.property == "transform.position.y") {
        target.transform_data.position_y = parse_float(action.value, target.transform_data.position_y);
        return true;
    }
    if (action.property == "transform.scale.x") {
        target.transform_data.scale_x = parse_float(action.value, target.transform_data.scale_x);
        return true;
    }
    if (action.property == "transform.scale.y") {
        target.transform_data.scale_y = parse_float(action.value, target.transform_data.scale_y);
        return true;
    }
    if (action.property == "transform.rotationDeg") {
        target.transform_data.rotation_deg = parse_float(action.value, target.transform_data.rotation_deg);
        return true;
    }
    return false;
}

bool apply_action(layer& target, const event_action& action, double event_time_ms) {
    if (action.type == "showLayer") {
        target.visible = true;
        return true;
    }
    if (action.type == "hideLayer") {
        target.visible = false;
        return true;
    }
    if (action.type == "setText") {
        target.source_data.text = action.value;
        return true;
    }
    if (action.type == "setProperty") {
        return apply_set_property(target, action);
    }
    if (action.type == "triggerEffect") {
        effect* target_effect = find_effect(target, action.effect_id);
        if (target_effect == nullptr) {
            return false;
        }
        target.start_ms = event_time_ms;
        if (target.duration_ms <= 0.0) {
            target.duration_ms = 1000.0;
        }
        return true;
    }
    return false;
}

event_evaluation_result apply_trigger(layer& target,
                                      const event_trigger& trigger,
                                      double event_time_ms) {
    event_evaluation_result result;
    ++result.matched_triggers;
    for (const event_action& action : trigger.actions) {
        if (apply_action(target, action, event_time_ms)) {
            ++result.applied_actions;
        }
    }
    return result;
}

void add_result(event_evaluation_result& target, const event_evaluation_result& source) {
    target.matched_triggers += source.matched_triggers;
    target.applied_actions += source.applied_actions;
}

}  // namespace

event_evaluation_result apply_event(mv_composition& composition,
                                    const std::string& event_name,
                                    double event_time_ms) {
    event_evaluation_result result;
    if (event_name.empty()) {
        return result;
    }
    for (layer& target : composition.layers) {
        for (const event_trigger& trigger : target.event_triggers) {
            if (trigger.event != event_name) {
                continue;
            }
            add_result(result, apply_trigger(target, trigger, event_time_ms));
        }
    }
    return result;
}

event_evaluation_result apply_timeline_events(mv_composition& composition,
                                              double previous_time_ms,
                                              double current_time_ms) {
    event_evaluation_result result;
    if (current_time_ms < previous_time_ms) {
        return result;
    }
    for (layer& target : composition.layers) {
        for (const event_trigger& trigger : target.event_triggers) {
            if (trigger.time_ms < 0.0) {
                continue;
            }
            if (trigger.time_ms > previous_time_ms && trigger.time_ms <= current_time_ms) {
                add_result(result, apply_trigger(target, trigger, trigger.time_ms));
            }
        }
    }
    return result;
}

}  // namespace mv::composition
