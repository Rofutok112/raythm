#include <cstdlib>
#include <iostream>
#include <string>

#include "network/friend_client_parser.h"

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

network::http::response response_with_body(std::string body) {
    return {
        .status_code = 200,
        .body = std::move(body),
    };
}

}  // namespace

int main() {
    bool ok = true;

    const auto friends = friend_client::detail::parse_friend_listing_response(response_with_body(
        "{"
        "\"pendingRequestCount\":2,"
        "\"unreadInviteCount\":1,"
        "\"friends\":[{"
        "\"id\":\"user-a\","
        "\"displayName\":\"Aki\","
        "\"avatarUrl\":\"https://example.test/a.png\","
        "\"relationshipStatus\":\"accepted\","
        "\"onlineStatus\":\"in_room\","
        "\"currentRoom\":{\"id\":\"room-1\",\"name\":\"Warmup\"}"
        "}]"
        "}"));
    expect(friends.has_value(), "Friend listing should parse.", ok);
    expect(friends->listing.has_value(), "Friend listing payload should be present.", ok);
    expect(friends->listing->pending_request_count == 2, "Pending request count should parse.", ok);
    expect(friends->listing->unread_invite_count == 1, "Unread invite count should parse.", ok);
    expect(friends->listing->friends.size() == 1, "Friend array should parse one user.", ok);
    if (!friends->listing->friends.empty()) {
        const friend_client::social_user& user = friends->listing->friends.front();
        expect(user.id == "user-a", "Friend id should parse.", ok);
        expect(user.display_name == "Aki", "Friend display name should parse.", ok);
        expect(user.relationship_status == "accepted", "Friend relationship should parse.", ok);
        expect(user.online_status == "in_room", "Friend presence should parse.", ok);
        expect(user.current_room_id == "room-1", "Friend current room id should parse.", ok);
        expect(user.current_room_name == "Warmup", "Friend current room name should parse.", ok);
    }

    const auto requests = friend_client::detail::parse_request_listing_response(response_with_body(
        "{"
        "\"incoming\":[{"
        "\"id\":\"friendship-1\","
        "\"direction\":\"incoming\","
        "\"status\":\"pending\","
        "\"requester\":{\"id\":\"user-b\",\"displayName\":\"Beni\"},"
        "\"addressee\":{\"id\":\"user-me\",\"displayName\":\"Me\"}"
        "}],"
        "\"outgoing\":[{"
        "\"id\":\"friendship-2\","
        "\"direction\":\"outgoing\","
        "\"status\":\"pending\","
        "\"requester\":{\"id\":\"user-me\",\"displayName\":\"Me\"},"
        "\"addressee\":{\"id\":\"user-c\",\"displayName\":\"Chika\"}"
        "}]"
        "}"));
    expect(requests.has_value(), "Friend request listing should parse.", ok);
    expect(requests->listing->incoming.size() == 1, "Incoming request should parse.", ok);
    expect(requests->listing->outgoing.size() == 1, "Outgoing request should parse.", ok);
    expect(requests->listing->incoming.front().requester.display_name == "Beni",
           "Incoming requester display name should parse.", ok);
    expect(requests->listing->outgoing.front().addressee.id == "user-c",
           "Outgoing addressee id should parse.", ok);

    const auto invite_accept = friend_client::detail::parse_invite_operation_response(response_with_body(
        "{"
        "\"invite\":{"
        "\"id\":\"invite-1\","
        "\"roomId\":\"room-2\","
        "\"roomName\":\"Private Practice\","
        "\"status\":\"accepted\","
        "\"read\":true,"
        "\"expiresAt\":\"2026-06-12T01:00:00Z\","
        "\"createdAt\":\"2026-06-12T00:30:00Z\","
        "\"sender\":{\"id\":\"user-d\",\"displayName\":\"Dai\"}"
        "},"
        "\"join\":{\"roomId\":\"room-2\",\"inviteId\":\"invite-1\"}"
        "}"));
    expect(invite_accept.has_value(), "Invite operation should parse.", ok);
    expect(invite_accept->invite.has_value(), "Invite operation payload should be present.", ok);
    expect(invite_accept->invite->read, "Invite read flag should parse.", ok);
    expect(invite_accept->invite->sender.display_name == "Dai", "Invite sender should parse.", ok);
    expect(invite_accept->join_room_id == "room-2", "Invite join room id should parse.", ok);
    expect(invite_accept->join_invite_id == "invite-1", "Invite join invite id should parse.", ok);

    const friend_client::social_realtime_event snapshot = friend_client::detail::parse_social_event_message(
        "{"
        "\"type\":\"social.presence_snapshot\","
        "\"payload\":{\"friends\":["
        "{\"userId\":\"user-a\",\"onlineStatus\":\"online\"},"
        "{\"userId\":\"user-b\",\"onlineStatus\":\"away\"}"
        "]}"
        "}");
    expect(snapshot.type == "social.presence_snapshot", "Presence snapshot type should parse.", ok);
    expect(snapshot.presence.size() == 2, "Presence snapshot users should parse.", ok);
    expect(snapshot.presence.front().user_id == "user-a", "Presence user id should parse.", ok);
    expect(snapshot.presence.back().online_status == "away", "Presence online status should parse.", ok);

    const friend_client::social_realtime_event created_invite = friend_client::detail::parse_social_event_message(
        "{"
        "\"type\":\"social.room_invite_created\","
        "\"payload\":{\"invite\":{"
        "\"id\":\"invite-2\","
        "\"roomId\":\"room-3\","
        "\"roomName\":\"Encore\","
        "\"sender\":{\"id\":\"user-e\",\"displayName\":\"Ema\"}"
        "}}"
        "}");
    expect(created_invite.invite.has_value(), "Realtime invite payload should parse.", ok);
    expect(created_invite.invite->id == "invite-2", "Realtime invite id should parse.", ok);
    expect(created_invite.invite->sender.id == "user-e", "Realtime invite sender should parse.", ok);

    const friend_client::social_realtime_event cancelled_invite = friend_client::detail::parse_social_event_message(
        "{"
        "\"type\":\"social.room_invite_updated\","
        "\"payload\":{\"invite\":{\"id\":\"invite-2\",\"status\":\"cancelled\"}}"
        "}");
    expect(cancelled_invite.type == "social.room_invite_updated",
           "Realtime invite update type should parse.", ok);
    expect(cancelled_invite.invite.has_value(),
           "Realtime cancelled invite payload should parse.", ok);
    expect(cancelled_invite.invite->id == "invite-2",
           "Realtime cancelled invite id should parse.", ok);
    expect(cancelled_invite.invite->status == "cancelled",
           "Realtime cancelled invite status should parse.", ok);

    const auto malformed_request = friend_client::detail::parse_request_operation_response(response_with_body(
        "{\"request\":{\"status\":\"pending\"}}"));
    expect(malformed_request.has_value(), "Malformed request envelope should still parse.", ok);
    expect(!malformed_request->request.has_value(), "Request missing id should be ignored.", ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "friend_client_parser smoke test passed\n";
    return EXIT_SUCCESS;
}
