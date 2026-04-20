#pragma once

#include <optional>
#include <string>

#include "scene_manager.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"

namespace title_play_session {

void reload_catalog(song_select::state& state,
                    song_select::preview_controller& preview_controller,
                    const std::string& preferred_song_id = "",
                    const std::string& preferred_chart_id = "",
                    bool sync_media_now = false);
void warm_scoring_ruleset();

void sync_media(song_select::state& state, song_select::preview_controller& preview_controller);
void reload_ranking(song_select::state& state);
bool start_selected_chart(scene_manager& manager,
                          song_select::state& state,
                          song_select::preview_controller& preview_controller);

}  // namespace title_play_session
