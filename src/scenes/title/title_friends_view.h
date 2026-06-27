#pragma once

#include <string>

#include "network/friend_client.h"
#include "raylib.h"
#include "title/title_friends_state.h"
#include "ui_draw.h"

namespace title_friends_view {

enum class tab {
    friends,
    requests,
    invites,
};

enum class command_type {
    none,
    close,
    refresh,
    select_tab,
    open_profile,
    accept_request,
    decline_request,
    remove_friend,
    block_user,
    accept_invite,
    decline_invite,
    mark_invite_read,
};

struct command {
    command_type type = command_type::none;
    tab selected_tab = tab::friends;
    std::string id;
};

struct model {
    float open_anim = 0.0f;
    bool loading = false;
    bool loaded_once = false;
    bool operation_active = false;
    std::string avatar_base_url;
    tab selected_tab = tab::friends;
    title_friends_state::social_state social;
};

[[nodiscard]] command handle_input(const model& state, ui::draw_layer layer = ui::draw_layer::modal);
void draw(const model& state, ui::draw_layer layer = ui::draw_layer::modal, bool draw_backdrop = true);
[[nodiscard]] Rectangle modal_bounds(float open_anim);

}  // namespace title_friends_view
