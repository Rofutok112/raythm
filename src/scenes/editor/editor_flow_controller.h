#pragma once

#include "editor_scene_types.h"

class editor_flow_controller final {
public:
    static editor_flow_result update(const editor_flow_context& context);
};
