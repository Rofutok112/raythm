#pragma once

#include "../api/mv_scene.h"

#include <string>
#include <vector>

namespace mv {

struct validation_limits {
    int max_nodes = 512;
};

struct validation_result {
    bool ok = true;
    std::vector<std::string> warnings;
};

// Validate a scene before rendering. Truncates excess nodes and reports warnings.
validation_result validate_scene(scene& sc, const validation_limits& limits = {});

} // namespace mv
