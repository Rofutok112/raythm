#pragma once

#include <optional>
#include <string>
#include <vector>

namespace multiplayer_client {

struct room_settings {
    std::string selected_song_id;
    std::string selected_chart_id;
    int key_count = 0;
};

struct room_progress {
    int position_ms = 0;
    int score = 0;
    int combo = 0;
    float accuracy = 0.0f;
    bool finished = false;
};

struct room_result {
    int score = 0;
    float accuracy = 0.0f;
    int max_combo = 0;
    std::string clear_rank;
    bool is_full_combo = false;
};

struct online_chart {
    std::string chart_id;
    std::string local_chart_id;
    std::string difficulty_name;
    int key_count = 0;
    int level = 0;
    int chart_version = 0;
    bool installed = false;
    bool update_available = false;
};

struct online_song {
    std::string song_id;
    std::string local_song_id;
    std::string title;
    std::string artist;
    bool installed = false;
    bool update_available = false;
    std::vector<online_chart> charts;
};

struct room_member {
    std::string user_id;
    std::string display_name;
    bool ready = false;
    bool connected = false;
    room_progress progress;
    std::optional<room_result> result;
};

struct room_state {
    std::string room_id;
    std::string room_code;
    std::string visibility;
    bool requires_password = false;
    std::string host_user_id;
    std::string status;
    room_settings settings;
    std::vector<room_member> members;
    std::string starts_at;
    std::string realtime_channel;
};

struct operation_result {
    bool success = false;
    bool unauthorized = false;
    bool maintenance = false;
    bool room_list_loaded = false;
    bool online_content_loaded = false;
    std::string message;
    std::string retry_after;
    std::string server_url;
    std::optional<room_state> room;
    std::vector<room_state> rooms;
    std::vector<online_song> online_songs;
};

std::optional<room_state> parse_room_response_for_test(const std::string& body);
std::vector<online_song> parse_online_content_response_for_test(const std::string& body);
std::vector<online_song> parse_online_song_response_for_test(const std::string& body);

operation_result list_rooms(const std::string& server_url, const std::string& access_token);
operation_result fetch_room(const std::string& server_url,
                            const std::string& access_token,
                            const std::string& room_id);
operation_result create_room(const std::string& server_url,
                             const std::string& access_token,
                             const room_settings& settings,
                             const std::string& visibility = "public",
                             const std::string& password = "");
operation_result join_room(const std::string& server_url,
                           const std::string& access_token,
                           const std::string& room_id,
                           const std::string& password = "");
operation_result join_room_by_code(const std::string& server_url,
                                   const std::string& access_token,
                                   const std::string& room_code,
                                   const std::string& password = "");
operation_result leave_room(const std::string& server_url,
                            const std::string& access_token,
                            const std::string& room_id);
operation_result update_room_settings(const std::string& server_url,
                                      const std::string& access_token,
                                      const std::string& room_id,
                                      const room_settings& settings,
                                      const std::string& visibility,
                                      const std::string& password = "");
operation_result set_ready(const std::string& server_url,
                           const std::string& access_token,
                           const std::string& room_id,
                           bool ready);
operation_result start_room(const std::string& server_url,
                            const std::string& access_token,
                            const std::string& room_id);
operation_result send_progress(const std::string& server_url,
                               const std::string& access_token,
                               const std::string& room_id,
                               const room_progress& progress);
operation_result submit_result(const std::string& server_url,
                               const std::string& access_token,
                               const std::string& room_id,
                               const room_result& result);

}  // namespace multiplayer_client
