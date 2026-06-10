#pragma once

#include <optional>
#include <string>

#include "multiplayer/multiplayer_state.h"
#include "song_select/song_select_state.h"
#include "title/local_content_index.h"
#include "title/title_command.h"
#include "title/title_multiplayer_audio_controller.h"
#include "title/title_browse_feature.h"

namespace title {

struct multiplayer_play_request {
    const song_select::song_entry* song = nullptr;
    const song_select::chart_option* chart = nullptr;
    std::string room_id;
    std::string match_id;
};

struct multiplayer_flow_result {
    command title_command;
    bool enter_home = false;
    std::optional<multiplayer_play_request> play_request;
};

struct multiplayer_flow_context {
    multiplayer::state& multiplayer_state;
    const song_select::state& play_state;
    const local_content_index::snapshot& multiplayer_local_index;
    bool& queue_selected_chart_on_multiplayer_return;
    multiplayer_audio_state& audio_state;
    title_audio_controller& audio_controller;
    title_browse_feature& browse_feature;
};

[[nodiscard]] multiplayer_flow_result update_multiplayer_flow(multiplayer_flow_context& context, float dt);

}  // namespace title
