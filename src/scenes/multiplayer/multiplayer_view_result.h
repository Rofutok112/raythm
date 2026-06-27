#pragma once

#include <string>

#include "multiplayer/multiplayer_state.h"

namespace multiplayer::view {

struct draw_result {
    ui_command command = ui_command::none;

    bool room_selected = false;
    std::string selected_room_id;
    bool selected_room_requires_password = false;

    std::string selected_profile_user_id;
    std::string selected_queue_item_id;
    std::string selected_invite_user_id;

    bool queue_scroll_changed = false;
    float queue_scroll_y = 0.0f;
    float queue_scroll_y_target = 0.0f;

    bool queue_preview_seek_requested = false;
    double queue_preview_seek_seconds = 0.0;

    bool queue_download_requested = false;
    std::string download_song_id;
    std::string download_chart_id;

    bool create_max_players_changed = false;
    int create_max_players = 4;
    bool toggle_create_host_only = false;
};

}  // namespace multiplayer::view
