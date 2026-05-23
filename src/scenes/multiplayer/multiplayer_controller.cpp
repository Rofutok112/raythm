#include "multiplayer/multiplayer_controller.h"

#include <chrono>
#include <future>
#include <thread>

#include "multiplayer/multiplayer_client.h"
#include "network/json_helpers.h"
#include "raylib.h"

namespace multiplayer {
namespace {

constexpr float kListRefreshSeconds = 15.0f;
constexpr float kRoomRefreshSeconds = 2.5f;
constexpr float kRealtimePingSeconds = 25.0f;

bool modal_open(const state& state) {
    return state.modal != modal_mode::none;
}

bool operation_busy(const state& state) {
    return state.pending != pending_operation::none || state.room_list_future.has_value() ||
           state.operation_future.has_value();
}

bool realtime_connected(const state& state) {
    return state.realtime != nullptr && state.realtime->connected();
}

bool send_realtime_command(state& state,
                           const std::string& command,
                           const std::string& body,
                           const std::string& message) {
    if (!realtime_connected(state)) {
        return false;
    }
    if (!state.realtime->send_command(command, body)) {
        const std::string error = state.realtime->last_error();
        state.status_message = error.empty() ? "Room WebSocket command failed. Falling back to polling." : error;
        return false;
    }
    state.status_message = message;
    return true;
}

void ensure_realtime(state& state) {
    if (!state.current_room.has_value() || !state.auth.logged_in) {
        return;
    }
    if (state.realtime != nullptr && state.realtime->room_id() == state.current_room->id &&
        state.realtime->connected()) {
        return;
    }
    state.realtime = std::make_unique<client::realtime_client>();
    if (!state.realtime->connect(state.auth, state.current_room->id)) {
        const std::string error = state.realtime->last_error();
        if (!error.empty()) {
            state.status_message = error + " Falling back to polling.";
        }
    }
}

void refresh_rooms(state& state) {
    if (!state.auth.logged_in || operation_busy(state)) {
        return;
    }
    state.pending = pending_operation::refresh_rooms;
    state.loading_rooms = true;
    state.room_request_started = true;
    state.status_message = "Loading rooms...";
    state.room_list_future = std::async(std::launch::async, [session = state.auth]() {
        return client::fetch_room_list(session);
    });
}

void start_operation(state& state, pending_operation operation,
                     std::future<room_operation_result>&& future,
                     const std::string& message) {
    if (operation_busy(state)) {
        return;
    }
    state.pending = operation;
    state.status_message = message;
    state.operation_future = std::move(future);
}

void apply_room(state& state, const room_detail& room) {
    state.current_room = room;
    state.selected_room_id = room.id;
    state.screen = screen_mode::room;
    state.modal = modal_mode::none;
    state.room_refresh_t = 0.0f;
    state.realtime_ping_t = 0.0f;
    state.local_ready = false;
    for (const room_member& member : room.members) {
        if ((!state.self_user_id.empty() && member.user_id == state.self_user_id) ||
            (state.self_user_id.empty() && member.display_name == state.auth.display_name)) {
            state.local_ready = member.ready;
            break;
        }
    }
    ensure_realtime(state);
}

void set_requested_start_from_room(state& state) {
    if (!state.current_room.has_value()) {
        return;
    }
    if (!state.current_room->queue.empty()) {
        state.requested_start_song_id = state.current_room->queue.front().song_id;
        state.requested_start_chart_id = state.current_room->queue.front().chart_id;
        return;
    }
    if (!state.current_room->current_song_id.empty() && !state.current_room->current_chart_id.empty()) {
        state.requested_start_song_id = state.current_room->current_song_id;
        state.requested_start_chart_id = state.current_room->current_chart_id;
    }
}

void apply_realtime_event(state& state, const room_operation_result& event) {
    if (!event.success) {
        state.status_message = event.message;
        return;
    }
    if (event.room.has_value()) {
        apply_room(state, *event.room);
    }
    if (event.type == "room.match_started" && !event.match_id.empty()) {
        state.active_match_id = event.match_id;
        set_requested_start_from_room(state);
        state.start_play_requested = true;
    }
}

void finish_operation(state& state, room_operation_result operation) {
    state.operation_future.reset();
    const pending_operation completed = state.pending;
    state.pending = pending_operation::none;
    state.status_message = operation.message;
    if (!operation.success) {
        return;
    }

    if (completed == pending_operation::start_match) {
        state.active_match_id = operation.match_id;
        if (operation.room.has_value()) {
            apply_room(state, *operation.room);
        }
        if (state.requested_start_chart_id.empty()) {
            set_requested_start_from_room(state);
        }
        state.start_play_requested = true;
    }

    if (completed == pending_operation::leave_room) {
        if (state.realtime != nullptr) {
            state.realtime->close();
            state.realtime.reset();
        }
        state.current_room.reset();
        state.selected_room_id.clear();
        state.screen = screen_mode::list;
        state.local_ready = false;
        refresh_rooms(state);
        return;
    }

    if (operation.room.has_value()) {
        apply_room(state, *operation.room);
    }
    if (completed == pending_operation::chat) {
        state.chat_input.value.clear();
        state.chat_input.cursor = 0;
    }
    if (completed == pending_operation::queue_remove) {
        state.selected_queue_item_id.clear();
    }
    if (completed == pending_operation::queue_reorder) {
        state.selected_queue_item_id.clear();
    }
}

void submit_create(state& state) {
    std::string name = state.create_name_input.value;
    if (name.empty()) {
        name = state.auth.display_name.empty() ? "raythm room" : state.auth.display_name + "'s room";
    }
    const std::string password = state.create_password_input.value;
    start_operation(state,
                    pending_operation::create_room,
                    std::async(std::launch::async,
                               [session = state.auth, name, password,
                                max_players = state.create_max_players,
                                host_only = state.create_host_only]() {
                                   return client::create_room(session, name, password, max_players, host_only);
                               }),
                    "Creating room...");
}

void submit_join(state& state, std::string password) {
    if (state.selected_room_id.empty()) {
        return;
    }
    start_operation(state,
                    pending_operation::join_room,
                    std::async(std::launch::async,
                               [session = state.auth, room_id = state.selected_room_id, password]() {
                                   return client::join_room(session, room_id, password);
                               }),
                    "Joining room...");
}

bool handle_command(state& state) {
    const ui_command command = state.command;
    if (command == ui_command::none) {
        return false;
    }
    if (operation_busy(state)) {
        return false;
    }
    state.command = ui_command::none;

    if (command == ui_command::refresh_rooms) {
        refresh_rooms(state);
    } else if (command == ui_command::open_song_select) {
        if (state.current_room.has_value() && state.local_ready) {
            state.local_ready = false;
            const std::string body = "{\"ready\":false}";
            if (!send_realtime_command(state, "ready.set", body, "Cancelling ready...")) {
                const auth::session_summary session = state.auth;
                const std::string room_id = state.current_room->id;
                state.status_message = "Cancelling ready...";
                std::thread([session, room_id]() {
                    (void)client::set_ready(session, room_id, false);
                }).detach();
            }
        }
        return true;
    } else if (command == ui_command::open_create_room) {
        state.create_name_input.value.clear();
        state.create_password_input.value.clear();
        state.create_name_input.active = true;
        state.create_name_input.cursor = 0;
        state.create_password_input.active = false;
        state.create_host_only = false;
        state.create_max_players = 4;
        state.modal = modal_mode::create_room;
    } else if (command == ui_command::cancel_modal) {
        state.modal = modal_mode::none;
    } else if (command == ui_command::submit_create_room) {
        submit_create(state);
    } else if (command == ui_command::submit_password) {
        submit_join(state, state.join_password_input.value);
    } else if (command == ui_command::leave_room && state.current_room.has_value()) {
        start_operation(state,
                        pending_operation::leave_room,
                        std::async(std::launch::async,
                                   [session = state.auth, room_id = state.current_room->id]() {
                                       return client::leave_room(session, room_id);
                                   }),
                        "Leaving room...");
    } else if (command == ui_command::toggle_ready && state.current_room.has_value()) {
        const bool next_ready = !state.local_ready;
        state.local_ready = next_ready;
        const std::string body = std::string("{\"ready\":") + (next_ready ? "true" : "false") + "}";
        if (send_realtime_command(state, "ready.set", body, next_ready ? "Setting ready..." : "Cancelling ready...")) {
            return false;
        }
        start_operation(state,
                        pending_operation::ready,
                        std::async(std::launch::async,
                                   [session = state.auth, room_id = state.current_room->id, next_ready]() {
                                       return client::set_ready(session, room_id, next_ready);
                                   }),
                        next_ready ? "Setting ready..." : "Cancelling ready...");
    } else if (command == ui_command::send_chat && state.current_room.has_value() &&
               !state.chat_input.value.empty()) {
        const std::string message = state.chat_input.value;
        if (send_realtime_command(state,
                                  "chat.send",
                                  "{\"message\":\"" + network::json::escape_string(message) + "\"}",
                                  "Sending message...")) {
            state.chat_input.value.clear();
            state.chat_input.cursor = 0;
            return false;
        }
        start_operation(state,
                        pending_operation::chat,
                        std::async(std::launch::async,
                                   [session = state.auth, room_id = state.current_room->id, message]() {
                                       return client::send_chat(session, room_id, message);
                                   }),
                        "Sending message...");
    } else if (command == ui_command::add_selected_chart && state.current_room.has_value() &&
               state.queue_candidate_available && !state.queue_candidate_remote_chart_id.empty()) {
        const std::string body = "{\"chartId\":\"" +
            network::json::escape_string(state.queue_candidate_remote_chart_id) + "\"}";
        if (send_realtime_command(state, "queue.add", body, "Adding song...")) {
            return false;
        }
        start_operation(state,
                        pending_operation::queue_add,
                        std::async(std::launch::async,
                                   [session = state.auth,
                                    room_id = state.current_room->id,
                                    chart_id = state.queue_candidate_remote_chart_id,
                                    chart_version = state.queue_candidate_remote_chart_version]() {
                                       return client::add_queue_item(session, room_id, chart_id, chart_version);
                                   }),
                        "Adding song...");
    } else if (command == ui_command::remove_queue_item && state.current_room.has_value() &&
               !state.selected_queue_item_id.empty()) {
        const std::string body = "{\"itemId\":\"" + network::json::escape_string(state.selected_queue_item_id) + "\"}";
        if (send_realtime_command(state, "queue.remove", body, "Removing song...")) {
            state.selected_queue_item_id.clear();
            return false;
        }
        start_operation(state,
                        pending_operation::queue_remove,
                        std::async(std::launch::async,
                                   [session = state.auth,
                                    room_id = state.current_room->id,
                                    item_id = state.selected_queue_item_id]() {
                                       return client::remove_queue_item(session, room_id, item_id);
                                   }),
                        "Removing song...");
    } else if ((command == ui_command::move_queue_item_up || command == ui_command::move_queue_item_down) &&
               state.current_room.has_value() && !state.selected_queue_item_id.empty()) {
        const bool move_up = command == ui_command::move_queue_item_up;
        const std::string body = "{\"itemId\":\"" + network::json::escape_string(state.selected_queue_item_id) +
            "\",\"direction\":\"" + (move_up ? "UP" : "DOWN") + "\"}";
        if (send_realtime_command(state, "queue.reorder", body, "Reordering queue...")) {
            state.selected_queue_item_id.clear();
            return false;
        }
        start_operation(state,
                        pending_operation::queue_reorder,
                        std::async(std::launch::async,
                                   [session = state.auth,
                                    room_id = state.current_room->id,
                                    item_id = state.selected_queue_item_id,
                                    move_up]() {
                                       return client::reorder_queue_item(session, room_id, item_id, move_up);
                                   }),
                        "Reordering queue...");
    } else if (command == ui_command::toggle_queue_permission && state.current_room.has_value()) {
        const bool next_host_only = state.current_room->queue_permission != "HOST_ONLY";
        const std::string body = "{\"queuePermission\":\"" + std::string(next_host_only ? "HOST_ONLY" : "ALL_PLAYERS") + "\"}";
        if (send_realtime_command(state, "settings.set", body, "Updating room settings...")) {
            return false;
        }
        start_operation(state,
                        pending_operation::settings,
                        std::async(std::launch::async,
                                   [session = state.auth,
                                    room_id = state.current_room->id,
                                    next_host_only]() {
                                       return client::set_queue_permission(session, room_id, next_host_only);
                                   }),
                        "Updating room settings...");
    } else if (command == ui_command::start_match && state.current_room.has_value()) {
        state.requested_start_song_id = state.current_room->queue.empty() ? "" : state.current_room->queue.front().song_id;
        state.requested_start_chart_id = state.current_room->queue.empty() ? "" : state.current_room->queue.front().chart_id;
        if (send_realtime_command(state, "match.start", "{}", "Starting match...")) {
            return false;
        }
        start_operation(state,
                        pending_operation::start_match,
                        std::async(std::launch::async,
                                   [session = state.auth, room_id = state.current_room->id]() {
                                       return client::start_match(session, room_id);
                                   }),
                        "Starting match...");
    }
    return false;
}

}  // namespace

void on_enter(state& state, const std::string& preferred_room_id) {
    state.auth = auth::load_session_summary();
    const std::optional<auth::session> saved_session = auth::load_saved_session();
    state.self_user_id = saved_session.has_value() ? saved_session->user.id : "";
    state.rooms.clear();
    state.current_room.reset();
    state.active_match_id.clear();
    state.requested_start_song_id.clear();
    state.requested_start_chart_id.clear();
    state.selected_queue_item_id.clear();
    state.start_play_requested = false;
    state.current_queue_download_requested = false;
    state.requested_download_song_id.clear();
    state.requested_download_chart_id.clear();
    state.current_queue_chart_installed = false;
    state.installed_queue_item_ids.clear();
    state.loading_rooms = false;
    state.room_request_started = false;
    state.room_list_future.reset();
    state.operation_future.reset();
    if (state.realtime != nullptr) {
        state.realtime->close();
        state.realtime.reset();
    }
    state.pending = pending_operation::none;
    state.command = ui_command::none;
    state.screen = screen_mode::list;
    state.modal = modal_mode::none;
    state.selected_room_id.clear();
    state.local_ready = false;
    state.queue_candidate_song_title.clear();
    state.queue_candidate_chart_name.clear();
    state.queue_candidate_remote_song_id.clear();
    state.queue_candidate_remote_chart_id.clear();
    state.queue_candidate_remote_chart_version = 0;
    state.queue_candidate_available = false;
    state.queue_candidate_message = "Select an online chart from Play.";
    state.list_refresh_t = 0.0f;
    state.room_refresh_t = 0.0f;
    state.realtime_ping_t = 0.0f;
    state.queue_scroll_y = 0.0f;
    state.queue_scroll_y_target = 0.0f;
    state.status_message = state.auth.logged_in ? "Loading rooms..." : "Login required.";
    if (!preferred_room_id.empty() && state.auth.logged_in) {
        state.selected_room_id = preferred_room_id;
        state.screen = screen_mode::room;
        state.room_request_started = true;
        start_operation(state,
                        pending_operation::refresh_room,
                        std::async(std::launch::async, [session = state.auth, preferred_room_id]() {
                            return client::fetch_room(session, preferred_room_id);
                        }),
                        "Returning to room...");
    }
}

void on_enter(state& state) {
    on_enter(state, "");
}

update_result update(state& state, float dt) {
    update_result result;
    if (!modal_open(state) && IsKeyPressed(KEY_ESCAPE)) {
        if (state.screen == screen_mode::room) {
            state.command = ui_command::leave_room;
        } else {
            result.back_requested = true;
        }
    } else if (!modal_open(state) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (state.screen != screen_mode::room) {
            result.back_requested = true;
        }
    } else if (modal_open(state) && IsKeyPressed(KEY_ESCAPE)) {
        state.modal = modal_mode::none;
    }

    if (state.command == ui_command::back_to_home) {
        state.command = ui_command::none;
        result.back_requested = true;
        return result;
    }

    result.open_song_select_requested = handle_command(state);
    if (result.back_requested) {
        return result;
    }

    if (state.room_list_future.has_value() &&
        state.room_list_future->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const room_list_result list = state.room_list_future->get();
        state.room_list_future.reset();
        state.pending = pending_operation::none;
        state.loading_rooms = false;
        state.status_message = list.message;
        if (list.success) {
            state.rooms = list.rooms;
        }
    }

    if (state.operation_future.has_value() &&
        state.operation_future->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        finish_operation(state, state.operation_future->get());
    }

    if (state.realtime != nullptr) {
        for (const room_operation_result& event : state.realtime->poll_room_events()) {
            apply_realtime_event(state, event);
        }
    }

    if (!modal_open(state) && state.screen == screen_mode::list && IsKeyPressed(KEY_R)) {
        refresh_rooms(state);
    }
    if (!modal_open(state) && state.screen == screen_mode::room && state.current_room.has_value() &&
        IsKeyPressed(KEY_F5)) {
        start_operation(state,
                        pending_operation::refresh_room,
                        std::async(std::launch::async,
                                   [session = state.auth, room_id = state.current_room->id]() {
                                       return client::fetch_room(session, room_id);
                                   }),
                        "Refreshing room...");
    }

    if (state.modal == modal_mode::password && IsKeyPressed(KEY_ENTER)) {
        state.command = ui_command::submit_password;
        result.open_song_select_requested = handle_command(state) || result.open_song_select_requested;
    } else if (state.modal == modal_mode::create_room && IsKeyPressed(KEY_ENTER)) {
        state.command = ui_command::submit_create_room;
        result.open_song_select_requested = handle_command(state) || result.open_song_select_requested;
    } else if (state.screen == screen_mode::room && state.chat_input.active && IsKeyPressed(KEY_ENTER)) {
        state.command = ui_command::send_chat;
        result.open_song_select_requested = handle_command(state) || result.open_song_select_requested;
    }

    if (!state.auth.logged_in) {
        return result;
    }

    if (!state.room_request_started) {
        refresh_rooms(state);
        return result;
    }

    if (state.screen == screen_mode::list) {
        state.list_refresh_t += dt;
        if (state.list_refresh_t >= kListRefreshSeconds) {
            state.list_refresh_t = 0.0f;
            refresh_rooms(state);
        }
    } else if (state.screen == screen_mode::room && state.current_room.has_value()) {
        if (realtime_connected(state)) {
            state.realtime_ping_t += dt;
            if (state.realtime_ping_t >= kRealtimePingSeconds) {
                state.realtime_ping_t = 0.0f;
                (void)send_realtime_command(state, "ping", "", state.status_message);
            }
        }
        state.room_refresh_t += dt;
        if (state.room_refresh_t >= kRoomRefreshSeconds && !operation_busy(state) && !realtime_connected(state)) {
            state.room_refresh_t = 0.0f;
            start_operation(state,
                            pending_operation::refresh_room,
                            std::async(std::launch::async,
                                       [session = state.auth, room_id = state.current_room->id]() {
                                           return client::fetch_room(session, room_id);
                                       }),
                            state.status_message);
        }
    }

    return result;
}

}  // namespace multiplayer
