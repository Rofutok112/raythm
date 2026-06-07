#pragma once

#include "play_session_types.h"

class play_hitsound_service final {
public:
    static float pan_for_event(const judge_event& event, int key_count, float pan_strength);
    static void play(const play_hitsound_paths& hitsounds, const judge_event& event, int key_count, float pan_strength);
};
