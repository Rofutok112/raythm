#include "network/friend_client_parser.h"

#include <optional>
#include <string>
#include <utility>

#include "network/json_helpers.h"

namespace {
namespace json = network::json;

friend_client::social_user parse_social_user(const std::string& object) {
    friend_client::social_user user;
    user.id = json::extract_string(object, "id").value_or("");
    user.display_name = json::extract_string(object, "displayName").value_or("");
    user.avatar_url = json::extract_string(object, "avatarUrl").value_or("");
    user.relationship_status = json::extract_string(object, "relationshipStatus").value_or("");
    user.online_status = json::extract_string(object, "onlineStatus").value_or("offline");
    if (const std::optional<std::string> current_room = json::extract_object(object, "currentRoom"); current_room.has_value()) {
        user.current_room_id = json::extract_string(*current_room, "id").value_or("");
        user.current_room_name = json::extract_string(*current_room, "name").value_or("");
    }
    return user;
}

std::optional<friend_client::friend_request> parse_friend_request(const std::string& object) {
    const std::optional<std::string> id = json::extract_string(object, "id");
    if (!id.has_value()) {
        return std::nullopt;
    }

    friend_client::friend_request request;
    request.id = *id;
    request.direction = json::extract_string(object, "direction").value_or("");
    request.status = json::extract_string(object, "status").value_or("");
    request.created_at = json::extract_string(object, "createdAt").value_or("");
    request.updated_at = json::extract_string(object, "updatedAt").value_or("");
    if (const std::optional<std::string> requester = json::extract_object(object, "requester"); requester.has_value()) {
        request.requester = parse_social_user(*requester);
    }
    if (const std::optional<std::string> addressee = json::extract_object(object, "addressee"); addressee.has_value()) {
        request.addressee = parse_social_user(*addressee);
    }
    return request;
}

std::optional<friend_client::room_invite> parse_room_invite(const std::string& object) {
    const std::optional<std::string> id = json::extract_string(object, "id");
    if (!id.has_value()) {
        return std::nullopt;
    }

    friend_client::room_invite invite;
    invite.id = *id;
    invite.room_id = json::extract_string(object, "roomId").value_or("");
    invite.room_name = json::extract_string(object, "roomName").value_or("");
    invite.status = json::extract_string(object, "status").value_or("");
    invite.read = json::extract_bool(object, "read").value_or(false);
    invite.expires_at = json::extract_string(object, "expiresAt").value_or("");
    invite.created_at = json::extract_string(object, "createdAt").value_or("");
    if (const std::optional<std::string> sender = json::extract_object(object, "sender"); sender.has_value()) {
        invite.sender = parse_social_user(*sender);
    }
    return invite;
}

}  // namespace

namespace friend_client::detail {

social_realtime_event parse_social_event_message(const std::string& message) {
    social_realtime_event event;
    event.type = json::extract_string(message, "type").value_or("");
    const std::optional<std::string> payload = json::extract_object(message, "payload");
    if (!payload.has_value()) {
        return event;
    }
    event.message = json::extract_string(*payload, "message").value_or("");
    bool parsed_friend_snapshot = false;
    if (const std::optional<std::string> friends = json::extract_array(*payload, "friends"); friends.has_value()) {
        parsed_friend_snapshot = true;
        for (const std::string& object : json::extract_objects_from_array(*friends)) {
            social_presence presence;
            presence.user_id = json::extract_string(object, "userId").value_or("");
            presence.online_status = json::extract_string(object, "onlineStatus").value_or("offline");
            if (!presence.user_id.empty()) {
                event.presence.push_back(std::move(presence));
            }
        }
    }
    if (!parsed_friend_snapshot) {
        const std::optional<std::string> user_id = json::extract_string(*payload, "userId");
        if (user_id.has_value()) {
            event.presence.push_back({
                .user_id = *user_id,
                .online_status = json::extract_string(*payload, "onlineStatus").value_or("offline"),
            });
        }
    }
    if (const std::optional<std::string> invite = json::extract_object(*payload, "invite"); invite.has_value()) {
        event.invite = parse_room_invite(*invite);
    }
    return event;
}

std::optional<friend_listing_result> parse_friend_listing_response(const network::http::response& response) {
    friend_listing_result result;
    result.listing = friend_listing{};
    result.listing->pending_request_count = json::extract_int(response.body, "pendingRequestCount").value_or(0);
    result.listing->unread_invite_count = json::extract_int(response.body, "unreadInviteCount").value_or(0);
    if (const std::optional<std::string> friends = json::extract_array(response.body, "friends"); friends.has_value()) {
        for (const std::string& object : json::extract_objects_from_array(*friends)) {
            result.listing->friends.push_back(parse_social_user(object));
        }
    }
    return result;
}

std::optional<request_listing_result> parse_request_listing_response(const network::http::response& response) {
    request_listing_result result;
    result.listing = request_listing{};
    if (const std::optional<std::string> incoming = json::extract_array(response.body, "incoming"); incoming.has_value()) {
        for (const std::string& object : json::extract_objects_from_array(*incoming)) {
            if (const auto request = parse_friend_request(object); request.has_value()) {
                result.listing->incoming.push_back(*request);
            }
        }
    }
    if (const std::optional<std::string> outgoing = json::extract_array(response.body, "outgoing"); outgoing.has_value()) {
        for (const std::string& object : json::extract_objects_from_array(*outgoing)) {
            if (const auto request = parse_friend_request(object); request.has_value()) {
                result.listing->outgoing.push_back(*request);
            }
        }
    }
    return result;
}

std::optional<invite_listing_result> parse_invite_listing_response(const network::http::response& response) {
    invite_listing_result result;
    result.listing = invite_listing{};
    if (const std::optional<std::string> invites = json::extract_array(response.body, "invites"); invites.has_value()) {
        for (const std::string& object : json::extract_objects_from_array(*invites)) {
            if (const auto invite = parse_room_invite(object); invite.has_value()) {
                result.listing->invites.push_back(*invite);
            }
        }
    }
    return result;
}

std::optional<request_operation_result> parse_request_operation_response(const network::http::response& response) {
    request_operation_result result;
    if (const std::optional<std::string> request_object = json::extract_object(response.body, "request"); request_object.has_value()) {
        result.request = parse_friend_request(*request_object);
    }
    return result;
}

std::optional<invite_operation_result> parse_invite_operation_response(const network::http::response& response) {
    invite_operation_result result;
    if (const std::optional<std::string> invite_object = json::extract_object(response.body, "invite"); invite_object.has_value()) {
        result.invite = parse_room_invite(*invite_object);
    }
    if (const std::optional<std::string> join_object = json::extract_object(response.body, "join"); join_object.has_value()) {
        result.join_room_id = json::extract_string(*join_object, "roomId").value_or("");
        result.join_invite_id = json::extract_string(*join_object, "inviteId").value_or("");
    }
    return result;
}

}  // namespace friend_client::detail
