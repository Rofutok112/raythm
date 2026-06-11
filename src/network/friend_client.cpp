#include "network/friend_client.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "network/auth_client.h"
#include "network/http_client.h"
#include "network/json_helpers.h"
#include "network/network_error.h"

namespace {
namespace json = network::json;
using http_response = network::http::response;

network::error_classification classify_response_error(const http_response& response, std::string fallback) {
    return network::classify_http_error(response.status_code, response.body, std::move(fallback), response.retry_after);
}

template <typename Result>
void apply_error(Result& result, const network::error_classification& error) {
    result.message = error.message;
    result.maintenance = error.is_maintenance();
    result.retry_after = error.retry_after;
}

template <typename Result>
Result no_session_result(const std::string& message) {
    Result result;
    result.unauthorized = true;
    result.message = message;
    return result;
}

template <typename Result>
Result transport_error_result(const std::string& message) {
    Result result;
    result.message = message;
    return result;
}

std::optional<auth::session> load_active_session() {
    std::optional<auth::session> stored = auth::load_saved_session();
    if (stored.has_value()) {
        return stored;
    }

    const auth::operation_result restored = auth::restore_saved_session();
    if (restored.success && restored.session_data.has_value()) {
        return restored.session_data;
    }
    return std::nullopt;
}

http_response send_session_request(const auth::session& session_data,
                                   const std::string& method,
                                   const std::string& path,
                                   const std::string& body = {}) {
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + session_data.access_token},
        {"User-Agent", "raythm/0.1"},
    };
    if (!body.empty()) {
        headers.emplace_back("Content-Type", "application/json");
    }
    return network::http::send_request(method, auth::normalize_server_url(session_data.server_url) + path, headers, body);
}

template <typename Result>
Result send_with_refresh(const std::string& method,
                         const std::string& path,
                         const std::string& body,
                         const std::string& unauthorized_message,
                         const std::string& fallback_message,
                         const std::function<std::optional<Result>(const http_response&)>& parse_success) {
    std::optional<auth::session> session_data = load_active_session();
    if (!session_data.has_value()) {
        return no_session_result<Result>(unauthorized_message);
    }

    http_response response = send_session_request(*session_data, method, path, body);
    if (response.error_message.empty() && response.status_code == 401) {
        const auth::operation_result restored = auth::restore_saved_session();
        if (restored.success && restored.session_data.has_value()) {
            session_data = restored.session_data;
            response = send_session_request(*session_data, method, path, body);
        }
    }

    if (!response.error_message.empty()) {
        return transport_error_result<Result>(response.error_message);
    }
    if (response.status_code == 401) {
        return no_session_result<Result>(unauthorized_message);
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        Result result;
        apply_error(result, classify_response_error(response, fallback_message));
        return result;
    }

    std::optional<Result> parsed = parse_success(response);
    if (!parsed.has_value()) {
        Result result;
        result.message = "Server returned an unexpected friends response.";
        return result;
    }
    parsed->success = true;
    return *parsed;
}

std::string json_string_field(const std::string& value, const std::string& field) {
    return "\"" + field + "\":\"" + json::escape_string(value) + "\"";
}

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

std::optional<friend_client::friend_listing_result> parse_friend_listing(const http_response& response) {
    friend_client::friend_listing_result result;
    result.listing = friend_client::friend_listing{};
    result.listing->pending_request_count = json::extract_int(response.body, "pendingRequestCount").value_or(0);
    result.listing->unread_invite_count = json::extract_int(response.body, "unreadInviteCount").value_or(0);
    if (const std::optional<std::string> friends = json::extract_array(response.body, "friends"); friends.has_value()) {
        for (const std::string& object : json::extract_objects_from_array(*friends)) {
            result.listing->friends.push_back(parse_social_user(object));
        }
    }
    return result;
}

std::optional<friend_client::request_listing_result> parse_request_listing(const http_response& response) {
    friend_client::request_listing_result result;
    result.listing = friend_client::request_listing{};
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

std::optional<friend_client::invite_listing_result> parse_invite_listing(const http_response& response) {
    friend_client::invite_listing_result result;
    result.listing = friend_client::invite_listing{};
    if (const std::optional<std::string> invites = json::extract_array(response.body, "invites"); invites.has_value()) {
        for (const std::string& object : json::extract_objects_from_array(*invites)) {
            if (const auto invite = parse_room_invite(object); invite.has_value()) {
                result.listing->invites.push_back(*invite);
            }
        }
    }
    return result;
}

std::optional<friend_client::request_operation_result> parse_request_operation(const http_response& response) {
    friend_client::request_operation_result result;
    if (const std::optional<std::string> request_object = json::extract_object(response.body, "request"); request_object.has_value()) {
        result.request = parse_friend_request(*request_object);
    }
    return result;
}

std::optional<friend_client::invite_operation_result> parse_invite_operation(const http_response& response) {
    friend_client::invite_operation_result result;
    if (const std::optional<std::string> invite_object = json::extract_object(response.body, "invite"); invite_object.has_value()) {
        result.invite = parse_room_invite(*invite_object);
    }
    if (const std::optional<std::string> join_object = json::extract_object(response.body, "join"); join_object.has_value()) {
        result.join_room_id = json::extract_string(*join_object, "roomId").value_or("");
        result.join_invite_id = json::extract_string(*join_object, "inviteId").value_or("");
    }
    return result;
}

std::optional<friend_client::operation_result> parse_basic_operation(const http_response&) {
    return friend_client::operation_result{.success = true};
}

}  // namespace

namespace friend_client {

friend_listing_result fetch_friends() {
    return send_with_refresh<friend_listing_result>(
        "GET", "/friends", {}, "Sign in to view friends.", "Failed to load friends.", parse_friend_listing);
}

request_listing_result fetch_friend_requests() {
    return send_with_refresh<request_listing_result>(
        "GET", "/friends/requests", {}, "Sign in to view friend requests.", "Failed to load friend requests.", parse_request_listing);
}

request_operation_result send_friend_request(const std::string& target_user_id) {
    return send_with_refresh<request_operation_result>(
        "POST",
        "/friends/requests",
        "{" + json_string_field(target_user_id, "targetUserId") + "}",
        "Sign in to send friend requests.",
        "Failed to send friend request.",
        parse_request_operation);
}

request_operation_result accept_friend_request(const std::string& request_id) {
    return send_with_refresh<request_operation_result>(
        "POST", "/friends/requests/" + request_id + "/accept", {}, "Sign in to accept friend requests.", "Failed to accept friend request.", parse_request_operation);
}

request_operation_result decline_friend_request(const std::string& request_id) {
    return send_with_refresh<request_operation_result>(
        "POST", "/friends/requests/" + request_id + "/decline", {}, "Sign in to decline friend requests.", "Failed to decline friend request.", parse_request_operation);
}

operation_result remove_friend(const std::string& user_id) {
    return send_with_refresh<operation_result>(
        "DELETE", "/friends/" + user_id, {}, "Sign in to remove friends.", "Failed to remove friend.", parse_basic_operation);
}

operation_result block_user(const std::string& user_id) {
    return send_with_refresh<operation_result>(
        "POST", "/friends/" + user_id + "/block", {}, "Sign in to block users.", "Failed to block user.", parse_basic_operation);
}

operation_result unblock_user(const std::string& user_id) {
    return send_with_refresh<operation_result>(
        "DELETE", "/friends/" + user_id + "/block", {}, "Sign in to unblock users.", "Failed to unblock user.", parse_basic_operation);
}

invite_listing_result fetch_room_invites() {
    return send_with_refresh<invite_listing_result>(
        "GET", "/room-invites", {}, "Sign in to view room invites.", "Failed to load room invites.", parse_invite_listing);
}

invite_operation_result send_room_invite(const std::string& room_id, const std::string& recipient_user_id) {
    return send_with_refresh<invite_operation_result>(
        "POST",
        "/rooms/" + room_id + "/invites",
        "{" + json_string_field(recipient_user_id, "recipientUserId") + "}",
        "Sign in to invite friends.",
        "Failed to send room invite.",
        parse_invite_operation);
}

invite_operation_result accept_room_invite(const std::string& invite_id) {
    return send_with_refresh<invite_operation_result>(
        "POST", "/room-invites/" + invite_id + "/accept", {}, "Sign in to accept room invites.", "Failed to accept room invite.", parse_invite_operation);
}

invite_operation_result decline_room_invite(const std::string& invite_id) {
    return send_with_refresh<invite_operation_result>(
        "POST", "/room-invites/" + invite_id + "/decline", {}, "Sign in to decline room invites.", "Failed to decline room invite.", parse_invite_operation);
}

invite_operation_result read_room_invite(const std::string& invite_id) {
    return send_with_refresh<invite_operation_result>(
        "POST", "/room-invites/" + invite_id + "/read", {}, "Sign in to read room invites.", "Failed to mark room invite read.", parse_invite_operation);
}

ranking_client::operation_result fetch_friend_chart_ranking(const std::string& chart_id, int limit) {
    const std::optional<auth::session> session_data = load_active_session();
    if (!session_data.has_value()) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in to view friend rankings.",
            .listing = std::nullopt,
        };
    }
    return ranking_client::fetch_friend_chart_ranking(
        auth::normalize_server_url(session_data->server_url),
        session_data->access_token,
        chart_id,
        limit);
}

}  // namespace friend_client
