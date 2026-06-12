#include "network/friend_client.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "network/auth_client.h"
#include "network/friend_client_parser.h"
#include "network/http_client.h"
#include "network/json_helpers.h"
#include "network/network_error.h"
#include "network/websocket_client.h"

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

std::vector<std::pair<std::string, std::string>> websocket_auth_headers(const auth::session& session_data) {
    return {
        {"Authorization", "Bearer " + session_data.access_token},
        {"User-Agent", "raythm/0.1"},
    };
}

std::string social_ws_url(const auth::session& session_data) {
    return auth::normalize_server_url(session_data.server_url) + "/social/ws";
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

std::optional<friend_client::operation_result> parse_basic_operation(const http_response&) {
    return friend_client::operation_result{.success = true};
}

}  // namespace

namespace friend_client {

class social_realtime_client::impl {
public:
    network::websocket::client socket;
};

social_realtime_client::social_realtime_client() : impl_(std::make_unique<impl>()) {
}

social_realtime_client::~social_realtime_client() {
    close();
}

bool social_realtime_client::connect() {
    close();
    std::optional<auth::session> session_data = load_active_session();
    if (!session_data.has_value()) {
        return false;
    }
    if (impl_->socket.connect(social_ws_url(*session_data), websocket_auth_headers(*session_data))) {
        return true;
    }
    const auth::operation_result restored = auth::restore_saved_session();
    if (!restored.success || !restored.session_data.has_value()) {
        return false;
    }
    close();
    return impl_->socket.connect(social_ws_url(*restored.session_data), websocket_auth_headers(*restored.session_data));
}

void social_realtime_client::close() {
    impl_->socket.close();
}

bool social_realtime_client::connected() const {
    return impl_->socket.connected();
}

bool social_realtime_client::send_ping() {
    return impl_->socket.send_text("{\"command\":\"ping\"}");
}

std::vector<social_realtime_event> social_realtime_client::poll_events() {
    std::vector<social_realtime_event> events;
    for (const std::string& message : impl_->socket.poll_messages()) {
        social_realtime_event event = detail::parse_social_event_message(message);
        if (!event.type.empty()) {
            events.push_back(std::move(event));
        }
    }
    return events;
}

std::string social_realtime_client::last_error() const {
    return impl_->socket.last_error();
}

friend_listing_result fetch_friends() {
    return send_with_refresh<friend_listing_result>(
        "GET", "/friends", {}, "Sign in to view friends.", "Failed to load friends.", detail::parse_friend_listing_response);
}

request_listing_result fetch_friend_requests() {
    return send_with_refresh<request_listing_result>(
        "GET", "/friends/requests", {}, "Sign in to view friend requests.", "Failed to load friend requests.", detail::parse_request_listing_response);
}

request_operation_result send_friend_request(const std::string& target_user_id) {
    return send_with_refresh<request_operation_result>(
        "POST",
        "/friends/requests",
        "{" + json_string_field(target_user_id, "targetUserId") + "}",
        "Sign in to send friend requests.",
        "Failed to send friend request.",
        detail::parse_request_operation_response);
}

request_operation_result accept_friend_request(const std::string& request_id) {
    return send_with_refresh<request_operation_result>(
        "POST", "/friends/requests/" + request_id + "/accept", {}, "Sign in to accept friend requests.", "Failed to accept friend request.", detail::parse_request_operation_response);
}

request_operation_result decline_friend_request(const std::string& request_id) {
    return send_with_refresh<request_operation_result>(
        "POST", "/friends/requests/" + request_id + "/decline", {}, "Sign in to decline friend requests.", "Failed to decline friend request.", detail::parse_request_operation_response);
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
        "GET", "/room-invites", {}, "Sign in to view room invites.", "Failed to load room invites.", detail::parse_invite_listing_response);
}

invite_operation_result send_room_invite(const std::string& room_id, const std::string& recipient_user_id) {
    return send_with_refresh<invite_operation_result>(
        "POST",
        "/rooms/" + room_id + "/invites",
        "{" + json_string_field(recipient_user_id, "recipientUserId") + "}",
        "Sign in to invite friends.",
        "Failed to send room invite.",
        detail::parse_invite_operation_response);
}

invite_operation_result accept_room_invite(const std::string& invite_id) {
    return send_with_refresh<invite_operation_result>(
        "POST", "/room-invites/" + invite_id + "/accept", {}, "Sign in to accept room invites.", "Failed to accept room invite.", detail::parse_invite_operation_response);
}

invite_operation_result decline_room_invite(const std::string& invite_id) {
    return send_with_refresh<invite_operation_result>(
        "POST", "/room-invites/" + invite_id + "/decline", {}, "Sign in to decline room invites.", "Failed to decline room invite.", detail::parse_invite_operation_response);
}

invite_operation_result read_room_invite(const std::string& invite_id) {
    return send_with_refresh<invite_operation_result>(
        "POST", "/room-invites/" + invite_id + "/read", {}, "Sign in to read room invites.", "Failed to mark room invite read.", detail::parse_invite_operation_response);
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
