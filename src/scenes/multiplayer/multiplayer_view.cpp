#include "multiplayer/multiplayer_view.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "localization/localization.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui/icons/raythm_icons.h"
#include "ui_notice.h"
#include "ui_text_input.h"
#include "virtual_screen.h"

namespace multiplayer::view {
namespace {

constexpr Rectangle kBackButtonRect{39.0f, 983.0f, 330.0f, 58.0f};
constexpr Rectangle kCreateButtonRect{1548.0f, 983.0f, 330.0f, 58.0f};
constexpr Rectangle kListRect{39.0f, 109.0f, 1839.0f, 854.0f};
constexpr Rectangle kListTitleRect{kListRect.x + 30.0f, kListRect.y + 15.0f, 360.0f, 42.0f};
constexpr Rectangle kRefreshButtonRect{kListRect.x + kListRect.width - 78.0f, kListRect.y + 15.0f, 54.0f, 54.0f};
constexpr Rectangle kListViewRect{kListRect.x + 14.0f, kListRect.y + 84.0f, kListRect.width - 28.0f, kListRect.height - 102.0f};
constexpr Rectangle kMemberPanelRect{39.0f, 109.0f, 430.0f, 854.0f};
constexpr Rectangle kQueuePanelRect{500.0f, 109.0f, 820.0f, 854.0f};
constexpr Rectangle kChatPanelRect{1351.0f, 109.0f, 527.0f, 854.0f};
constexpr Rectangle kLeaveButtonRect{39.0f, 983.0f, 430.0f, 58.0f};
constexpr Rectangle kReadyButtonRect{500.0f, 983.0f, 820.0f, 58.0f};
constexpr Rectangle kChatInputRect{1351.0f, 983.0f, 370.0f, 58.0f};
constexpr Rectangle kChatButtonRect{1740.0f, 983.0f, 138.0f, 58.0f};
constexpr Rectangle kQueuePermissionButtonRect{kQueuePanelRect.x + kQueuePanelRect.width - 244.0f,
                                               kQueuePanelRect.y + 15.0f, 220.0f, 42.0f};
constexpr Rectangle kQueuePreviewButtonRect{kQueuePanelRect.x + 18.0f, kQueuePanelRect.y + 138.0f, 48.0f, 48.0f};
constexpr Rectangle kQueuePreviewBarRect{kQueuePanelRect.x + 82.0f, kQueuePanelRect.y + 155.0f,
                                         kQueuePanelRect.width - 116.0f, 14.0f};
constexpr Rectangle kQueueListRect{kQueuePanelRect.x + 16.0f, kQueuePanelRect.y + 210.0f,
                                   kQueuePanelRect.width - 44.0f, kQueuePanelRect.height - 228.0f};
constexpr Rectangle kQueueScrollbarTrackRect{kQueuePanelRect.x + kQueuePanelRect.width - 18.0f,
                                             kQueueListRect.y, 8.0f, kQueueListRect.height};
constexpr float kQueueRowHeight = 86.0f;
constexpr float kQueueRowGap = 10.0f;
constexpr float kQueueWheelStep = 120.0f;
constexpr Rectangle kCreateModalRect{610.0f, 250.0f, 700.0f, 430.0f};
constexpr Rectangle kPasswordModalRect{660.0f, 310.0f, 600.0f, 300.0f};
constexpr int kRoomGridColumns = 3;
constexpr float kRoomCardGap = 16.0f;
constexpr float kRoomCardHeight = 132.0f;
constexpr float kChatMessageGap = 8.0f;
constexpr float kChatMessagePaddingX = 12.0f;
constexpr float kChatMessagePaddingY = 9.0f;
constexpr float kChatLineHeight = 21.0f;

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

bool queue_item_installed(const state& state, const std::string& item_id) {
    return std::find(state.installed_queue_item_ids.begin(), state.installed_queue_item_ids.end(), item_id) !=
           state.installed_queue_item_ids.end();
}

float queue_content_height(const room_detail& room) {
    if (room.queue.empty()) {
        return 0.0f;
    }
    return static_cast<float>(room.queue.size()) * kQueueRowHeight +
           static_cast<float>(room.queue.size() - 1) * kQueueRowGap;
}

ui::notice_tone status_notice_tone(const std::string& message) {
    const std::string lower = [&message]() {
        std::string result = message;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return result;
    }();
    if (lower.find("failed") != std::string::npos ||
        lower.find("required") != std::string::npos ||
        lower.find("missing") != std::string::npos ||
        lower.find("not ") != std::string::npos ||
        lower.find("could") != std::string::npos) {
        return ui::notice_tone::error;
    }
    return ui::notice_tone::info;
}

void notify_status_change(const state& state, const room_detail& room) {
    static std::string last_room_id;
    static std::string last_message;
    if (last_room_id != room.id) {
        last_room_id = room.id;
        last_message.clear();
    }
    if (state.status_message.empty() || state.status_message == last_message) {
        return;
    }
    last_message = state.status_message;
    ui::notify(state.status_message, status_notice_tone(state.status_message), 2.2f);
}

std::string room_status_label(const room_summary& room) {
    const std::string status = localization::tr_literal(room.playing ? "playing" : "lobby");
    return room.host_name + " " + localization::tr_literal("host") + "  " +
           std::to_string(room.player_count) + "/" +
           std::to_string(room.max_players) + "  " + status;
}

std::string format_duration_label(double seconds) {
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    const int total = static_cast<int>(seconds + 0.5);
    const int minutes = total / 60;
    const int secs = total % 60;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, secs);
    return buffer;
}

void draw_panel_title(Rectangle rect, const char* title) {
    ui::draw_text_in_rect(title, 22, {rect.x + 24.0f, rect.y + 15.0f, rect.width - 48.0f, 34.0f},
                          g_theme->text, ui::text_align::left);
}

size_t utf8_codepoint_length(unsigned char ch) {
    if ((ch & 0x80) == 0) {
        return 1;
    }
    if ((ch & 0xE0) == 0xC0) {
        return 2;
    }
    if ((ch & 0xF0) == 0xE0) {
        return 3;
    }
    if ((ch & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

std::vector<std::string> wrap_chat_text(const std::string& text, float max_width, int font_size) {
    std::vector<std::string> lines;
    std::string line;
    for (size_t index = 0; index < text.size();) {
        const unsigned char ch = static_cast<unsigned char>(text[index]);
        const size_t length = std::min(utf8_codepoint_length(ch), text.size() - index);
        const std::string next = text.substr(index, length);
        index += length;

        if (next == "\r") {
            continue;
        }
        if (next == "\n") {
            lines.push_back(line);
            line.clear();
            continue;
        }

        const std::string candidate = line + next;
        if (!line.empty() && ui::measure_text_size(candidate, static_cast<float>(font_size), 0.0f).x > max_width) {
            lines.push_back(line);
            line = next == " " ? "" : next;
        } else {
            line = candidate;
        }
    }
    if (!line.empty() || lines.empty()) {
        lines.push_back(line);
    }
    return lines;
}

Rectangle centered_icon_rect(Rectangle rect, float inset) {
    const float size = std::max(1.0f, std::min(rect.width, rect.height) - inset * 2.0f);
    return {
        rect.x + (rect.width - size) * 0.5f,
        rect.y + (rect.height - size) * 0.5f,
        size,
        size
    };
}

Color member_presence_color(const room_member& member) {
    if (member.status == "PLAYING") {
        return g_theme->accent;
    }
    return member.connected ? g_theme->success : g_theme->text_dim;
}

void draw_member_presence_icon(Rectangle rect, const room_member& member) {
    const Color color = member_presence_color(member);
    if (member.status == "PLAYING") {
        raythm_icons::draw_play(centered_icon_rect(rect, 4.0f), color, 3.0f);
    } else if (member.connected) {
        raythm_icons::draw_note_tap(centered_icon_rect(rect, 4.0f), color, 3.0f);
    } else {
        raythm_icons::draw_clock_3(centered_icon_rect(rect, 2.0f), color, 3.0f);
    }
}

ui::button_state draw_refresh_icon_button(Rectangle rect) {
    const ui::button_state button = ui::draw_button_colored(rect, "", 18,
                                                            g_theme->row_soft,
                                                            g_theme->row_soft_hover,
                                                            g_theme->text,
                                                            1.5f);
    const Rectangle visual = button.pressed ? ui::inset(rect, 1.5f) : rect;
    raythm_icons::draw_refresh(centered_icon_rect(visual, 13.0f),
                               button.hovered ? g_theme->text : g_theme->text_muted,
                               3.0f);
    return button;
}

ui::button_state draw_icon_button(Rectangle rect,
                                  void (*draw_icon)(Rectangle, Color, float),
                                  Color bg,
                                  Color bg_hover,
                                  Color icon,
                                  bool enabled = true,
                                  float inset = 9.0f,
                                  Color icon_hover = {0, 0, 0, 0}) {
    const ui::button_state button = ui::draw_button_colored(
        rect, "", 18,
        enabled ? bg : g_theme->panel,
        enabled ? bg_hover : g_theme->panel,
        enabled ? icon : g_theme->text_dim,
        1.5f);
    const Rectangle visual = button.pressed ? ui::inset(rect, 1.5f) : rect;
    const Color hover_icon = icon_hover.a > 0 ? icon_hover : g_theme->text;
    draw_icon(centered_icon_rect(visual, inset),
              enabled ? (button.hovered ? hover_icon : icon) : g_theme->text_dim,
              3.0f);
    return button;
}

void draw_queue_preview_controls(state& state, const room_detail& room) {
    const bool has_queue = !room.queue.empty();
    const bool available = has_queue && state.queue_preview_available;
    const Color muted = available ? g_theme->text_muted : with_alpha(g_theme->text_muted, 120);
    const ui::button_state toggle =
        draw_icon_button(kQueuePreviewButtonRect,
                         state.queue_preview_playing ? raythm_icons::draw_pause : raythm_icons::draw_play,
                         available ? g_theme->row : g_theme->panel,
                         available ? g_theme->row_hover : g_theme->panel,
                         muted,
                         available,
                         11.0f,
                         g_theme->text);
    if (toggle.clicked && available) {
        state.command = ui_command::toggle_queue_preview;
    }

    const double duration = state.queue_preview_duration_seconds;
    const double position = state.queue_preview_position_seconds;
    const float ratio = duration > 0.0
        ? std::clamp(static_cast<float>(position / duration), 0.0f, 1.0f)
        : 0.0f;
    ui::draw_rect_f(kQueuePreviewBarRect, with_alpha(g_theme->bg_alt, 220));
    ui::draw_rect_f({kQueuePreviewBarRect.x, kQueuePreviewBarRect.y,
                     kQueuePreviewBarRect.width * ratio, kQueuePreviewBarRect.height},
                    available ? with_alpha(g_theme->accent, 220) : with_alpha(g_theme->text_muted, 80));
    ui::draw_rect_lines(kQueuePreviewBarRect, 1.0f, with_alpha(g_theme->border_light, 190));
    const Rectangle hit{kQueuePreviewBarRect.x, kQueuePreviewBarRect.y - 12.0f,
                        kQueuePreviewBarRect.width, kQueuePreviewBarRect.height + 24.0f};
    if (available && duration > 0.0 && ui::is_clicked(hit)) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        const float seek_ratio = std::clamp((mouse.x - kQueuePreviewBarRect.x) / kQueuePreviewBarRect.width, 0.0f, 1.0f);
        state.queue_preview_seek_seconds = static_cast<double>(seek_ratio) * duration;
        state.queue_preview_seek_requested = true;
    }

    const std::string label = available
        ? format_duration_label(position) + " / " + format_duration_label(duration)
        : (has_queue ? localization::tr_literal("Not installed") : localization::tr_literal("No queued songs"));
    ui::draw_text_in_rect(label.c_str(), 13,
                          {kQueuePreviewBarRect.x, kQueuePreviewBarRect.y + 18.0f,
                           kQueuePreviewBarRect.width, 20.0f},
                          muted, ui::text_align::right);
}

void draw_room_card(state& state, const room_summary& room, int index) {
    const int column = index % kRoomGridColumns;
    const int row_index = index / kRoomGridColumns;
    const float card_width =
        (kListViewRect.width - kRoomCardGap * static_cast<float>(kRoomGridColumns - 1)) /
        static_cast<float>(kRoomGridColumns);
    const Rectangle row{
        kListViewRect.x + static_cast<float>(column) * (card_width + kRoomCardGap),
        kListViewRect.y + static_cast<float>(row_index) * (kRoomCardHeight + kRoomCardGap),
        card_width,
        kRoomCardHeight,
    };
    const ui::row_state row_state =
        ui::draw_row(row, g_theme->row, g_theme->row_hover, room.locked ? g_theme->slow : g_theme->border);
    const std::string lock = room.locked ? "[LOCK] " : "";
    const std::string title = lock + room.name;
    const std::string meta = room_status_label(room);
    ui::draw_text_in_rect(title.c_str(), 25, {row.x + 24.0f, row.y + 14.0f, row.width - 48.0f, 34.0f},
                          g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(meta.c_str(), 18, {row.x + 24.0f, row.y + 52.0f, row.width - 48.0f, 24.0f},
                          g_theme->text_muted, ui::text_align::left);
    ui::draw_text_in_rect(room.chart_title.empty() ? localization::tr_literal("No chart selected") : room.chart_title.c_str(),
                          18, {row.x + 24.0f, row.y + 90.0f, row.width - 48.0f, 24.0f},
                          g_theme->text_muted, ui::text_align::left);
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
    if (ui::draw_button_colored(kBackButtonRect, localization::tr(localization::text_key::back), 16,
                                g_theme->row_soft, g_theme->row_soft_hover, g_theme->text, 1.5f).clicked) {
        state.command = ui_command::back_to_home;
    }
    if (ui::draw_button_colored(kCreateButtonRect, localization::tr_literal("Create Room"), 18,
                                state.auth.logged_in ? g_theme->accent : g_theme->row,
                                state.auth.logged_in ? g_theme->row_active : g_theme->row_hover,
                                state.auth.logged_in ? g_theme->text : g_theme->text_muted).clicked &&
        state.auth.logged_in) {
        state.command = ui_command::open_create_room;
    }

    ui::draw_panel(kListRect);
    ui::draw_text_in_rect(localization::tr_literal("Rooms"), 28, kListTitleRect, g_theme->text, ui::text_align::left);
    if (draw_refresh_icon_button(kRefreshButtonRect).clicked) {
        state.command = ui_command::refresh_rooms;
    }
    if (!state.auth.logged_in) {
        ui::draw_text_in_rect(localization::tr_literal("Sign in from the account menu before joining multiplayer."),
                              24, kListViewRect, g_theme->text_muted);
        return;
    }

    int index = 0;
    for (const room_summary& room : state.rooms) {
        if (index >= 8) {
            break;
        }
        draw_room_card(state, room, index++);
    }
    if (state.rooms.empty() && !state.loading_rooms) {
        ui::draw_text_in_rect(localization::tr_literal("No rooms yet."), 24, kListViewRect, g_theme->text_muted);
    }
}

void draw_members(const state& state, const room_detail& room) {
    ui::draw_panel(kMemberPanelRect);
    draw_panel_title(kMemberPanelRect, localization::tr_literal("Players"));
    Rectangle row{kMemberPanelRect.x + 16.0f, kMemberPanelRect.y + 72.0f, kMemberPanelRect.width - 32.0f, 56.0f};
    for (const room_member& member : room.members) {
        if (!visible_member_status(member.status)) {
            continue;
        }
        const bool self = (!state.self_user_id.empty() && member.user_id == state.self_user_id);
        ui::draw_row(row, self ? g_theme->row_selected : g_theme->row, g_theme->row_hover,
                     member.role == "HOST" ? g_theme->slow : g_theme->border, 1.5f);
        const bool host = member.role == "HOST";
        const std::string name = member.display_name;
        const Rectangle host_icon{row.x + row.width - 150.0f, row.y + 14.0f, 28.0f, 28.0f};
        ui::draw_text_in_rect(name.c_str(), 19,
                              {row.x + 14.0f, row.y, row.width - (host ? 172.0f : 132.0f), row.height},
                              g_theme->text, ui::text_align::left);
        if (host) {
            raythm_icons::draw_crown(centered_icon_rect(host_icon, 2.0f), g_theme->slow, 3.0f);
        }
        const Rectangle presence_icon{row.x + row.width - 104.0f, row.y + 14.0f, 28.0f, 28.0f};
        const Rectangle ready_icon{row.x + row.width - 56.0f, row.y + 14.0f, 28.0f, 28.0f};
        draw_member_presence_icon(presence_icon, member);
        if (member.ready) {
            raythm_icons::draw_circle_check(centered_icon_rect(ready_icon, 2.0f), g_theme->success, 3.0f);
        } else {
            raythm_icons::draw_clock_3(centered_icon_rect(ready_icon, 2.0f), g_theme->text_muted, 3.0f);
        }
        row.y += row.height + 10.0f;
        if (row.y + row.height > kMemberPanelRect.y + kMemberPanelRect.height - 12.0f) {
            break;
        }
    }
}

void draw_queue(state& state, const room_detail& room) {
    ui::draw_panel(kQueuePanelRect);
    draw_panel_title(kQueuePanelRect, localization::tr_literal("Beatmap queue"));
    const bool self_host = is_self_host(state, room);
    if (self_host) {
        const bool host_only = room.queue_permission == "HOST_ONLY";
        const std::string permission_label = localization::tr_literal(
            host_only ? "Queue: host only" : "Queue: all players");
        if (ui::draw_button_colored(kQueuePermissionButtonRect, permission_label.c_str(), 15,
                                    g_theme->row, g_theme->row_hover, g_theme->text).clicked &&
            !busy(state)) {
            state.command = ui_command::toggle_queue_permission;
        }
    }

    const Rectangle add_rect{kQueuePanelRect.x + 16.0f, kQueuePanelRect.y + 72.0f, kQueuePanelRect.width - 32.0f, 50.0f};
    if (ui::draw_button_colored(add_rect, localization::tr_literal("Add song"), 17,
                                g_theme->accent, g_theme->row_hover, g_theme->text).clicked &&
        !busy(state)) {
        state.command = ui_command::open_song_select;
    }
    draw_queue_preview_controls(state, room);

    if (room.queue.empty()) {
        ui::draw_text_in_rect(localization::tr_literal("No queued songs yet."), 21,
                              {kQueueListRect.x, kQueueListRect.y + 80.0f, kQueueListRect.width, kQueueListRect.height - 80.0f},
                              g_theme->text_muted);
        return;
    }

    const float max_scroll = std::max(0.0f, queue_content_height(room) - kQueueListRect.height);
    if (ui::is_hovered(kQueueListRect)) {
        state.queue_scroll_y_target -= GetMouseWheelMove() * kQueueWheelStep;
    }
    state.queue_scroll_y_target = std::clamp(state.queue_scroll_y_target, 0.0f, max_scroll);
    state.queue_scroll_y = std::clamp(state.queue_scroll_y, 0.0f, max_scroll);
    if (std::abs(state.queue_scroll_y - state.queue_scroll_y_target) < 0.5f) {
        state.queue_scroll_y = state.queue_scroll_y_target;
    } else {
        state.queue_scroll_y += (state.queue_scroll_y_target - state.queue_scroll_y) * 0.32f;
    }

    {
        ui::scoped_clip_rect clip(kQueueListRect);
        int queue_index = 0;
        for (const room_queue_item& item : room.queue) {
            Rectangle row{
                kQueueListRect.x,
                kQueueListRect.y + static_cast<float>(queue_index) * (kQueueRowHeight + kQueueRowGap) - state.queue_scroll_y,
                kQueueListRect.width,
                kQueueRowHeight,
            };
            if (row.y + row.height < kQueueListRect.y || row.y > kQueueListRect.y + kQueueListRect.height) {
                ++queue_index;
                continue;
            }
            const bool has_level = item.level > 0.0f;
            const Color level_color = has_level ? difficulty_level_color(item.level) : g_theme->accent;
            const bool hovered = ui::is_hovered(row);
            const Color base_fill = lerp_color(g_theme->bg_alt, level_color, has_level ? 0.08f : 0.0f);
            const Color hover_fill = lerp_color(g_theme->row_hover, level_color, has_level ? 0.10f : 0.0f);
            ui::draw_rect_f(row, hovered ? hover_fill : base_fill);
            ui::draw_rect_f({row.x, row.y, 4.0f, row.height}, level_color);
            ui::draw_rect_f({row.x + 4.0f, row.y, row.width - 4.0f, 1.0f}, g_theme->border_light);
            ui::draw_rect_f({row.x + 4.0f, row.y + row.height - 1.0f, row.width - 4.0f, 1.0f},
                            with_alpha(g_theme->border_light, 170));
            const bool can_remove = self_host || (!state.self_user_id.empty() && item.requested_by_user_id == state.self_user_id);
            const bool installed = queue_item_installed(state, item.id);
            const bool can_download = !installed && !item.song_id.empty() && !item.chart_id.empty();
            const float action_width = (self_host ? 100.0f : 0.0f) +
                                       (can_remove ? 50.0f : 0.0f) +
                                       (can_download ? 50.0f : 0.0f);
            const std::string title = item.song_title.empty() ? item.chart_id : item.song_title;
            ui::draw_text_in_rect(title.c_str(), 19, {row.x + 18.0f, row.y + 10.0f, row.width - 32.0f - action_width, 28.0f},
                                  g_theme->text, ui::text_align::left);
            const std::string meta = item.difficulty_name + (item.requested_by.empty() ? "" : "  by " + item.requested_by);
            ui::draw_text_in_rect(meta.c_str(), 15, {row.x + 18.0f, row.y + 42.0f, row.width - 130.0f - action_width, 24.0f},
                                  g_theme->text_muted, ui::text_align::left);
            const Rectangle level_badge_rect{row.x + row.width - action_width - 106.0f, row.y + 38.0f, 78.0f, 28.0f};
            if (has_level) {
                draw_difficulty_level_badge(item.level, level_badge_rect, 13, 255);
            } else {
                ui::draw_text_in_rect("Lv.-", 13, level_badge_rect, g_theme->text_muted, ui::text_align::center);
            }
            ui::draw_text_in_rect(localization::tr_literal(installed ? "Installed" : "Not installed"), 13,
                                  {row.x + 18.0f, row.y + 64.0f, 160.0f, 18.0f},
                                  installed ? g_theme->success : g_theme->text_muted, ui::text_align::left);
            float action_x = row.x + row.width - 16.0f;
            if (can_remove) {
                action_x -= 42.0f;
                if (draw_icon_button({action_x, row.y + 22.0f, 42.0f, 42.0f},
                                     raythm_icons::draw_trash_2,
                                     lerp_color(g_theme->row, g_theme->error, 0.08f),
                                     lerp_color(g_theme->row_hover, g_theme->error, 0.18f),
                                     g_theme->error,
                                     true,
                                     8.0f,
                                     lerp_color(g_theme->error, WHITE, 0.18f)).clicked &&
                    !busy(state)) {
                    state.selected_queue_item_id = item.id;
                    state.command = ui_command::remove_queue_item;
                }
                action_x -= 8.0f;
            }
            if (self_host) {
                const bool can_move_up = queue_index > 0;
                const bool can_move_down = queue_index + 1 < static_cast<int>(room.queue.size());
                action_x -= 48.0f;
                if (draw_icon_button({action_x, row.y + 22.0f, 42.0f, 42.0f},
                                     raythm_icons::draw_chevron_down,
                                     g_theme->row, g_theme->row_hover, g_theme->text_muted,
                                     can_move_down, 8.0f).clicked &&
                    can_move_down && !busy(state)) {
                    state.selected_queue_item_id = item.id;
                    state.command = ui_command::move_queue_item_down;
                }
                action_x -= 46.0f;
                if (draw_icon_button({action_x, row.y + 22.0f, 42.0f, 42.0f},
                                     raythm_icons::draw_chevron_up,
                                     g_theme->row, g_theme->row_hover, g_theme->text_muted,
                                     can_move_up, 8.0f).clicked &&
                    can_move_up && !busy(state)) {
                    state.selected_queue_item_id = item.id;
                    state.command = ui_command::move_queue_item_up;
                }
                action_x -= 8.0f;
            }
            if (can_download) {
                action_x -= 42.0f;
                if (draw_icon_button({action_x, row.y + 22.0f, 42.0f, 42.0f},
                                     raythm_icons::draw_download,
                                     g_theme->accent, g_theme->row_hover, g_theme->text, true, 8.0f).clicked &&
                    !busy(state)) {
                    state.requested_download_song_id = item.song_id;
                    state.requested_download_chart_id = item.chart_id;
                    state.current_queue_download_requested = true;
                }
            }
            ++queue_index;
        }
    }
    ui::draw_scrollbar(kQueueScrollbarTrackRect,
                       queue_content_height(room),
                       state.queue_scroll_y,
                       g_theme->scrollbar_track,
                       g_theme->scrollbar_thumb);
}

void draw_chat(state& state, const room_detail& room) {
    ui::draw_panel(kChatPanelRect);
    draw_panel_title(kChatPanelRect, localization::tr_literal("Chat"));
    struct chat_row {
        std::vector<std::string> lines;
        float height = 0.0f;
    };
    std::vector<chat_row> rows;
    const Rectangle message_area{kChatPanelRect.x + 16.0f, kChatPanelRect.y + 66.0f,
                                 kChatPanelRect.width - 32.0f, kChatPanelRect.height - 86.0f};
    const float text_width = message_area.width - kChatMessagePaddingX * 2.0f;
    float total_height = 0.0f;
    for (int i = static_cast<int>(room.chat.size()) - 1; i >= 0; --i) {
        const chat_message& message = room.chat[static_cast<size_t>(i)];
        chat_row row;
        row.lines = wrap_chat_text(message.display_name + ": " + message.message, text_width, 16);
        row.height = std::max(42.0f,
                              kChatMessagePaddingY * 2.0f +
                              static_cast<float>(row.lines.size()) * kChatLineHeight);
        const float next_total = total_height + (rows.empty() ? 0.0f : kChatMessageGap) + row.height;
        if (next_total > message_area.height && !rows.empty()) {
            break;
        }
        total_height = next_total;
        rows.push_back(std::move(row));
    }
    std::reverse(rows.begin(), rows.end());
    float y = message_area.y + std::max(0.0f, message_area.height - total_height);
    for (const chat_row& row : rows) {
        const Rectangle box{message_area.x, y, message_area.width, row.height};
        ui::draw_rect_f(box, with_alpha(g_theme->row, 190));
        ui::draw_rect_lines(box, 1.0f, with_alpha(g_theme->border_light, 190));
        float text_y = box.y + kChatMessagePaddingY;
        for (const std::string& line : row.lines) {
            ui::draw_text_f(line.c_str(), box.x + kChatMessagePaddingX, text_y, 16, g_theme->text_muted);
            text_y += kChatLineHeight;
        }
        y += row.height + kChatMessageGap;
    }
    const ui::text_input_result chat_result =
        ui::draw_text_input(kChatInputRect, state.chat_input, "", localization::tr_literal("Message..."), nullptr,
                            ui::draw_layer::base, 16, 160, ui::default_text_input_filter, 0.0f,
                            false, true, false, false, ui::text_align::left);
    if (chat_result.submitted) {
        state.command = ui_command::send_chat;
    }
    if (ui::draw_button(kChatButtonRect, localization::tr_literal("Send"), 18).clicked) {
        state.command = ui_command::send_chat;
    }
}

void draw_room(state& state) {
    if (!state.current_room.has_value()) {
        return;
    }
    const room_detail& room = *state.current_room;
    notify_status_change(state, room);

    draw_members(state, room);
    draw_queue(state, room);
    draw_chat(state, room);

    if (ui::draw_button(kLeaveButtonRect, localization::tr_literal("Leave"), 20).clicked) {
        state.command = ui_command::leave_room;
    }
    const bool queue_ready = !room.queue.empty() && state.current_queue_chart_installed;
    const bool can_start = is_self_host(state, room) && queue_ready && all_joined_members_ready(room);
    const std::string action_label = can_start ? localization::tr_literal("Start") : localization::tr_literal(state.local_ready ? "Cancel Ready"
        : (room.queue.empty() ? "Ready (queue empty)"
        : (state.current_queue_chart_installed ? "Ready" : "Ready (download needed)")));
    const Color action_bg = can_start ? g_theme->accent :
        (queue_ready ? (state.local_ready ? g_theme->row_selected : g_theme->success) : g_theme->row);
    const bool action_enabled = can_start || queue_ready;
    if (ui::draw_button_colored(kReadyButtonRect, action_label.c_str(), 22,
                                action_bg, g_theme->row_hover,
                                action_enabled ? g_theme->text : g_theme->text_muted).clicked &&
        action_enabled) {
        state.command = can_start ? ui_command::start_match : ui_command::toggle_ready;
    }
}

void draw_create_modal(state& state) {
    ui::register_hit_region(kCreateModalRect, ui::draw_layer::modal);
    ui::draw_fullscreen_overlay(with_alpha(BLACK, 130));
    ui::draw_panel(kCreateModalRect);
    ui::draw_text_in_rect(localization::tr_literal("Create Room"), 28,
                          {kCreateModalRect.x + 26.0f, kCreateModalRect.y + 22.0f,
                           kCreateModalRect.width - 52.0f, 36.0f},
                          g_theme->text, ui::text_align::left);
    const ui::text_input_result name_result =
        ui::draw_text_input({kCreateModalRect.x + 40.0f, kCreateModalRect.y + 90.0f, 620.0f, 56.0f},
                            state.create_name_input, localization::tr_literal("Name"), localization::tr_literal("Room name"), nullptr,
                            ui::draw_layer::modal, 18, 80, ui::default_text_input_filter, 110.0f);
    const ui::text_input_result password_result =
        ui::draw_text_input({kCreateModalRect.x + 40.0f, kCreateModalRect.y + 162.0f, 620.0f, 56.0f},
                            state.create_password_input, localization::tr_literal("Password"), localization::tr_literal("Optional"), nullptr,
                            ui::draw_layer::modal, 18, 128, ui::default_text_input_filter, 110.0f, true);
    if ((name_result.submitted || password_result.submitted) && !busy(state)) {
        state.command = ui_command::submit_create_room;
    }

    const Rectangle max_rect{kCreateModalRect.x + 40.0f, kCreateModalRect.y + 238.0f, 280.0f, 48.0f};
    const Rectangle host_rect{kCreateModalRect.x + 340.0f, kCreateModalRect.y + 238.0f, 320.0f, 48.0f};
    const std::string max_label = std::string(localization::tr_literal("Players:")) + " " + std::to_string(state.create_max_players);
    const ui::selector_state max_selector = ui::draw_value_selector(max_rect, localization::tr_literal("Max"), max_label.c_str(),
                                                                    ui::draw_layer::modal, 17, 32.0f, 82.0f);
    if (max_selector.left.clicked) {
        state.create_max_players = std::max(2, state.create_max_players - 1);
    }
    if (max_selector.right.clicked) {
        state.create_max_players = std::min(8, state.create_max_players + 1);
    }
    const ui::button_state host_button =
        ui::enqueue_button(host_rect,
                           localization::tr_literal(state.create_host_only ? "Queue: host only" : "Queue: all players"),
                           17,
                           ui::draw_layer::modal);
    if (host_button.clicked) {
        state.create_host_only = !state.create_host_only;
    }

    const ui::button_state cancel =
        ui::enqueue_button({kCreateModalRect.x + 40.0f, kCreateModalRect.y + 342.0f, 180.0f, 54.0f},
                           localization::tr(localization::text_key::cancel), 18, ui::draw_layer::modal);
    const ui::button_state create =
        ui::enqueue_button({kCreateModalRect.x + 250.0f, kCreateModalRect.y + 342.0f, 410.0f, 54.0f},
                           busy(state) ? localization::tr(localization::text_key::creating) : localization::tr(localization::text_key::create), 18, ui::draw_layer::modal);
    if (cancel.clicked) {
        state.command = ui_command::cancel_modal;
    }
    if (create.clicked && !busy(state)) {
        state.command = ui_command::submit_create_room;
    }
}

void draw_password_modal(state& state) {
    ui::register_hit_region(kPasswordModalRect, ui::draw_layer::modal);
    ui::draw_fullscreen_overlay(with_alpha(BLACK, 130));
    ui::draw_panel(kPasswordModalRect);
    ui::draw_text_in_rect(localization::tr_literal("Room password"), 28,
                          {kPasswordModalRect.x + 26.0f, kPasswordModalRect.y + 24.0f,
                           kPasswordModalRect.width - 52.0f, 36.0f},
                          g_theme->text, ui::text_align::left);
    const ui::text_input_result password_result =
        ui::draw_text_input({kPasswordModalRect.x + 40.0f, kPasswordModalRect.y + 96.0f, 520.0f, 56.0f},
                            state.join_password_input, localization::tr_literal("Password"), "", nullptr,
                            ui::draw_layer::modal, 18, 128, ui::default_text_input_filter, 120.0f, true);
    if (password_result.submitted && !busy(state)) {
        state.command = ui_command::submit_password;
    }
    const ui::button_state cancel =
        ui::enqueue_button({kPasswordModalRect.x + 40.0f, kPasswordModalRect.y + 206.0f, 170.0f, 54.0f},
                           localization::tr(localization::text_key::cancel), 18, ui::draw_layer::modal);
    const ui::button_state join =
        ui::enqueue_button({kPasswordModalRect.x + 240.0f, kPasswordModalRect.y + 206.0f, 320.0f, 54.0f},
                           busy(state) ? localization::tr_literal("Joining...") : localization::tr_literal("Join"), 18, ui::draw_layer::modal);
    if (cancel.clicked) {
        state.command = ui_command::cancel_modal;
    }
    if (join.clicked && !busy(state)) {
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
