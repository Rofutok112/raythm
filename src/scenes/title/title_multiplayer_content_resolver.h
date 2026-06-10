#pragma once

#include <string>

#include "multiplayer/multiplayer_state.h"
#include "song_select/song_select_state.h"
#include "title/local_content_index.h"

namespace title {

struct local_chart_match {
    const song_select::song_entry* song = nullptr;
    const song_select::chart_option* chart = nullptr;
};

[[nodiscard]] local_chart_match find_online_chart_match(const song_select::state& state,
                                                        const local_content_index::snapshot& index,
                                                        const std::string& server_url,
                                                        const std::string& remote_song_id,
                                                        const std::string& remote_chart_id,
                                                        int remote_chart_version = 0);

void prepare_multiplayer_queue_state(multiplayer::state& multiplayer_state,
                                     const song_select::state& play_state,
                                     const local_content_index::snapshot& multiplayer_local_index,
                                     bool& queue_selected_chart_on_multiplayer_return);

}  // namespace title
