#pragma once

#include "multiplayer/multiplayer_state.h"
#include "multiplayer/multiplayer_view_result.h"

namespace multiplayer {

struct update_result {
    bool back_requested = false;
    bool open_song_select_requested = false;
    std::string requested_profile_user_id;
};

void on_enter(state& state);
void on_enter(state& state, const std::string& preferred_room_id);
void on_enter(state& state, const std::string& preferred_room_id, const std::string& invite_id);
update_result update(state& state, float dt);
void apply_view_result(state& state, const view::draw_result& result);
void leave_current_room_best_effort(state& state);

}  // namespace multiplayer
