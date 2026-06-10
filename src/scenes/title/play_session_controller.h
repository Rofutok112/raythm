#pragma once

#include "scene_manager.h"
#include "song_select/song_select_state.h"
#include "title/title_audio_controller.h"

namespace title_play_session {

bool start_selected_chart(scene_manager& manager,
                          song_select::state& state,
                          title_audio_controller& audio_controller);

}  // namespace title_play_session
