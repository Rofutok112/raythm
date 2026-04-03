#pragma once

#include "editor_scene_types.h"

class editor_transport_controller final {
public:
    static editor_transport_result sync(const editor_transport_context& context);
    static editor_transport_result toggle_playback(const editor_transport_context& context);
    static editor_transport_result seek_to_tick(const editor_transport_context& context, int tick);
};
