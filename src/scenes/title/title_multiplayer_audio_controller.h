#pragma once

#include <future>
#include <optional>
#include <string>

#include "multiplayer/multiplayer_state.h"
#include "song_select/song_select_state.h"
#include "title/local_content_index.h"
#include "title/title_audio_controller.h"

namespace title {

struct multiplayer_audio_state {
    std::string preview_song_id;
    std::string remote_preview_key;
    std::optional<song_select::song_entry> remote_preview_song;
    std::future<std::optional<song_select::song_entry>> remote_preview_future;
};

void reset_multiplayer_audio(multiplayer_audio_state& audio_state);

void update_multiplayer_audio(multiplayer_audio_state& audio_state,
                              multiplayer::state& multiplayer_state,
                              const song_select::state& play_state,
                              const local_content_index::snapshot& multiplayer_local_index,
                              title_audio_controller& audio_controller,
                              float dt);

}  // namespace title
