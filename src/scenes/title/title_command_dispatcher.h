#pragma once

#include <functional>
#include <string>

#include "title/title_command.h"

namespace title {

struct command_dispatch_context {
    std::function<void()> enter_home;
    std::function<void(const std::string& song_id, const std::string& chart_id)> open_update_catalog;
    std::function<bool()> add_selected_to_multiplayer;
    std::function<void()> open_self_profile;
    std::function<void(const std::string& user_id)> open_public_profile;
    std::function<void()> open_multiplayer_song_select;
};

bool dispatch_command(const command_dispatch_context& context, const command& command);

}  // namespace title
