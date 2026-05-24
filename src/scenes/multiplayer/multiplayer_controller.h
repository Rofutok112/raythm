#pragma once

#include "multiplayer/multiplayer_state.h"

namespace multiplayer {

struct update_result {
    bool back_requested = false;
    bool open_song_select_requested = false;
};

void on_enter(state& state);
void on_enter(state& state, const std::string& preferred_room_id);
update_result update(state& state, float dt);
void leave_current_room_best_effort(state& state);

}  // namespace multiplayer
