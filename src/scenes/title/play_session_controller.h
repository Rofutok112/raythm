#pragma once

#include "scene_manager.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"

namespace title_play_session {

void sync_preview(song_select::state& state, song_select::preview_controller& preview_controller);
bool start_selected_chart(scene_manager& manager,
                          song_select::state& state,
                          song_select::preview_controller& preview_controller);

}  // namespace title_play_session
