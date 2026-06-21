#pragma once

#include <string>

#include "mv/composition/mv_composition.h"

namespace mv::composition {

struct cue_authoring_result {
    bool changed = false;
    bool created = false;
    std::string effect_id;
};

cue_authoring_result add_flash_cue(layer& target,
                                   const mv_composition& composition,
                                   double time_ms,
                                   double tolerance_ms);
cue_authoring_result add_show_cue(layer& target, double time_ms, double tolerance_ms);
cue_authoring_result add_text_cue(layer& target,
                                  double time_ms,
                                  double tolerance_ms,
                                  const std::string& text);
int count_timeline_cues_near(const layer& target, double time_ms, double tolerance_ms);
int clear_timeline_cues_near(layer& target, double time_ms, double tolerance_ms);

}  // namespace mv::composition
