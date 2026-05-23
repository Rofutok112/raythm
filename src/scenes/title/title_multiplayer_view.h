#pragma once

#include <future>
#include <optional>
#include <string>
#include <vector>

#include "network/multiplayer_client.h"
#include "raylib.h"
#include "song_select/song_select_state.h"
#include "title/multiplayer_playable_catalog.h"
#include "ui_text_input.h"

namespace title_multiplayer_view {

enum class screen {
    browser,
    password_prompt,
    setup,
    room,
};

struct state {
    screen current_screen = screen::browser;
    std::vector<multiplayer_client::room_state> rooms;
    title_multiplayer_content::multiplayer_playable_catalog playable_catalog;
    std::optional<multiplayer_client::room_state> current_room;
    ui::text_input_state join_password_input;
    ui::text_input_state setup_password_input;
    std::future<multiplayer_client::operation_result> pending;
    bool loading = false;
    std::string loading_label;
    std::string status_message;
    bool entered = false;
    size_t online_content_local_song_count = 0;
    int selected_room_index = 0;
    int selected_song_index = 0;
    int selected_chart_index = 0;
    bool private_room = false;
    std::string play_launch_room_id;
};

struct update_result {
    bool back_requested = false;
};

void on_enter(state& state, const song_select::state& play_state);
update_result update(state& state,
                     const song_select::state& play_state,
                     float play_view_anim,
                     Rectangle entry_origin_rect,
                     float dt);
void draw(state& state,
          const song_select::state& play_state,
          float play_view_anim,
          Rectangle entry_origin_rect);

}  // namespace title_multiplayer_view
