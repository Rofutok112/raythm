#include "title/title_friends_state.h"

#include <algorithm>

namespace title_friends_state {

int unread_badge_count(const social_state& state) {
    return state.friends.pending_request_count + state.friends.unread_invite_count;
}

bool is_active_invite_status(const std::string& status) {
    return status.empty() || status == "pending" || status == "accepted";
}

void apply_presence(social_state& state, const friend_client::social_presence& presence) {
    if (presence.user_id.empty()) {
        return;
    }
    for (friend_client::social_user& user : state.friends.friends) {
        if (user.id == presence.user_id) {
            user.online_status = presence.online_status.empty() ? "offline" : presence.online_status;
            break;
        }
    }
}

event_apply_result apply_social_event(social_state& state, const friend_client::social_realtime_event& event) {
    event_apply_result result;
    for (const friend_client::social_presence& presence : event.presence) {
        apply_presence(state, presence);
    }

    if (event.invite.has_value()) {
        const friend_client::room_invite& invite = *event.invite;
        const bool active = is_active_invite_status(invite.status);
        const auto existing = std::find_if(
            state.invites.invites.begin(),
            state.invites.invites.end(),
            [&invite](const friend_client::room_invite& current) {
                return current.id == invite.id;
            });
        if (existing == state.invites.invites.end()) {
            if (!active) {
                return result;
            }
            state.invites.invites.insert(state.invites.invites.begin(), invite);
            ++state.friends.unread_invite_count;
            result.invite_inserted = true;
        } else if (!active) {
            state.invites.invites.erase(existing);
            result.invite_removed = true;
        } else {
            *existing = invite;
            result.invite_updated = true;
        }
    }
    return result;
}

}  // namespace title_friends_state
