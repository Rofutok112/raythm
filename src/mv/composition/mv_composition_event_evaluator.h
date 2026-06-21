#pragma once

#include <string>

#include "mv/composition/mv_composition.h"

namespace mv::composition {

struct event_evaluation_result {
    int matched_triggers = 0;
    int applied_actions = 0;
};

event_evaluation_result apply_event(mv_composition& composition,
                                    const std::string& event_name,
                                    double event_time_ms);
event_evaluation_result apply_timeline_events(mv_composition& composition,
                                              double previous_time_ms,
                                              double current_time_ms);

}  // namespace mv::composition
