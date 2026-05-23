#include "multiplayer/multiplayer_client.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <sstream>

#include "network/http_client.h"
#include "network/json_helpers.h"
#include "network/server_environment.h"
#include "network/websocket_client.h"

namespace multiplayer::client {
namespace {

std::string room_url(const auth::session_summary& session, const std::string& path) {
    return server_environment::normalize_url(session.server_url) + "/rooms" + path;
}

std::optional<auth::session> valid_session(const auth::session_summary& session, std::string& message) {
    if (!session.logged_in || session.server_url.empty()) {
        message = "Login required.";
        return std::nullopt;
    }
    const std::optional<auth::session> saved_session = auth::load_saved_session();
    if (!saved_session.has_value() || saved_session->access_token.empty()) {
        message = "Login required.";
        return std::nullopt;
    }
    return saved_session;
}

std::vector<std::pair<std::string, std::string>> auth_headers(const auth::session& session,
                                                              bool json_body = false) {
    std::vector<std::pair<std::string, std::string>> headers{{"Authorization", "Bearer " + session.access_token}};
    if (json_body) {
        headers.emplace_back("Content-Type", "application/json");
    }
    return headers;
}

std::string error_message_from_response(const network::http::response& response, const char* fallback) {
    if (!response.error_message.empty()) {
        return response.error_message;
    }
    return network::json::extract_string(response.body, "message").value_or(fallback);
}

std::optional<size_t> find_top_level_value_start(const std::string& object, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    int object_depth = 0;
    int array_depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (size_t index = 0; index < object.size(); ++index) {
        const char ch = object[index];
        if (in_string) {
            if (escaping) {
                escaping = false;
            } else if (ch == '\\') {
                escaping = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            if (object_depth == 1 && array_depth == 0 && object.compare(index, token.size(), token) == 0) {
                size_t colon = index + token.size();
                while (colon < object.size() && std::isspace(static_cast<unsigned char>(object[colon]))) {
                    ++colon;
                }
                if (colon >= object.size() || object[colon] != ':') {
                    return std::nullopt;
                }
                size_t start = colon + 1;
                while (start < object.size() && std::isspace(static_cast<unsigned char>(object[start]))) {
                    ++start;
                }
                return start < object.size() ? std::optional<size_t>(start) : std::nullopt;
            }
            in_string = true;
        } else if (ch == '{') {
            ++object_depth;
        } else if (ch == '}') {
            --object_depth;
        } else if (ch == '[') {
            ++array_depth;
        } else if (ch == ']') {
            --array_depth;
        }
    }
    return std::nullopt;
}

std::optional<std::string> extract_top_level_string(const std::string& object, const std::string& key) {
    const std::optional<size_t> start = find_top_level_value_start(object, key);
    if (!start.has_value() || object[*start] != '"') {
        return std::nullopt;
    }
    std::string wrapper = "{\"value\":" + object.substr(*start);
    return network::json::extract_string(wrapper, "value");
}

std::optional<int> extract_top_level_int(const std::string& object, const std::string& key) {
    const std::optional<size_t> start = find_top_level_value_start(object, key);
    if (!start.has_value()) {
        return std::nullopt;
    }
    return network::json::extract_int("{\"value\":" + object.substr(*start), "value");
}

std::optional<bool> extract_top_level_bool(const std::string& object, const std::string& key) {
    const std::optional<size_t> start = find_top_level_value_start(object, key);
    if (!start.has_value()) {
        return std::nullopt;
    }
    return network::json::extract_bool("{\"value\":" + object.substr(*start), "value");
}

std::string json_string_field(const char* name, const std::string& value) {
    return "\"" + std::string(name) + "\":\"" + network::json::escape_string(value) + "\"";
}

std::string create_room_body(const std::string& name, const std::string& password,
                             int max_players, bool host_only) {
    std::ostringstream body;
    body << "{";
    bool needs_comma = false;
    auto comma = [&]() {
        if (needs_comma) {
            body << ",";
        }
        needs_comma = true;
    };
    if (!name.empty()) {
        comma();
        body << json_string_field("name", name);
    }
    if (!password.empty()) {
        comma();
        body << json_string_field("password", password);
    }
    comma();
    body << "\"maxPlayers\":" << std::clamp(max_players, 2, 8);
    comma();
    body << json_string_field("queuePermission", host_only ? "HOST_ONLY" : "ALL_PLAYERS");
    body << "}";
    return body.str();
}

bool id_in_connected_users(const std::string& room_object, const std::string& user_id) {
    const std::optional<std::string> connected = network::json::extract_array(room_object, "connectedUsers");
    if (!connected.has_value()) {
        return false;
    }
    for (const std::string& user_object : network::json::extract_objects_from_array(*connected)) {
        if (network::json::extract_string(user_object, "id").value_or("") == user_id) {
            return true;
        }
    }
    return false;
}

room_summary parse_room_summary(const std::string& object) {
    room_summary room;
    room.id = extract_top_level_string(object, "id").value_or("");
    room.name = extract_top_level_string(object, "name").value_or("Untitled room");
    room.host_name = extract_top_level_string(object, "hostName").value_or("");
    room.chart_title = extract_top_level_string(object, "chartTitle").value_or("");
    room.player_count = extract_top_level_int(object, "playerCount").value_or(0);
    room.max_players = extract_top_level_int(object, "maxPlayers").value_or(0);
    room.locked = extract_top_level_bool(object, "locked").value_or(false);
    room.playing = extract_top_level_string(object, "status").value_or("") == "IN_MATCH";
    return room;
}

room_member parse_room_member(const std::string& object, const std::string& room_object) {
    room_member member;
    const std::optional<std::string> user = network::json::extract_object(object, "user");
    if (user.has_value()) {
        member.user_id = network::json::extract_string(*user, "id").value_or("");
        member.display_name = network::json::extract_string(*user, "displayName").value_or("Player");
    }
    member.role = network::json::extract_string(object, "role").value_or("PLAYER");
    member.status = network::json::extract_string(object, "status").value_or("JOINED");
    member.ready = network::json::extract_bool(object, "ready").value_or(false);
    member.connected = id_in_connected_users(room_object, member.user_id);
    return member;
}

room_queue_item parse_queue_item(const std::string& object) {
    room_queue_item item;
    item.id = network::json::extract_string(object, "id").value_or("");
    item.status = network::json::extract_string(object, "status").value_or("");
    const std::optional<std::string> requester = network::json::extract_object(object, "requestedBy");
    if (requester.has_value()) {
        item.requested_by = network::json::extract_string(*requester, "displayName").value_or("");
        item.requested_by_user_id = network::json::extract_string(*requester, "id").value_or("");
    }
    const std::optional<std::string> chart = network::json::extract_object(object, "chart");
    if (chart.has_value()) {
        item.chart_id = network::json::extract_string(*chart, "id").value_or("");
        item.difficulty_name = network::json::extract_string(*chart, "difficultyName").value_or("");
        const std::optional<std::string> song = network::json::extract_object(*chart, "song");
        if (song.has_value()) {
            item.song_id = network::json::extract_string(*song, "id").value_or("");
            item.song_title = network::json::extract_string(*song, "title").value_or("");
        }
    }
    return item;
}

chat_message parse_chat_message(const std::string& object) {
    chat_message message;
    message.id = network::json::extract_string(object, "id").value_or("");
    message.message = network::json::extract_string(object, "message").value_or("");
    const std::optional<std::string> user = network::json::extract_object(object, "user");
    if (user.has_value()) {
        message.display_name = network::json::extract_string(*user, "displayName").value_or("Player");
    }
    return message;
}

live_score parse_live_score(const std::string& object) {
    live_score score;
    score.score = network::json::extract_int(object, "score").value_or(0);
    score.combo = network::json::extract_int(object, "combo").value_or(0);
    const std::optional<std::string> user = network::json::extract_object(object, "user");
    if (user.has_value()) {
        score.user_id = network::json::extract_string(*user, "id").value_or("");
    score.display_name = network::json::extract_string(*user, "displayName").value_or("Player");
    }
    score.failed = network::json::extract_bool(object, "failed").value_or(false);
    return score;
}

room_detail parse_room_detail(const std::string& object) {
    room_detail room;
    room.id = extract_top_level_string(object, "id").value_or("");
    room.name = extract_top_level_string(object, "name").value_or("Untitled room");
    room.host_name = extract_top_level_string(object, "hostName").value_or("");
    room.status = extract_top_level_string(object, "status").value_or("");
    room.queue_permission = extract_top_level_string(object, "queuePermission").value_or("ALL_PLAYERS");
    room.player_count = extract_top_level_int(object, "playerCount").value_or(0);
    room.max_players = extract_top_level_int(object, "maxPlayers").value_or(0);
    room.locked = extract_top_level_bool(object, "locked").value_or(false);
    const std::optional<std::string> host = network::json::extract_object(object, "host");
    if (host.has_value()) {
        room.host_user_id = network::json::extract_string(*host, "id").value_or("");
    }

    const std::optional<std::string> members = network::json::extract_array(object, "members");
    if (members.has_value()) {
        for (const std::string& member : network::json::extract_objects_from_array(*members)) {
            room.members.push_back(parse_room_member(member, object));
        }
    }
    const std::optional<std::string> queue = network::json::extract_array(object, "queueItems");
    if (queue.has_value()) {
        for (const std::string& item : network::json::extract_objects_from_array(*queue)) {
            room_queue_item parsed = parse_queue_item(item);
            if (parsed.status.empty() || parsed.status == "QUEUED") {
                room.queue.push_back(parsed);
            }
        }
    }
    const std::optional<std::string> chat = network::json::extract_array(object, "chatMessages");
    if (chat.has_value()) {
        for (const std::string& message : network::json::extract_objects_from_array(*chat)) {
            room.chat.push_back(parse_chat_message(message));
        }
    }
    const std::optional<std::string> scores = network::json::extract_array(object, "liveScores");
    if (scores.has_value()) {
        for (const std::string& score : network::json::extract_objects_from_array(*scores)) {
            room.live_scores.push_back(parse_live_score(score));
        }
    }
    return room;
}

room_operation_result parse_room_operation(const network::http::response& response,
                                           const char* success_message,
                                           const char* failure_message) {
    room_operation_result result;
    if (response.status_code < 200 || response.status_code >= 300) {
        result.message = error_message_from_response(response, failure_message);
        return result;
    }
    const std::optional<std::string> room = network::json::extract_object(response.body, "room");
    if (room.has_value()) {
        result.room = parse_room_detail(*room);
    }
    result.match_id = network::json::extract_string(response.body, "matchId").value_or("");
    result.success = true;
    result.message = success_message;
    return result;
}

room_operation_result send_room_request(const auth::session_summary& session,
                                        const std::string& method,
                                        const std::string& path,
                                        const std::string& body,
                                        const char* success_message,
                                        const char* failure_message) {
    room_operation_result result;
    std::string message;
    const std::optional<auth::session> saved_session = valid_session(session, message);
    if (!saved_session.has_value()) {
        result.message = message;
        return result;
    }
    const network::http::response response = network::http::send_request(
        method, room_url(session, path), auth_headers(*saved_session, !body.empty()), body);
    if (!response.error_message.empty()) {
        result.message = response.error_message;
        return result;
    }
    return parse_room_operation(response, success_message, failure_message);
}

}  // namespace

class realtime_client::impl {
public:
    network::websocket::client socket;
    std::string room_id;
};

realtime_client::realtime_client() : impl_(std::make_unique<impl>()) {
}

realtime_client::~realtime_client() {
    close();
}

bool realtime_client::connect(const auth::session_summary& session, const std::string& room_id) {
    close();
    std::string message;
    const std::optional<auth::session> saved_session = valid_session(session, message);
    if (!saved_session.has_value()) {
        return false;
    }
    impl_->room_id = room_id;
    return impl_->socket.connect(room_url(session, "/" + room_id + "/ws"), auth_headers(*saved_session));
}

void realtime_client::close() {
    impl_->socket.close();
    impl_->room_id.clear();
}

bool realtime_client::connected() const {
    return impl_->socket.connected();
}

const std::string& realtime_client::room_id() const {
    return impl_->room_id;
}

bool realtime_client::send_command(const std::string& command, const std::string& body) {
    std::ostringstream message;
    message << "{\"command\":\"" << network::json::escape_string(command) << "\"";
    if (!body.empty()) {
        message << ",\"body\":" << body;
    }
    message << "}";
    return impl_->socket.send_text(message.str());
}

std::vector<room_operation_result> realtime_client::poll_room_events() {
    std::vector<room_operation_result> results;
    for (const std::string& message : impl_->socket.poll_messages()) {
        const std::optional<std::string> payload = network::json::extract_object(message, "payload");
        if (!payload.has_value()) {
            continue;
        }
    room_operation_result result;
    result.success = true;
    result.message = "Room updated.";
    result.match_id = network::json::extract_string(*payload, "matchId").value_or("");
    const std::optional<std::string> scores = network::json::extract_array(*payload, "liveScores");
    if (scores.has_value()) {
        room_detail room;
        for (const std::string& score : network::json::extract_objects_from_array(*scores)) {
            room.live_scores.push_back(parse_live_score(score));
        }
        result.room = std::move(room);
    }
    const std::optional<std::string> room = network::json::extract_object(*payload, "room");
    if (room.has_value()) {
        result.room = parse_room_detail(*room);
        } else if (network::json::extract_string(message, "type").value_or("") == "error") {
            result.success = false;
            result.message = network::json::extract_string(*payload, "message").value_or("Room WebSocket error.");
        }
        results.push_back(std::move(result));
    }
    return results;
}

std::string realtime_client::last_error() const {
    return impl_->socket.last_error();
}

room_list_result fetch_room_list(const auth::session_summary& session) {
    room_list_result result;
    std::string message;
    const std::optional<auth::session> saved_session = valid_session(session, message);
    if (!saved_session.has_value()) {
        result.message = message;
        return result;
    }

    const network::http::response response = network::http::send_request(
        "GET",
        room_url(session, ""),
        auth_headers(*saved_session));
    if (!response.error_message.empty()) {
        result.message = response.error_message;
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        result.message = "Failed to load rooms.";
        return result;
    }

    const std::optional<std::string> rooms_array = network::json::extract_array(response.body, "rooms");
    if (rooms_array.has_value()) {
        for (const std::string& object : network::json::extract_objects_from_array(*rooms_array)) {
            result.rooms.push_back(parse_room_summary(object));
        }
    }
    result.success = true;
    result.message = result.rooms.empty() ? "No rooms yet." : "Rooms loaded.";
    return result;
}

room_operation_result create_room(const auth::session_summary& session,
                                  const std::string& name,
                                  const std::string& password,
                                  int max_players,
                                  bool host_only) {
    return send_room_request(session, "POST", "", create_room_body(name, password, max_players, host_only),
                             "Room created.", "Failed to create room.");
}

room_operation_result join_room(const auth::session_summary& session,
                                const std::string& room_id,
                                const std::string& password) {
    const std::string body = password.empty() ? "{}" : ("{" + json_string_field("password", password) + "}");
    return send_room_request(session, "POST", "/" + room_id + "/join", body,
                             "Joined room.", "Failed to join room.");
}

room_operation_result fetch_room(const auth::session_summary& session, const std::string& room_id) {
    return send_room_request(session, "GET", "/" + room_id, "", "Room updated.", "Failed to load room.");
}

room_operation_result leave_room(const auth::session_summary& session, const std::string& room_id) {
    return send_room_request(session, "POST", "/" + room_id + "/leave", "{}",
                             "Left room.", "Failed to leave room.");
}

room_operation_result set_ready(const auth::session_summary& session, const std::string& room_id, bool ready) {
    return send_room_request(session, "POST", "/" + room_id + "/ready",
                             std::string("{\"ready\":") + (ready ? "true" : "false") + "}",
                             ready ? "Ready." : "Ready cancelled.", "Failed to update ready state.");
}

room_operation_result send_chat(const auth::session_summary& session,
                                const std::string& room_id,
                                const std::string& message) {
    return send_room_request(session, "POST", "/" + room_id + "/chat",
                             "{" + json_string_field("message", message) + "}",
                             "Message sent.", "Failed to send message.");
}

room_operation_result add_queue_item(const auth::session_summary& session,
                                     const std::string& room_id,
                                     const std::string& chart_id,
                                     int chart_version) {
    (void)chart_version;
    std::string body = "{" + json_string_field("chartId", chart_id) + "}";
    return send_room_request(session, "POST", "/" + room_id + "/queue", body,
                             "Added song to queue.", "Failed to add song.");
}

room_operation_result remove_queue_item(const auth::session_summary& session,
                                        const std::string& room_id,
                                        const std::string& item_id) {
    return send_room_request(session, "DELETE", "/" + room_id + "/queue/" + item_id, "",
                             "Removed song from queue.", "Failed to remove song.");
}

room_operation_result reorder_queue_item(const auth::session_summary& session,
                                         const std::string& room_id,
                                         const std::string& item_id,
                                         bool move_up) {
    const std::string body = "{" + json_string_field("direction", move_up ? "UP" : "DOWN") + "}";
    return send_room_request(session, "PATCH", "/" + room_id + "/queue/" + item_id + "/reorder", body,
                             "Queue reordered.", "Failed to reorder queue.");
}

room_operation_result set_queue_permission(const auth::session_summary& session,
                                           const std::string& room_id,
                                           bool host_only) {
    const std::string body = "{" + json_string_field("queuePermission", host_only ? "HOST_ONLY" : "ALL_PLAYERS") + "}";
    return send_room_request(session, "PATCH", "/" + room_id + "/settings", body,
                             "Room settings updated.", "Failed to update room settings.");
}

room_operation_result start_match(const auth::session_summary& session, const std::string& room_id) {
    return send_room_request(session, "POST", "/" + room_id + "/start", "{}",
                             "Match started.", "Failed to start match.");
}

room_operation_result complete_match(const auth::session_summary& session, const std::string& match_id) {
    room_operation_result result;
    std::string message;
    const std::optional<auth::session> saved_session = valid_session(session, message);
    if (!saved_session.has_value()) {
        result.message = message;
        return result;
    }
    const network::http::response response = network::http::send_request(
        "POST",
        server_environment::normalize_url(session.server_url) + "/matches/" + match_id + "/complete",
        auth_headers(*saved_session, true),
        "{}");
    if (!response.error_message.empty()) {
        result.message = response.error_message;
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        result.message = error_message_from_response(response, "Failed to complete match.");
        return result;
    }
    result.success = true;
    result.message = "Match completed.";
    return result;
}

room_operation_result update_score(const auth::session_summary& session,
                                   const std::string& room_id,
                                   const std::string& match_id,
                                   int score,
                                   int combo,
                                   bool failed) {
    std::ostringstream body;
    body << "{\"score\":" << std::clamp(score, 0, 1000000)
         << ",\"combo\":" << std::max(0, combo)
         << ",\"failed\":" << (failed ? "true" : "false");
    if (!match_id.empty()) {
        body << "," << json_string_field("matchId", match_id);
    }
    body << "}";
    return send_room_request(session, "POST", "/" + room_id + "/score", body.str(),
                             "Score updated.", "Failed to update score.");
}

}  // namespace multiplayer::client
