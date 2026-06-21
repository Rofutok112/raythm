#include "mv/composition/mv_composition_event_authoring.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace mv::composition {

namespace {

std::string next_effect_id(const mv_composition& composition, const std::string& prefix) {
    for (int index = 1; index < 10000; ++index) {
        const std::string id = prefix + "-" + std::to_string(index);
        bool exists = false;
        for (const layer& current_layer : composition.layers) {
            exists = std::any_of(current_layer.effects.begin(), current_layer.effects.end(), [&](const auto& effect) {
                return effect.id == id;
            });
            if (exists) {
                break;
            }
        }
        if (!exists) {
            return id;
        }
    }
    return prefix + "-fallback";
}

bool near_time(double left_ms, double right_ms, double tolerance_ms) {
    return left_ms >= 0.0 && std::abs(left_ms - right_ms) <= tolerance_ms;
}

std::vector<event_action> single_action(event_action action) {
    std::vector<event_action> actions;
    actions.push_back(std::move(action));
    return actions;
}

cue_authoring_result upsert_cue(layer& target,
                                const std::string& event_name,
                                double time_ms,
                                double tolerance_ms,
                                std::vector<event_action> actions) {
    auto trigger_it = std::find_if(target.event_triggers.begin(), target.event_triggers.end(),
                                   [&](const auto& trigger) {
                                       return trigger.event == event_name &&
                                              near_time(trigger.time_ms, time_ms, tolerance_ms);
                                   });
    if (trigger_it == target.event_triggers.end()) {
        event_trigger trigger;
        trigger.event = event_name;
        trigger.time_ms = time_ms;
        trigger.actions = std::move(actions);
        target.event_triggers.push_back(std::move(trigger));
        return {.changed = true, .created = true};
    }
    trigger_it->time_ms = time_ms;
    trigger_it->actions = std::move(actions);
    return {.changed = true, .created = false};
}

}  // namespace

cue_authoring_result add_flash_cue(layer& target,
                                   const mv_composition& composition,
                                   double time_ms,
                                   double tolerance_ms) {
    auto effect_it = std::find_if(target.effects.begin(), target.effects.end(), [](const auto& effect) {
        return effect.type == "flash";
    });
    if (effect_it == target.effects.end()) {
        effect effect_data;
        effect_data.id = next_effect_id(composition, "fx-flash");
        effect_data.type = "flash";
        effect_data.target = "transform.opacity";
        effect_data.amount = 0.35f;
        target.effects.push_back(std::move(effect_data));
        effect_it = std::prev(target.effects.end());
    }

    event_action action;
    action.type = "triggerEffect";
    action.effect_id = effect_it->id;
    cue_authoring_result result = upsert_cue(target, "mv.flash", time_ms, tolerance_ms, single_action(action));
    result.effect_id = effect_it->id;
    return result;
}

cue_authoring_result add_show_cue(layer& target, double time_ms, double tolerance_ms) {
    event_action action;
    action.type = "showLayer";
    return upsert_cue(target, "mv.show", time_ms, tolerance_ms, single_action(action));
}

cue_authoring_result add_text_cue(layer& target,
                                  double time_ms,
                                  double tolerance_ms,
                                  const std::string& text) {
    event_action action;
    action.type = "setText";
    action.value = text.empty() ? "Text" : text;
    cue_authoring_result result = upsert_cue(target, "text.lyric", time_ms, tolerance_ms, single_action(action));
    target.source_data.text = action.value;
    return result;
}

int count_timeline_cues_near(const layer& target, double time_ms, double tolerance_ms) {
    return static_cast<int>(std::count_if(target.event_triggers.begin(), target.event_triggers.end(),
                                          [&](const auto& trigger) {
                                              return near_time(trigger.time_ms, time_ms, tolerance_ms);
                                          }));
}

int clear_timeline_cues_near(layer& target, double time_ms, double tolerance_ms) {
    const auto old_size = target.event_triggers.size();
    target.event_triggers.erase(
        std::remove_if(target.event_triggers.begin(), target.event_triggers.end(), [&](const auto& trigger) {
            return near_time(trigger.time_ms, time_ms, tolerance_ms);
        }),
        target.event_triggers.end());
    return static_cast<int>(old_size - target.event_triggers.size());
}

}  // namespace mv::composition
