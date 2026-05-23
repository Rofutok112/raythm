#include "multiplayer/multiplayer_view.h"

#include <algorithm>
#include <string>

#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_text_input.h"

namespace multiplayer::view {
namespace {

constexpr Rectangle kTitleRect{80.0f, 72.0f, 720.0f, 48.0f};
constexpr Rectangle kStatusRect{80.0f, 128.0f, 980.0f, 32.0f};
constexpr Rectangle kRefreshButtonRect{1260.0f, 92.0f, 190.0f, 54.0f};
constexpr Rectangle kCreateButtonRect{1470.0f, 92.0f, 330.0f, 54.0f};
constexpr Rectangle kListRect{80.0f, 190.0f, 1760.0f, 760.0f};
constexpr Rectangle kMemberPanelRect{80.0f, 190.0f, 500.0f, 660.0f};
constexpr Rectangle kQueuePanelRect{610.0f, 190.0f, 620.0f, 660.0f};
constexpr Rectangle kChatPanelRect{1260.0f, 190.0f, 580.0f, 660.0f};
constexpr Rectangle kLeaveButtonRect{80.0f, 880.0f, 190.0f, 54.0f};
constexpr Rectangle kReadyButtonRect{300.0f, 880.0f, 660.0f, 54.0f};
constexpr Rectangle kStartButtonRect{990.0f, 880.0f, 240.0f, 54.0f};
constexpr Rectangle kChatInputRect{1260.0f, 870.0f, 430.0f, 54.0f};
constexpr Rectangle kChatButtonRect{1710.0f, 870.0f, 130.0f, 54.0f};
constexpr Rectangle kQueuePermissionButtonRect{1490.0f, 92.0f, 350.0f, 54.0f};
constexpr Rectangle kCreateModalRect{610.0f, 250.0f, 700.0f, 430.0f};
constexpr Rectangle kPasswordModalRect{660.0f, 310.0f, 600.0f, 300.0f};
constexpr float kRoomRowHeight = 82.0f;
constexpr float kRoomRowGap = 12.0f;

bool busy(const state& state) {
    return state.pending != pending_operation::none;
}

bool is_self_host(const state& state, const room_detail& room) {
    return !state.self_user_id.empty() && state.self_user_id == room.host_user_id;
}

bool all_joined_members_ready(const room_detail& room) {
    if (room.members.empty()) {
        return false;
    }
    bool has_joined = false;
    for (const room_member& member : room.members) {
        if (member.status != "JOINED") {
            continue;
        }
        has_joined = true;
        if (!member.ready) {
            return false;
        }
    }
    return has_joined;
}

bool visible_member_status(const std::string& status) {
    return status == "JOINED" || status == "PLAYING";
}

std::string room_status_label(const room_summary& room) {
    const std::string status = room.playing ? "playing" : "lobby";
    return room.host_name + "  " + std::to_string(room.player_count) + "/" +
           std::to_string(room.max_players) + "  " + status;
}

void draw_panel_title(Rectangle rect, const char* title) {
    ui::draw_text_in_rect(title, 22, {rect.x, rect.y - 38.0f, rect.width, 30.0f},
                          g_theme->text, ui::text_align::left);
}

void draw_room_row(state& state, const room_summary& room, int index) {
    const Rectangle row{
        kListRect.x,
        kListRect.y + static_cast<float>(index) * (kRoomRowHeight + kRoomRowGap),
        kListRect.width,
        kRoomRowHeight,
    };
    const ui::row_state row_state =
        ui::draw_row(row, g_theme->row, g_theme->row_hover, room.locked ? g_theme->slow : g_theme->border);
    const std::string lock = room.locked ? "[LOCK] " : "";
    const std::string title = lock + room.name;
    const std::string meta = room_status_label(room);
    ui::draw_text_in_rect(title.c_str(), 25, {row.x + 24.0f, row.y + 10.0f, row.width - 48.0f, 32.0f},
                          g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(meta.c_str(), 18, {row.x + 24.0f, row.y + 45.0f, 560.0f, 24.0f},
                          g_theme->text_muted, ui::text_align::left);
    ui::draw_text_in_rect(room.chart_title.empty() ? "No chart selected" : room.chart_title.c_str(),
                          18, {row.x + 620.0f, row.y + 45.0f, row.width - 650.0f, 24.0f},
                          g_theme->text_muted, ui::text_align::right);
    if (row_state.clicked) {
        state.selected_room_id = room.id;
        if (room.locked) {
            state.join_password_input.value.clear();
            state.join_password_input.cursor = 0;
            state.join_password_input.active = true;
            state.modal = modal_mode::password;
        } else {
            state.command = ui_command::submit_password;
            state.join_password_input.value.clear();
        }
    }
}

void draw_room_list(state& state) {
    ui::draw_display_text_in_rect("MULTIPLAY", 38, kTitleRect, g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(state.status_message.c_str(), 20, kStatusRect,
                          state.auth.logged_in ? g_theme->text_muted : g_theme->slow,
                          ui::text_align::left);

    if (ui::draw_button(kRefreshButtonRect, "Refresh", 20).clicked) {
        state.command = ui_command::refresh_rooms;
    }
    if (ui::draw_button_colored(kCreateButtonRect, "Create room", 22,
                                state.auth.logged_in ? g_theme->accent : g_theme->row,
                                state.auth.logged_in ? g_theme->row_active : g_theme->row_hover,
                                g_theme->text).clicked &&
        state.auth.logged_in) {
        state.command = ui_command::open_create_room;
    }

    ui::draw_rect_f(kListRect, with_alpha(g_theme->panel, 170));
    ui::draw_rect_lines(kListRect, 2.0f, g_theme->border);
    if (!state.auth.logged_in) {
        ui::draw_text_in_rect("Sign in from the account menu before joining multiplayer.",
                              24, kListRect, g_theme->text_muted);
        return;
    }

    int index = 0;
    for (const room_summary& room : state.rooms) {
        if (index >= 8) {
            break;
        }
        draw_room_row(state, room, index++);
    }
    if (state.rooms.empty() && !state.loading_rooms) {
        ui::draw_text_in_rect("No rooms yet.", 24, kListRect, g_theme->text_muted);
    }
}

void draw_members(const state& state, const room_detail& room) {
    draw_panel_title(kMemberPanelRect, "Players");
    ui::draw_panel(kMemberPanelRect);
    Rectangle row{kMemberPanelRect.x + 16.0f, kMemberPanelRect.y + 18.0f, kMemberPanelRect.width - 32.0f, 56.0f};
    for (const room_member& member : room.members) {
        if (!visible_member_status(member.status)) {
            continue;
        }
        const bool self = (!state.self_user_id.empty() && member.user_id == state.self_user_id);
        ui::draw_row(row, self ? g_theme->row_selected : g_theme->row, g_theme->row_hover,
                     member.role == "HOST" ? g_theme->slow : g_theme->border, 1.5f);
        const std::string name = (member.role == "HOST" ? "[HOST] " : "") + member.display_name;
        ui::draw_text_in_rect(name.c_str(), 19, {row.x + 14.0f, row.y, row.width - 170.0f, row.height},
                              g_theme->text, ui::text_align::left);
        const std::string presence = member.status == "PLAYING" ? "playing" : (member.connected ? "online" : "away");
        const Color presence_color = member.status == "PLAYING"
            ? g_theme->accent
            : (member.connected ? g_theme->text_muted : g_theme->text_dim);
        ui::draw_text_in_rect(presence.c_str(), 15,
                              {row.x + row.width - 160.0f, row.y, 70.0f, row.height},
                              presence_color);
        ui::draw_text_in_rect(member.ready ? "READY" : "WAIT", 16,
                              {row.x + row.width - 82.0f, row.y, 72.0f, row.height},
                              member.ready ? g_theme->success : g_theme->text_muted);
        row.y += row.height + 10.0f;
        if (row.y + row.height > kMemberPanelRect.y + kMemberPanelRect.height - 12.0f) {
            break;
        }
    }
}

void draw_queue(state& state, const room_detail& room) {
    draw_panel_title(kQueuePanelRect, "Beatmap queue");
    ui::draw_panel(kQueuePanelRect);
    const bool self_host = is_self_host(state, room);
    const Rectangle add_rect{kQueuePanelRect.x + 16.0f, kQueuePanelRect.y + 18.0f, kQueuePanelRect.width - 32.0f, 50.0f};
    if (ui::draw_button_colored(add_rect, "Add song", 17,
                                g_theme->accent, g_theme->row_hover, g_theme->text).clicked &&
        !busy(state)) {
        state.command = ui_command::open_song_select;
    }

    Rectangle row{kQueuePanelRect.x + 16.0f, kQueuePanelRect.y + 86.0f, kQueuePanelRect.width - 32.0f, 70.0f};
    if (room.queue.empty()) {
        ui::draw_text_in_rect("No queued songs yet.", 21,
                              {kQueuePanelRect.x, kQueuePanelRect.y + 80.0f,
                               kQueuePanelRect.width, kQueuePanelRect.height - 80.0f},
                              g_theme->text_muted);
        return;
    }
    if (!state.current_queue_chart_message.empty()) {
        ui::draw_text_in_rect(state.current_queue_chart_message.c_str(), 15,
                              {kQueuePanelRect.x + 16.0f, kQueuePanelRect.y + 70.0f,
                               kQueuePanelRect.width - 32.0f, 18.0f},
                              state.current_queue_chart_installed ? g_theme->text_muted : g_theme->slow,
                              ui::text_align::left);
    }
    int queue_index = 0;
    for (const room_queue_item& item : room.queue) {
        ui::draw_row(row, g_theme->row, g_theme->row_hover, g_theme->border, 1.5f);
        const bool can_remove = self_host || (!state.self_user_id.empty() && item.requested_by_user_id == state.self_user_id);
        const float action_width = self_host ? 188.0f : (can_remove ? 96.0f : 0.0f);
        const std::string title = item.song_title.empty() ? item.chart_id : item.song_title;
        ui::draw_text_in_rect(title.c_str(), 19, {row.x + 14.0f, row.y + 8.0f, row.width - 28.0f - action_width, 28.0f},
                              g_theme->text, ui::text_align::left);
        const std::string meta = item.difficulty_name + (item.requested_by.empty() ? "" : "  by " + item.requested_by);
        ui::draw_text_in_rect(meta.c_str(), 15, {row.x + 14.0f, row.y + 38.0f, row.width - 28.0f - action_width, 24.0f},
                              g_theme->text_muted, ui::text_align::left);
        if (self_host) {
            const bool can_move_up = queue_index > 0;
            const bool can_move_down = queue_index + 1 < static_cast<int>(room.queue.size());
            if (ui::draw_button_colored({row.x + row.width - 196.0f, row.y + 14.0f, 42.0f, 42.0f},
                                        "Up", 14,
                                        can_move_up ? g_theme->row : g_theme->panel,
                                        g_theme->row_hover,
                                        can_move_up ? g_theme->text_muted : g_theme->text_dim).clicked &&
                can_move_up && !busy(state)) {
                state.selected_queue_item_id = item.id;
                state.command = ui_command::move_queue_item_up;
            }
            if (ui::draw_button_colored({row.x + row.width - 148.0f, row.y + 14.0f, 48.0f, 42.0f},
                                        "Down", 13,
                                        can_move_down ? g_theme->row : g_theme->panel,
                                        g_theme->row_hover,
                                        can_move_down ? g_theme->text_muted : g_theme->text_dim).clicked &&
                can_move_down && !busy(state)) {
                state.selected_queue_item_id = item.id;
                state.command = ui_command::move_queue_item_down;
            }
        }
        if (can_remove &&
            ui::draw_button_colored({row.x + row.width - 104.0f, row.y + 14.0f, 88.0f, 42.0f},
                                    "Remove", 15, g_theme->row, g_theme->row_hover, g_theme->text_muted).clicked &&
            !busy(state)) {
            state.selected_queue_item_id = item.id;
            state.command = ui_command::remove_queue_item;
        }
        row.y += row.height + 10.0f;
        ++queue_index;
        if (row.y + row.height > kQueuePanelRect.y + kQueuePanelRect.height - 12.0f) {
            break;
        }
    }
}

void draw_chat(state& state, const room_detail& room) {
    draw_panel_title(kChatPanelRect, "Chat");
    ui::draw_panel(kChatPanelRect);
    Rectangle row{kChatPanelRect.x + 16.0f, kChatPanelRect.y + 18.0f, kChatPanelRect.width - 32.0f, 46.0f};
    const int first = std::max(0, static_cast<int>(room.chat.size()) - 11);
    for (int i = first; i < static_cast<int>(room.chat.size()); ++i) {
        const chat_message& message = room.chat[static_cast<size_t>(i)];
        const std::string line = message.display_name + ": " + message.message;
        ui::draw_text_in_rect(line.c_str(), 16, row, g_theme->text_muted, ui::text_align::left);
        row.y += row.height + 6.0f;
    }
    const ui::text_input_result chat_result =
        ui::draw_text_input(kChatInputRect, state.chat_input, "", "Message...", nullptr,
                            ui::draw_layer::base, 16, 160, ui::default_text_input_filter, 0.0f,
                            false, true);
    if (chat_result.submitted) {
        state.command = ui_command::send_chat;
    }
    if (ui::draw_button(kChatButtonRect, "Send", 18).clicked) {
        state.command = ui_command::send_chat;
    }
}

void draw_room(state& state) {
    if (!state.current_room.has_value()) {
        return;
    }
    const room_detail& room = *state.current_room;
    ui::draw_display_text_in_rect(room.name.c_str(), 36, kTitleRect, g_theme->text, ui::text_align::left);
    const std::string subtitle = room.host_name + " host  " + std::to_string(room.player_count) + "/" +
                                 std::to_string(room.max_players) + "  " + room.queue_permission;
    ui::draw_text_in_rect(subtitle.c_str(), 18, kStatusRect, g_theme->text_muted, ui::text_align::left);
    if (is_self_host(state, room)) {
        ui::draw_text_in_rect(state.status_message.c_str(), 17,
                              {kStatusRect.x + 860.0f, kStatusRect.y, 530.0f, kStatusRect.height},
                              g_theme->text_muted, ui::text_align::right);
        const bool host_only = room.queue_permission == "HOST_ONLY";
        const std::string permission_label = host_only ? "Queue: host only" : "Queue: all players";
        if (ui::draw_button_colored(kQueuePermissionButtonRect, permission_label.c_str(), 18,
                                    g_theme->row, g_theme->row_hover, g_theme->text).clicked &&
            !busy(state)) {
            state.command = ui_command::toggle_queue_permission;
        }
    } else {
        ui::draw_text_in_rect(state.status_message.c_str(), 17,
                              {kStatusRect.x + 900.0f, kStatusRect.y, 720.0f, kStatusRect.height},
                              g_theme->text_muted, ui::text_align::right);
    }

    draw_members(state, room);
    draw_queue(state, room);
    draw_chat(state, room);

    if (ui::draw_button(kLeaveButtonRect, "Leave", 20).clicked) {
        state.command = ui_command::leave_room;
    }
    const bool queue_ready = !room.queue.empty() && state.current_queue_chart_installed;
    const std::string ready_label = state.local_ready ? "Cancel Ready"
        : (room.queue.empty() ? "Ready (queue empty)"
        : (state.current_queue_chart_installed ? "Ready" : "Ready (download needed)"));
    const Color ready_bg = queue_ready ? (state.local_ready ? g_theme->row_selected : g_theme->success) : g_theme->row;
    if (ui::draw_button_colored(kReadyButtonRect, ready_label.c_str(), 22,
                                ready_bg, g_theme->row_hover, g_theme->text).clicked &&
        queue_ready) {
        state.command = ui_command::toggle_ready;
    }
    const bool can_start = is_self_host(state, room) && queue_ready && all_joined_members_ready(room);
    if (!queue_ready && !room.queue.empty()) {
        if (ui::draw_button_colored(kStartButtonRect, "Download", 20,
                                    g_theme->accent, g_theme->row_hover, g_theme->text).clicked &&
            !busy(state)) {
            state.current_queue_download_requested = true;
        }
    } else {
        if (ui::draw_button_colored(kStartButtonRect, "Start", 22,
                                    can_start ? g_theme->accent : g_theme->row,
                                    g_theme->row_hover,
                                    can_start ? g_theme->text : g_theme->text_muted).clicked &&
            can_start) {
            state.command = ui_command::start_match;
        }
    }
}

void draw_create_modal(state& state) {
    ui::register_hit_region(kCreateModalRect, ui::draw_layer::modal);
    ui::draw_fullscreen_overlay(with_alpha(BLACK, 130));
    ui::draw_panel(kCreateModalRect);
    ui::draw_text_in_rect("Create room", 28,
                          {kCreateModalRect.x + 26.0f, kCreateModalRect.y + 22.0f,
                           kCreateModalRect.width - 52.0f, 36.0f},
                          g_theme->text, ui::text_align::left);
    const ui::text_input_result name_result =
        ui::draw_text_input({kCreateModalRect.x + 40.0f, kCreateModalRect.y + 90.0f, 620.0f, 56.0f},
                            state.create_name_input, "Name", "Room name", nullptr,
                            ui::draw_layer::modal, 18, 80, ui::default_text_input_filter, 110.0f);
    const ui::text_input_result password_result =
        ui::draw_text_input({kCreateModalRect.x + 40.0f, kCreateModalRect.y + 162.0f, 620.0f, 56.0f},
                            state.create_password_input, "Password", "Optional", nullptr,
                            ui::draw_layer::modal, 18, 128, ui::default_text_input_filter, 110.0f, true);
    if ((name_result.submitted || password_result.submitted)) {
        state.command = ui_command::submit_create_room;
    }

    const Rectangle max_rect{kCreateModalRect.x + 40.0f, kCreateModalRect.y + 238.0f, 280.0f, 48.0f};
    const Rectangle host_rect{kCreateModalRect.x + 340.0f, kCreateModalRect.y + 238.0f, 320.0f, 48.0f};
    const std::string max_label = "Players: " + std::to_string(state.create_max_players);
    const ui::selector_state max_selector = ui::draw_value_selector(max_rect, "Max", max_label.c_str(),
                                                                    ui::draw_layer::modal, 17, 32.0f, 82.0f);
    if (max_selector.left.clicked) {
        state.create_max_players = std::max(2, state.create_max_players - 1);
    }
    if (max_selector.right.clicked) {
        state.create_max_players = std::min(8, state.create_max_players + 1);
    }
    const ui::button_state host_button =
        ui::enqueue_button(host_rect,
                           state.create_host_only ? "Queue: host only" : "Queue: all players",
                           17,
                           ui::draw_layer::modal);
    if (host_button.clicked) {
        state.create_host_only = !state.create_host_only;
    }

    const ui::button_state cancel =
        ui::enqueue_button({kCreateModalRect.x + 40.0f, kCreateModalRect.y + 342.0f, 180.0f, 54.0f},
                           "Cancel", 18, ui::draw_layer::modal);
    const ui::button_state create =
        ui::enqueue_button({kCreateModalRect.x + 250.0f, kCreateModalRect.y + 342.0f, 410.0f, 54.0f},
                           busy(state) ? "Creating..." : "Create", 18, ui::draw_layer::modal);
    if (cancel.clicked) {
        state.command = ui_command::cancel_modal;
    }
    if (create.clicked) {
        state.command = ui_command::submit_create_room;
    }
}

void draw_password_modal(state& state) {
    ui::register_hit_region(kPasswordModalRect, ui::draw_layer::modal);
    ui::draw_fullscreen_overlay(with_alpha(BLACK, 130));
    ui::draw_panel(kPasswordModalRect);
    ui::draw_text_in_rect("Room password", 28,
                          {kPasswordModalRect.x + 26.0f, kPasswordModalRect.y + 24.0f,
                           kPasswordModalRect.width - 52.0f, 36.0f},
                          g_theme->text, ui::text_align::left);
    const ui::text_input_result password_result =
        ui::draw_text_input({kPasswordModalRect.x + 40.0f, kPasswordModalRect.y + 96.0f, 520.0f, 56.0f},
                            state.join_password_input, "Password", "", nullptr,
                            ui::draw_layer::modal, 18, 128, ui::default_text_input_filter, 120.0f, true);
    if (password_result.submitted) {
        state.command = ui_command::submit_password;
    }
    const ui::button_state cancel =
        ui::enqueue_button({kPasswordModalRect.x + 40.0f, kPasswordModalRect.y + 206.0f, 170.0f, 54.0f},
                           "Cancel", 18, ui::draw_layer::modal);
    const ui::button_state join =
        ui::enqueue_button({kPasswordModalRect.x + 240.0f, kPasswordModalRect.y + 206.0f, 320.0f, 54.0f},
                           busy(state) ? "Joining..." : "Join", 18, ui::draw_layer::modal);
    if (cancel.clicked) {
        state.command = ui_command::cancel_modal;
    }
    if (join.clicked) {
        state.command = ui_command::submit_password;
    }
}

}  // namespace

void draw(state& state) {
    if (state.screen == screen_mode::room) {
        draw_room(state);
    } else {
        draw_room_list(state);
    }

    if (state.modal == modal_mode::create_room) {
        draw_create_modal(state);
    } else if (state.modal == modal_mode::password) {
        draw_password_modal(state);
    }
}

}  // namespace multiplayer::view
