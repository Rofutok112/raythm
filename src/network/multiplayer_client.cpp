#include "network/multiplayer_client.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "network/http_client.h"
#include "network/json_helpers.h"
#include "network/network_error.h"

namespace {
namespace json = network::json;
using http_response = network::http::response;
using network::http::send_request;

std::string room_url(const std::string& server_url, const std::string& path) {
    std::string normalized = json::trim(server_url);
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized + path;
}

std::vector<std::pair<std::string, std::string>> headers(const std::string& access_token, bool json_body) {
    std::vector<std::pair<std::string, std::string>> result = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + access_token},
        {"User-Agent", "raythm/0.1"},
    };
    if (json_body) {
        result.emplace_back("Content-Type", "application/json");
    }
    return result;
}

template <typename Result>
void apply_error(Result& result, const network::error_classification& error) {
    result.message = error.message;
    result.maintenance = error.is_maintenance();
    result.retry_after = error.retry_after;
}

multiplayer_client::operation_result make_http_error(const http_response& response, std::string fallback) {
    multiplayer_client::operation_result result;
    result.success = false;
    result.unauthorized = response.status_code == 401;
    apply_error(result, network::classify_http_error(
                            response.status_code,
                            response.body,
                            std::move(fallback),
                            response.retry_after));
    return result;
}

std::optional<multiplayer_client::room_progress> parse_progress(const std::string& object) {
    return multiplayer_client::room_progress{
        .position_ms = json::extract_int(object, "positionMs").value_or(0),
        .score = json::extract_int(object, "score").value_or(0),
        .combo = json::extract_int(object, "combo").value_or(0),
        .accuracy = json::extract_float(object, "accuracy").value_or(0.0f),
        .finished = json::extract_bool(object, "finished").value_or(false),
    };
}

std::optional<multiplayer_client::room_result> parse_result(const std::string& object) {
    const auto score = json::extract_int(object, "score");
    const auto accuracy = json::extract_float(object, "accuracy");
    const auto max_combo = json::extract_int(object, "maxCombo");
    const auto full_combo = json::extract_bool(object, "isFullCombo");
    if (!score.has_value() || !accuracy.has_value() || !max_combo.has_value() || !full_combo.has_value()) {
        return std::nullopt;
    }
    return multiplayer_client::room_result{
        .score = *score,
        .accuracy = *accuracy,
        .max_combo = *max_combo,
        .clear_rank = json::extract_string(object, "clearRank").value_or(""),
        .is_full_combo = *full_combo,
    };
}

std::optional<multiplayer_client::room_member> parse_member(const std::string& object) {
    const auto user_id = json::extract_string(object, "userId");
    const auto display_name = json::extract_string(object, "displayName");
    if (!user_id.has_value() || !display_name.has_value()) {
        return std::nullopt;
    }

    std::optional<multiplayer_client::room_result> member_result;
    if (const auto result_object = json::extract_object(object, "result"); result_object.has_value()) {
        member_result = parse_result(*result_object);
    }

    return multiplayer_client::room_member{
        .user_id = *user_id,
        .display_name = *display_name,
        .ready = json::extract_bool(object, "ready").value_or(false),
        .connected = json::extract_bool(object, "connected").value_or(false),
        .progress = [&object]() {
            const auto progress_object = json::extract_object(object, "progress");
            if (!progress_object.has_value()) {
                return multiplayer_client::room_progress{};
            }
            return parse_progress(*progress_object).value_or(multiplayer_client::room_progress{});
        }(),
        .result = std::move(member_result),
    };
}

std::optional<multiplayer_client::room_state> parse_room_object(const std::string& object) {
    const auto room_id = json::extract_string(object, "roomId");
    const auto room_code = json::extract_string(object, "roomCode");
    const auto host_user_id = json::extract_string(object, "hostUserId");
    const auto status = json::extract_string(object, "status");
    const auto settings_object = json::extract_object(object, "settings");
    if (!room_id.has_value() || !room_code.has_value() || !host_user_id.has_value() ||
        !status.has_value() || !settings_object.has_value()) {
        return std::nullopt;
    }

    multiplayer_client::room_state room;
    room.room_id = *room_id;
    room.room_code = *room_code;
    room.visibility = json::extract_string(object, "visibility").value_or("public");
    room.requires_password = json::extract_bool(object, "requiresPassword").value_or(false);
    room.host_user_id = *host_user_id;
    room.status = *status;
    room.settings = {
        .selected_song_id = json::extract_string(*settings_object, "selectedSongId").value_or(""),
        .selected_chart_id = json::extract_string(*settings_object, "selectedChartId").value_or(""),
        .key_count = json::extract_int(*settings_object, "keyCount").value_or(0),
    };
    room.starts_at = json::extract_string(object, "startsAt").value_or("");

    if (const auto realtime = json::extract_object(object, "realtime"); realtime.has_value()) {
        room.realtime_channel = json::extract_string(*realtime, "channel").value_or("");
    }

    if (const auto members = json::extract_array(object, "members"); members.has_value()) {
        for (const std::string& member_object : json::extract_objects_from_array(*members)) {
            if (const auto member = parse_member(member_object); member.has_value()) {
                room.members.push_back(*member);
            }
        }
    }
    return room;
}

std::optional<multiplayer_client::room_state> parse_room_response(const std::string& body) {
    const auto room_object = json::extract_object(body, "room");
    return room_object.has_value() ? parse_room_object(*room_object) : parse_room_object(body);
}

std::optional<multiplayer_client::online_song> parse_online_song(const std::string& object) {
    const auto song_id = json::extract_string(object, "id");
    const auto title = json::extract_string(object, "title");
    if (!song_id.has_value() || !title.has_value()) {
        return std::nullopt;
    }
    multiplayer_client::online_song song;
    song.song_id = *song_id;
    song.title = *title;
    song.artist = json::extract_string(object, "artist").value_or("");
    return song;
}

std::optional<multiplayer_client::online_chart> parse_online_chart(const std::string& object) {
    const auto chart_id = json::extract_string(object, "id");
    if (!chart_id.has_value()) {
        return std::nullopt;
    }
    return multiplayer_client::online_chart{
        .chart_id = *chart_id,
        .difficulty_name = json::extract_string(object, "difficultyName").value_or(""),
        .key_count = json::extract_int(object, "keyCount").value_or(0),
        .level = json::extract_int(object, "calculatedLevel").value_or(json::extract_int(object, "level").value_or(0)),
        .chart_version = json::extract_int(object, "chartVersion").value_or(0),
    };
}

void append_online_song(std::vector<multiplayer_client::online_song>& songs, multiplayer_client::online_song song) {
    if (song.song_id.empty()) {
        return;
    }
    const auto existing = std::find_if(songs.begin(), songs.end(), [&](const auto& item) {
        return item.song_id == song.song_id;
    });
    if (existing == songs.end()) {
        songs.push_back(std::move(song));
    } else {
        if (existing->title.empty()) {
            existing->title = std::move(song.title);
        }
        if (existing->artist.empty()) {
            existing->artist = std::move(song.artist);
        }
    }
}

void append_online_chart(std::vector<multiplayer_client::online_song>& songs, const std::string& object) {
    const auto song_object = json::extract_object(object, "song");
    const auto chart = parse_online_chart(object);
    if (!chart.has_value()) {
        return;
    }

    const std::string song_id = song_object.has_value()
        ? json::extract_string(*song_object, "id").value_or("")
        : json::extract_string(object, "songId").value_or("");
    if (song_id.empty()) {
        return;
    }

    auto existing = std::find_if(songs.begin(), songs.end(), [&](const auto& song) {
        return song.song_id == song_id;
    });
    if (existing == songs.end()) {
        multiplayer_client::online_song song;
        song.song_id = song_id;
        if (song_object.has_value()) {
            song.title = json::extract_string(*song_object, "title").value_or("");
            song.artist = json::extract_string(*song_object, "artist").value_or("");
        }
        song.charts.push_back(*chart);
        songs.push_back(std::move(song));
    } else {
        existing->charts.push_back(*chart);
    }
}

multiplayer_client::operation_result parse_operation_response(const http_response& response, std::string fallback) {
    multiplayer_client::operation_result result;
    if (!response.error_message.empty()) {
        result.message = response.error_message;
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        return make_http_error(response, std::move(fallback));
    }

    result.success = true;
    result.room = parse_room_response(response.body);
    if (const auto rooms_array = json::extract_array(response.body, "rooms"); rooms_array.has_value()) {
        result.room_list_loaded = true;
        for (const std::string& room_object : json::extract_objects_from_array(*rooms_array)) {
            if (const auto room = parse_room_object(room_object); room.has_value()) {
                result.rooms.push_back(*room);
            }
        }
    }
    return result;
}

std::string settings_json(const multiplayer_client::room_settings& settings,
                          const std::string& visibility,
                          const std::string& password,
                          bool include_password) {
    const std::string song_id = settings.selected_song_id.empty()
        ? "null"
        : "\"" + json::escape_string(settings.selected_song_id) + "\"";
    const std::string chart_id = settings.selected_chart_id.empty()
        ? "null"
        : "\"" + json::escape_string(settings.selected_chart_id) + "\"";
    const std::string key_count = settings.key_count > 0 ? std::to_string(settings.key_count) : "null";
    std::string body = "{"
        "\"visibility\":\"" + json::escape_string(visibility) + "\","
        "\"selectedSongId\":" + song_id + ","
        "\"selectedChartId\":" + chart_id + ","
        "\"keyCount\":" + key_count;
    if (include_password) {
        body += ",\"password\":\"" + json::escape_string(password) + "\"";
    }
    body += "}";
    return body;
}

}  // namespace

namespace multiplayer_client {

std::optional<room_state> parse_room_response_for_test(const std::string& body) {
    return parse_room_response(body);
}

std::vector<online_song> parse_online_content_response_for_test(const std::string& body) {
    std::vector<online_song> songs;
    const auto items = json::extract_array(body, "items");
    if (!items.has_value()) {
        return songs;
    }
    for (const std::string& object : json::extract_objects_from_array(*items)) {
        append_online_chart(songs, object);
    }
    return songs;
}

std::vector<online_song> parse_online_song_response_for_test(const std::string& body) {
    std::vector<online_song> songs;
    const auto items = json::extract_array(body, "items");
    if (!items.has_value()) {
        return songs;
    }
    for (const std::string& object : json::extract_objects_from_array(*items)) {
        if (auto song = parse_online_song(object); song.has_value()) {
            append_online_song(songs, std::move(*song));
        }
    }
    return songs;
}

operation_result list_rooms(const std::string& server_url, const std::string& access_token) {
    const http_response response = send_request("GET", room_url(server_url, "/rooms"), headers(access_token, false));
    return parse_operation_response(response, "Failed to load multiplayer rooms.");
}

operation_result fetch_room(const std::string& server_url,
                            const std::string& access_token,
                            const std::string& room_id) {
    const http_response response = send_request(
        "GET",
        room_url(server_url, "/rooms/" + room_id),
        headers(access_token, false));
    return parse_operation_response(response, "Failed to refresh multiplayer room.");
}

operation_result create_room(const std::string& server_url,
                             const std::string& access_token,
                             const room_settings& settings,
                             const std::string& visibility,
                             const std::string& password) {
    const http_response response = send_request(
        "POST",
        room_url(server_url, "/rooms"),
        headers(access_token, true),
        settings_json(settings, visibility, password, true));
    return parse_operation_response(response, "Failed to create multiplayer room.");
}

operation_result join_room(const std::string& server_url,
                           const std::string& access_token,
                           const std::string& room_id,
                           const std::string& password) {
    const std::string body = "{\"password\":\"" + json::escape_string(password) + "\"}";
    const http_response response = send_request(
        "POST",
        room_url(server_url, "/rooms/" + room_id + "/join"),
        headers(access_token, true),
        body);
    return parse_operation_response(response, "Failed to join multiplayer room.");
}

operation_result join_room_by_code(const std::string& server_url,
                                   const std::string& access_token,
                                   const std::string& room_code,
                                   const std::string& password) {
    const std::string body =
        "{\"roomCode\":\"" + json::escape_string(room_code) + "\","
        "\"password\":\"" + json::escape_string(password) + "\"}";
    const http_response response = send_request(
        "POST",
        room_url(server_url, "/rooms/join"),
        headers(access_token, true),
        body);
    return parse_operation_response(response, "Failed to join multiplayer room.");
}

operation_result leave_room(const std::string& server_url,
                            const std::string& access_token,
                            const std::string& room_id) {
    const http_response response = send_request(
        "POST",
        room_url(server_url, "/rooms/" + room_id + "/leave"),
        headers(access_token, false));
    return parse_operation_response(response, "Failed to leave multiplayer room.");
}

operation_result update_room_settings(const std::string& server_url,
                                      const std::string& access_token,
                                      const std::string& room_id,
                                      const room_settings& settings,
                                      const std::string& visibility,
                                      const std::string& password) {
    const http_response response = send_request(
        "PATCH",
        room_url(server_url, "/rooms/" + room_id + "/settings"),
        headers(access_token, true),
        settings_json(settings, visibility, password, !password.empty()));
    return parse_operation_response(response, "Failed to update multiplayer room settings.");
}

operation_result set_ready(const std::string& server_url,
                           const std::string& access_token,
                           const std::string& room_id,
                           bool ready) {
    const std::string body = std::string("{\"ready\":") + (ready ? "true" : "false") + "}";
    const http_response response = send_request(
        "POST",
        room_url(server_url, "/rooms/" + room_id + "/ready"),
        headers(access_token, true),
        body);
    return parse_operation_response(response, "Failed to update ready state.");
}

operation_result start_room(const std::string& server_url,
                            const std::string& access_token,
                            const std::string& room_id) {
    const http_response response = send_request(
        "POST",
        room_url(server_url, "/rooms/" + room_id + "/start"),
        headers(access_token, false));
    return parse_operation_response(response, "Failed to start multiplayer room.");
}

operation_result send_progress(const std::string& server_url,
                               const std::string& access_token,
                               const std::string& room_id,
                               const room_progress& progress) {
    const std::string body =
        "{\"positionMs\":" + std::to_string(progress.position_ms) +
        ",\"score\":" + std::to_string(progress.score) +
        ",\"combo\":" + std::to_string(progress.combo) +
        ",\"accuracy\":" + std::to_string(progress.accuracy) +
        ",\"finished\":" + (progress.finished ? "true" : "false") +
        "}";
    const http_response response = send_request(
        "POST",
        room_url(server_url, "/rooms/" + room_id + "/progress"),
        headers(access_token, true),
        body);
    return parse_operation_response(response, "Failed to send multiplayer progress.");
}

operation_result submit_result(const std::string& server_url,
                               const std::string& access_token,
                               const std::string& room_id,
                               const room_result& result) {
    const std::string body =
        "{\"score\":" + std::to_string(result.score) +
        ",\"accuracy\":" + std::to_string(result.accuracy) +
        ",\"maxCombo\":" + std::to_string(result.max_combo) +
        ",\"clearRank\":\"" + json::escape_string(result.clear_rank) + "\"" +
        ",\"isFullCombo\":" + (result.is_full_combo ? "true" : "false") +
        "}";
    const http_response response = send_request(
        "POST",
        room_url(server_url, "/rooms/" + room_id + "/result"),
        headers(access_token, true),
        body);
    return parse_operation_response(response, "Failed to submit multiplayer result.");
}

}  // namespace multiplayer_client
