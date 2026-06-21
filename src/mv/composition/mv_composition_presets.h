#pragma once

#include <string>
#include <vector>

#include "mv/composition/mv_composition.h"

namespace mv::composition {

struct preset_definition {
    std::string id;
    std::string label;
    std::string description;
};

struct preset_apply_result {
    bool success = false;
    std::string selected_layer_id;
    std::vector<std::string> added_layer_ids;
    std::string message;
};

const std::vector<preset_definition>& available_presets();
preset_apply_result apply_preset(mv_composition& composition,
                                 const std::string& preset_id,
                                 double start_ms,
                                 double duration_ms);

}  // namespace mv::composition
