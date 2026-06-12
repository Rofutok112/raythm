#include <cstdlib>
#include <iostream>
#include <string>

#include "title/title_friends_state.h"

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

friend_client::social_user social_user(std::string id, std::string name, std::string status = "offline") {
    return {
        .id = std::move(id),
        .display_name = std::move(name),
        .relationship_status = "accepted",
        .online_status = std::move(status),
    };
}

friend_client::room_invite room_invite(std::string id, std::string room_name, std::string sender_name) {
    return {
        .id = std::move(id),
        .room_id = "room-1",
        .room_name = std::move(room_name),
        .sender = social_user("sender-1", std::move(sender_name), "online"),
        .status = "pending",
    };
}

}  // namespace

int main() {
    bool ok = true;

    title_friends_state::social_state state;
    state.friends.pending_request_count = 2;
    state.friends.unread_invite_count = 1;
    state.friends.friends.push_back(social_user("friend-a", "Aki"));
    state.friends.friends.push_back(social_user("friend-b", "Beni"));

    expect(title_friends_state::unread_badge_count(state) == 3,
           "Badge count should include pending requests and unread invites.", ok);

    title_friends_state::apply_presence(state, {
        .user_id = "friend-a",
        .online_status = "online",
    });
    expect(state.friends.friends[0].online_status == "online",
           "Presence should update the matching friend.", ok);

    title_friends_state::apply_presence(state, {
        .user_id = "friend-b",
        .online_status = "",
    });
    expect(state.friends.friends[1].online_status == "offline",
           "Empty presence status should normalize to offline.", ok);

    title_friends_state::apply_presence(state, {
        .user_id = "stranger",
        .online_status = "online",
    });
    expect(state.friends.friends.size() == 2,
           "Presence for a non-friend should not insert a user.", ok);

    friend_client::social_realtime_event first_invite;
    first_invite.type = "social.room_invite_created";
    first_invite.invite = room_invite("invite-1", "Practice", "Dai");
    const title_friends_state::event_apply_result first_result =
        title_friends_state::apply_social_event(state, first_invite);
    expect(first_result.invite_inserted, "New invite should report inserted.", ok);
    expect(!first_result.invite_updated, "New invite should not report updated.", ok);
    expect(state.invites.invites.size() == 1, "New invite should be inserted.", ok);
    expect(state.invites.invites.front().room_name == "Practice",
           "Inserted invite should keep room details.", ok);
    expect(state.friends.unread_invite_count == 2,
           "New invite should increment unread invite count.", ok);

    friend_client::social_realtime_event duplicate_invite;
    duplicate_invite.type = "social.room_invite_created";
    duplicate_invite.invite = room_invite("invite-1", "Encore", "Dai");
    const title_friends_state::event_apply_result duplicate_result =
        title_friends_state::apply_social_event(state, duplicate_invite);
    expect(!duplicate_result.invite_inserted, "Duplicate invite should not report inserted.", ok);
    expect(duplicate_result.invite_updated, "Duplicate invite should report updated.", ok);
    expect(state.invites.invites.size() == 1, "Duplicate invite should not add a row.", ok);
    expect(state.invites.invites.front().room_name == "Encore",
           "Duplicate invite should refresh existing invite details.", ok);
    expect(state.friends.unread_invite_count == 2,
           "Duplicate invite should not increment unread invite count.", ok);

    friend_client::social_realtime_event cancelled_unknown_invite;
    cancelled_unknown_invite.type = "social.room_invite_created";
    cancelled_unknown_invite.invite = room_invite("invite-unknown", "Ghost", "Dai");
    cancelled_unknown_invite.invite->status = "cancelled";
    const title_friends_state::event_apply_result cancelled_unknown_result =
        title_friends_state::apply_social_event(state, cancelled_unknown_invite);
    expect(!cancelled_unknown_result.invite_inserted &&
               !cancelled_unknown_result.invite_updated &&
               !cancelled_unknown_result.invite_removed,
           "Unknown inactive invite should not mutate invite state.", ok);
    expect(state.invites.invites.size() == 1,
           "Unknown inactive invite should not be inserted.", ok);
    expect(state.friends.unread_invite_count == 2,
           "Unknown inactive invite should not increment unread invite count.", ok);

    friend_client::social_realtime_event cancelled_existing_invite;
    cancelled_existing_invite.type = "social.room_invite_created";
    cancelled_existing_invite.invite = room_invite("invite-1", "Encore", "Dai");
    cancelled_existing_invite.invite->status = "cancelled";
    const title_friends_state::event_apply_result cancelled_existing_result =
        title_friends_state::apply_social_event(state, cancelled_existing_invite);
    expect(cancelled_existing_result.invite_removed,
           "Inactive update for an existing invite should report removed.", ok);
    expect(state.invites.invites.empty(),
           "Inactive update for an existing invite should remove it from visible invites.", ok);
    expect(state.friends.unread_invite_count == 2,
           "Removing an inactive invite should not change unread invite count.", ok);

    expect(title_friends_state::is_active_invite_status("pending"), "Pending invite should be active.", ok);
    expect(title_friends_state::is_active_invite_status("accepted"), "Accepted invite should be active.", ok);
    expect(!title_friends_state::is_active_invite_status("declined"), "Declined invite should be inactive.", ok);
    expect(!title_friends_state::is_active_invite_status("expired"), "Expired invite should be inactive.", ok);
    expect(!title_friends_state::is_active_invite_status("cancelled"), "Cancelled invite should be inactive.", ok);

    friend_client::social_realtime_event mixed_event;
    mixed_event.type = "social.presence_changed";
    mixed_event.presence.push_back({
        .user_id = "friend-a",
        .online_status = "away",
    });
    const title_friends_state::event_apply_result mixed_result =
        title_friends_state::apply_social_event(state, mixed_event);
    expect(!mixed_result.invite_inserted && !mixed_result.invite_updated,
           "Presence-only event should not report invite changes.", ok);
    expect(state.friends.friends[0].online_status == "away",
           "Social event should apply presence changes.", ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "title_friends_state smoke test passed\n";
    return EXIT_SUCCESS;
}
