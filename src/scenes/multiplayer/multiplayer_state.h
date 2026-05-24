#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "network/auth_client.h"
#include "ui_text_input.h"

namespace multiplayer {
namespace client {
class realtime_client;
}

struct room_summary {
    std::string id;
    std::string name;
    std::string host_name;
    std::string chart_title;
    int player_count = 0;
    int max_players = 0;
    bool locked = false;
    bool playing = false;
};

struct room_member {
    std::string user_id;
    std::string display_name;
    std::string role;
    std::string status;
    bool ready = false;
    bool connected = false;
};

struct room_queue_item {
    std::string id;
    std::string song_id;
    std::string chart_id;
    std::string song_title;
    std::string difficulty_name;
    float level = 0.0f;
    std::string requested_by;
    std::string requested_by_user_id;
    std::string status;
};

struct chat_message {
    std::string id;
    std::string display_name;
    std::string message;
};

struct live_score {
    std::string user_id;
    std::string display_name;
    int score = 0;
    int combo = 0;
    float accuracy = 0.0f;
    bool failed = false;
};

struct room_detail {
    std::string id;
    std::string name;
    std::string host_user_id;
    std::string host_name;
    std::string status;
    std::string chart_title;
    std::string current_song_id;
    std::string current_chart_id;
    std::string queue_permission;
    int player_count = 0;
    int max_players = 0;
    bool locked = false;
    std::vector<room_member> members;
    std::vector<room_queue_item> queue;
    std::vector<chat_message> chat;
    std::vector<live_score> live_scores;
};

struct room_list_result {
    bool success = false;
    std::string message;
    std::vector<room_summary> rooms;
};

struct room_operation_result {
    bool success = false;
    std::string type;
    std::string message;
    std::string match_id;
    std::string match_start_at;
    std::optional<room_detail> room;
    std::vector<live_score> live_scores;
};

enum class screen_mode {
    list,
    room,
};

enum class modal_mode {
    none,
    create_room,
    password,
};

enum class pending_operation {
    none,
    refresh_rooms,
    create_room,
    join_room,
    refresh_room,
    leave_room,
    ready,
    chat,
    queue_add,
    queue_remove,
    queue_reorder,
    settings,
    start_match,
};

enum class ui_command {
    none,
    refresh_rooms,
    open_create_room,
    cancel_modal,
    submit_create_room,
    submit_password,
    leave_room,
    toggle_ready,
    send_chat,
    add_selected_chart,
    open_song_select,
    remove_queue_item,
    move_queue_item_up,
    move_queue_item_down,
    toggle_queue_permission,
    start_match,
    back_to_home,
};

struct state {
    state();
    ~state();

    auth::session_summary auth;
    std::string self_user_id;
    std::vector<room_summary> rooms;
    std::optional<room_detail> current_room;
    std::string status_message = "Loading rooms...";
    std::string selected_room_id;
    std::string selected_queue_item_id;
    std::string active_match_id;
    std::string requested_start_song_id;
    std::string requested_start_chart_id;
    bool start_play_requested = false;
    bool current_queue_download_requested = false;
    std::string requested_download_song_id;
    std::string requested_download_chart_id;
    bool current_queue_chart_installed = false;
    std::vector<std::string> installed_queue_item_ids;
    screen_mode screen = screen_mode::list;
    modal_mode modal = modal_mode::none;
    pending_operation pending = pending_operation::none;
    ui_command command = ui_command::none;
    ui::text_input_state create_name_input;
    ui::text_input_state create_password_input;
    ui::text_input_state join_password_input;
    ui::text_input_state chat_input;
    std::string queue_candidate_song_title;
    std::string queue_candidate_chart_name;
    std::string queue_candidate_remote_song_id;
    std::string queue_candidate_remote_chart_id;
    int queue_candidate_remote_chart_version = 0;
    bool queue_candidate_available = false;
    std::string queue_candidate_message = "Select an online chart from Play.";
    float list_refresh_t = 0.0f;
    float room_refresh_t = 0.0f;
    float realtime_ping_t = 0.0f;
    float queue_scroll_y = 0.0f;
    float queue_scroll_y_target = 0.0f;
    bool loading_rooms = false;
    bool room_request_started = false;
    bool local_ready = false;
    bool create_host_only = false;
    int create_max_players = 4;
    std::optional<std::future<room_list_result>> room_list_future;
    std::optional<std::future<room_operation_result>> operation_future;
    std::unique_ptr<client::realtime_client> realtime;
};

}  // namespace multiplayer
