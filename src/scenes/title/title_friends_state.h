#pragma once

#include "network/friend_client.h"

namespace title_friends_state {

struct social_state {
    friend_client::friend_listing friends;
    friend_client::request_listing requests;
    friend_client::invite_listing invites;
};

struct event_apply_result {
    bool invite_inserted = false;
    bool invite_updated = false;
    bool invite_removed = false;
};

int unread_badge_count(const social_state& state);
bool is_active_invite_status(const std::string& status);
void apply_presence(social_state& state, const friend_client::social_presence& presence);
event_apply_result apply_social_event(social_state& state, const friend_client::social_realtime_event& event);

}  // namespace title_friends_state
