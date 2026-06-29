#include "multiplayer/multiplayer_view.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "localization/localization.h"
#include "scene_common.h"
#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_layout.h"
#include "ui_scroll.h"
#include "ui/icons/raythm_icons.h"
#include "ui_notice.h"
#include "ui_text_input.h"
#include "virtual_screen.h"

namespace multiplayer::view {
namespace {

constexpr Rectangle kFooterRect{39.0f, 983.0f, 1839.0f, 58.0f};
constexpr Rectangle kBackButtonRect = ui::split_columns(kFooterRect, 330.0f).first;
constexpr Rectangle kCreateButtonRect = ui::split_trailing(kFooterRect, 330.0f).second;
constexpr Rectangle kListRect{39.0f, 109.0f, 1839.0f, 854.0f};
constexpr Rectangle kListTitleRect{kListRect.x + 30.0f, kListRect.y + 15.0f, 360.0f, 42.0f};
constexpr Rectangle kRefreshButtonRect = ui::split_trailing(
    {kListRect.x + 24.0f, kListRect.y + 15.0f, kListRect.width - 48.0f, 54.0f},
    54.0f).second;
constexpr Rectangle kListViewRect{kListRect.x + 14.0f, kListRect.y + 84.0f, kListRect.width - 28.0f, kListRect.height - 102.0f};
constexpr Rectangle kMemberPanelRect{39.0f, 109.0f, 430.0f, 854.0f};
constexpr Rectangle kInviteFriendsButtonRect = ui::split_trailing(
    {kMemberPanelRect.x + 24.0f, kMemberPanelRect.y + 15.0f,
     kMemberPanelRect.width - 48.0f, 42.0f},
    122.0f).second;
constexpr Rectangle kQueuePanelRect{500.0f, 109.0f, 820.0f, 854.0f};
constexpr Rectangle kChatPanelRect{1351.0f, 109.0f, 527.0f, 854.0f};
constexpr ui::rect_pair kRoomFooterLead = ui::split_columns(kFooterRect, 430.0f, 31.0f);
constexpr ui::rect_pair kRoomFooterTail = ui::split_columns(kRoomFooterLead.second, 820.0f, 31.0f);
constexpr Rectangle kLeaveButtonRect = kRoomFooterLead.first;
constexpr Rectangle kReadyButtonRect = kRoomFooterTail.first;
constexpr Rectangle kChatComposerRect = kRoomFooterTail.second;
constexpr float kChatSendButtonWidth = 138.0f;
constexpr float kChatComposerGap = 19.0f;
constexpr Rectangle kQueuePermissionButtonRect = ui::split_trailing(
    {kQueuePanelRect.x + 24.0f, kQueuePanelRect.y + 15.0f,
     kQueuePanelRect.width - 48.0f, 42.0f},
    220.0f).second;
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
constexpr Rectangle kInviteModalRect{560.0f, 210.0f, 800.0f, 620.0f};
constexpr float kInviteFriendRowHeight = 72.0f;
constexpr float kInviteFriendRowGap = 10.0f;
constexpr int kRoomGridColumns = 3;
constexpr float kRoomCardGap = 16.0f;
constexpr float kRoomCardHeight = 132.0f;
constexpr float kChatMessageGap = 8.0f;
constexpr float kChatMessagePaddingX = 12.0f;
constexpr float kChatMessagePaddingY = 9.0f;
constexpr float kChatLineHeight = 21.0f;
constexpr float kMemberAvatarSize = 40.0f;
constexpr float kMemberRowHeight = 56.0f;
constexpr float kMemberRowGap = 10.0f;

struct room_card_layout {
    Rectangle card = {};
    Rectangle title = {};
    Rectangle meta = {};
    Rectangle chart = {};
};

struct member_row_layout {
    Rectangle row = {};
    Rectangle avatar = {};
    Rectangle name = {};
    Rectangle host_icon = {};
    Rectangle presence_icon = {};
    Rectangle ready_icon = {};
};

struct queue_preview_layout {
    Rectangle toggle = {};
    Rectangle bar = {};
    Rectangle seek_hit = {};
    Rectangle time_label = {};
};

struct queue_row_layout {
    Rectangle row = {};
    Rectangle accent = {};
    Rectangle top_divider = {};
    Rectangle bottom_divider = {};
    Rectangle title = {};
    Rectangle meta = {};
    Rectangle level_badge = {};
    Rectangle install_status = {};
};

struct chat_panel_layout {
    Rectangle message_area = {};
    Rectangle composer_input = {};
    Rectangle send_button = {};
};

struct create_room_modal_layout {
    Rectangle title = {};
    Rectangle name_input = {};
    Rectangle password_input = {};
    Rectangle options = {};
    Rectangle max_players = {};
    Rectangle host_only = {};
    std::array<Rectangle, 2> footer_buttons = {};
};

struct password_modal_layout {
    Rectangle title = {};
    Rectangle password_input = {};
    std::array<Rectangle, 2> footer_buttons = {};
};

struct invite_modal_layout {
    Rectangle title = {};
    Rectangle subtitle = {};
    Rectangle close_button = {};
    Rectangle list = {};
    Rectangle viewport = {};
};

struct invite_friend_row_layout {
    Rectangle row = {};
    Rectangle avatar = {};
    Rectangle name = {};
    Rectangle status = {};
    Rectangle invite_button = {};
};

struct room_card_interaction {
    bool selected = false;
    std::string room_id;
    bool locked = false;
};

struct member_panel_interaction {
    bool invite_requested = false;
    std::string profile_user_id;
};

struct queue_preview_interaction {
    ui_command command = ui_command::none;
    bool seek_requested = false;
    double seek_seconds = 0.0;
};

struct queue_panel_interaction {
    ui_command command = ui_command::none;
    bool scroll_updated = false;
    float queue_scroll_y = 0.0f;
    float queue_scroll_y_target = 0.0f;
    bool seek_requested = false;
    double seek_seconds = 0.0;
    std::string selected_queue_item_id;
    bool download_requested = false;
    std::string download_song_id;
    std::string download_chart_id;
};

struct chat_panel_interaction {
    bool send_requested = false;
};

struct room_footer_interaction {
    ui_command command = ui_command::none;
};

struct room_list_interaction {
    ui_command command = ui_command::none;
    std::string selected_room_id;
    bool password_required = false;
};

struct create_room_modal_interaction {
    ui_command command = ui_command::none;
    bool max_players_changed = false;
    int max_players = 4;
    bool toggle_host_only = false;
};

struct modal_command_interaction {
    ui_command command = ui_command::none;
};

struct invite_friends_modal_interaction {
    ui_command command = ui_command::none;
    std::string selected_user_id;
};

struct queue_row_action_button {
    ui_command command = ui_command::none;
    Rectangle rect{};
    void (*draw_icon)(Rectangle, Color, float) = nullptr;
    Color bg{};
    Color bg_hover{};
    Color icon{};
    Color icon_hover{};
    bool enabled = true;
    float inset = 8.0f;
};

struct invite_friend_action_button {
    std::string user_id;
    Rectangle rect{};
    const char* label = "";
    bool enabled = true;
};

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
    return ui::vertical_list_content_height(room.queue.size(), kQueueRowHeight, kQueueRowGap);
}

constexpr Rectangle queue_row_rect(int index, float scroll_y) {
    return ui::vertical_list_row_rect(kQueueListRect, index, kQueueRowHeight, kQueueRowGap, scroll_y);
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

room_card_layout room_card_layout_for(int index) {
    const float card_width =
        (kListViewRect.width - kRoomCardGap * static_cast<float>(kRoomGridColumns - 1)) /
        static_cast<float>(kRoomGridColumns);
    const Rectangle card = ui::vertical_grid_item_rect(
        kListViewRect,
        index,
        kRoomGridColumns,
        card_width,
        kRoomCardHeight,
        kRoomCardGap,
        kRoomCardGap,
        0.0f);
    return {
        .card = card,
        .title = {card.x + 24.0f, card.y + 14.0f, card.width - 48.0f, 34.0f},
        .meta = {card.x + 24.0f, card.y + 52.0f, card.width - 48.0f, 24.0f},
        .chart = {card.x + 24.0f, card.y + 90.0f, card.width - 48.0f, 24.0f},
    };
}

member_row_layout member_row_layout_for(Rectangle row, bool host) {
    return {
        .row = row,
        .avatar = {row.x + 12.0f,
                   row.y + (row.height - kMemberAvatarSize) * 0.5f,
                   kMemberAvatarSize,
                   kMemberAvatarSize},
        .name = {row.x + 64.0f, row.y, row.width - (host ? 222.0f : 182.0f), row.height},
        .host_icon = {row.x + row.width - 150.0f, row.y + 14.0f, 28.0f, 28.0f},
        .presence_icon = {row.x + row.width - 104.0f, row.y + 14.0f, 28.0f, 28.0f},
        .ready_icon = {row.x + row.width - 56.0f, row.y + 14.0f, 28.0f, 28.0f},
    };
}

constexpr Rectangle member_rows_viewport() {
    return {
        kMemberPanelRect.x + 16.0f,
        kMemberPanelRect.y + 72.0f,
        kMemberPanelRect.width - 32.0f,
        kMemberPanelRect.height - 84.0f,
    };
}

constexpr Rectangle member_row_rect(int index) {
    return ui::vertical_list_row_rect(member_rows_viewport(), index, kMemberRowHeight, kMemberRowGap, 0.0f);
}

queue_preview_layout queue_preview_layout_for() {
    return {
        .toggle = kQueuePreviewButtonRect,
        .bar = kQueuePreviewBarRect,
        .seek_hit = {kQueuePreviewBarRect.x, kQueuePreviewBarRect.y - 12.0f,
                     kQueuePreviewBarRect.width, kQueuePreviewBarRect.height + 24.0f},
        .time_label = {kQueuePreviewBarRect.x, kQueuePreviewBarRect.y + 18.0f,
                       kQueuePreviewBarRect.width, 20.0f},
    };
}

queue_row_layout queue_row_layout_for(Rectangle row, float action_width) {
    return {
        .row = row,
        .accent = {row.x, row.y, 4.0f, row.height},
        .top_divider = {row.x + 4.0f, row.y, row.width - 4.0f, 1.0f},
        .bottom_divider = {row.x + 4.0f, row.y + row.height - 1.0f, row.width - 4.0f, 1.0f},
        .title = {row.x + 18.0f, row.y + 10.0f, row.width - 32.0f - action_width, 28.0f},
        .meta = {row.x + 18.0f, row.y + 42.0f, row.width - 130.0f - action_width, 24.0f},
        .level_badge = {row.x + row.width - action_width - 106.0f, row.y + 38.0f, 78.0f, 28.0f},
        .install_status = {row.x + 18.0f, row.y + 64.0f, 160.0f, 18.0f},
    };
}

Rectangle queue_row_action_rect(Rectangle row, float right_x) {
    return {right_x, row.y + 22.0f, 42.0f, 42.0f};
}

std::vector<queue_row_action_button> queue_row_action_buttons_for(Rectangle row,
                                                                  float start_x,
                                                                  bool can_remove,
                                                                  bool self_host,
                                                                  bool can_move_up,
                                                                  bool can_move_down,
                                                                  bool can_download) {
    std::vector<queue_row_action_button> buttons;
    float action_x = start_x;
    if (can_remove) {
        action_x -= 42.0f;
        buttons.push_back({
            .command = ui_command::remove_queue_item,
            .rect = queue_row_action_rect(row, action_x),
            .draw_icon = raythm_icons::draw_trash_2,
            .bg = lerp_color(g_theme->row, g_theme->error, 0.08f),
            .bg_hover = lerp_color(g_theme->row_hover, g_theme->error, 0.18f),
            .icon = g_theme->error,
            .icon_hover = lerp_color(g_theme->error, WHITE, 0.18f),
            .enabled = true,
            .inset = 8.0f,
        });
        action_x -= 8.0f;
    }
    if (self_host) {
        action_x -= 48.0f;
        buttons.push_back({
            .command = ui_command::move_queue_item_down,
            .rect = queue_row_action_rect(row, action_x),
            .draw_icon = raythm_icons::draw_chevron_down,
            .bg = g_theme->row,
            .bg_hover = g_theme->row_hover,
            .icon = g_theme->text_muted,
            .enabled = can_move_down,
            .inset = 8.0f,
        });
        action_x -= 46.0f;
        buttons.push_back({
            .command = ui_command::move_queue_item_up,
            .rect = queue_row_action_rect(row, action_x),
            .draw_icon = raythm_icons::draw_chevron_up,
            .bg = g_theme->row,
            .bg_hover = g_theme->row_hover,
            .icon = g_theme->text_muted,
            .enabled = can_move_up,
            .inset = 8.0f,
        });
        action_x -= 8.0f;
    }
    if (can_download) {
        action_x -= 42.0f;
        buttons.push_back({
            .command = ui_command::none,
            .rect = queue_row_action_rect(row, action_x),
            .draw_icon = raythm_icons::draw_download,
            .bg = g_theme->accent,
            .bg_hover = g_theme->row_hover,
            .icon = g_theme->text,
            .enabled = true,
            .inset = 8.0f,
        });
    }
    return buttons;
}

chat_panel_layout chat_panel_layout_for() {
    const Rectangle message_area{kChatPanelRect.x + 16.0f, kChatPanelRect.y + 66.0f,
                                 kChatPanelRect.width - 32.0f, kChatPanelRect.height - 86.0f};
    const ui::rect_pair chat_composer =
        ui::split_trailing(kChatComposerRect, kChatSendButtonWidth, kChatComposerGap);
    return {
        .message_area = message_area,
        .composer_input = chat_composer.first,
        .send_button = chat_composer.second,
    };
}

create_room_modal_layout create_room_modal_layout_for() {
    constexpr std::array<float, 2> footer_button_widths = {180.0f, 410.0f};
    create_room_modal_layout layout{};
    layout.title = {kCreateModalRect.x + 26.0f, kCreateModalRect.y + 22.0f,
                    kCreateModalRect.width - 52.0f, 36.0f};
    layout.name_input = {kCreateModalRect.x + 40.0f, kCreateModalRect.y + 90.0f, 620.0f, 56.0f};
    layout.password_input = {kCreateModalRect.x + 40.0f, kCreateModalRect.y + 162.0f, 620.0f, 56.0f};
    layout.options = {kCreateModalRect.x + 40.0f, kCreateModalRect.y + 238.0f, 620.0f, 48.0f};
    const ui::rect_pair option_columns = ui::split_columns(layout.options, 280.0f, 20.0f);
    layout.max_players = option_columns.first;
    layout.host_only = option_columns.second;
    ui::hstack_widths({kCreateModalRect.x + 40.0f, kCreateModalRect.y + 342.0f, 620.0f, 54.0f},
                      footer_button_widths,
                      30.0f,
                      layout.footer_buttons);
    return layout;
}

password_modal_layout password_modal_layout_for() {
    constexpr std::array<float, 2> footer_button_widths = {170.0f, 320.0f};
    password_modal_layout layout{};
    layout.title = {kPasswordModalRect.x + 26.0f, kPasswordModalRect.y + 24.0f,
                    kPasswordModalRect.width - 52.0f, 36.0f};
    layout.password_input = {kPasswordModalRect.x + 40.0f, kPasswordModalRect.y + 96.0f, 520.0f, 56.0f};
    ui::hstack_widths({kPasswordModalRect.x + 40.0f, kPasswordModalRect.y + 206.0f, 520.0f, 54.0f},
                      footer_button_widths,
                      30.0f,
                      layout.footer_buttons);
    return layout;
}

invite_modal_layout invite_modal_layout_for() {
    const Rectangle list{kInviteModalRect.x + 30.0f, kInviteModalRect.y + 108.0f,
                         kInviteModalRect.width - 60.0f, kInviteModalRect.height - 146.0f};
    return {
        .title = {kInviteModalRect.x + 26.0f, kInviteModalRect.y + 22.0f,
                  kInviteModalRect.width - 220.0f, 36.0f},
        .subtitle = {kInviteModalRect.x + 26.0f, kInviteModalRect.y + 60.0f,
                     kInviteModalRect.width - 220.0f, 24.0f},
        .close_button = ui::split_trailing(
            {kInviteModalRect.x + 26.0f, kInviteModalRect.y + 26.0f,
             kInviteModalRect.width - 64.0f, 42.0f},
            104.0f).second,
        .list = list,
        .viewport = ui::inset(list, ui::edge_insets::uniform(14.0f)),
    };
}

invite_friend_row_layout invite_friend_row_layout_for(Rectangle row) {
    return {
        .row = row,
        .avatar = {row.x + 14.0f, row.y + 12.0f, 48.0f, 48.0f},
        .name = {row.x + 78.0f, row.y + 11.0f, row.width - 240.0f, 28.0f},
        .status = {row.x + 78.0f, row.y + 41.0f, row.width - 240.0f, 20.0f},
        .invite_button = ui::split_trailing(
            {row.x + 22.0f, row.y + 17.0f, row.width - 44.0f, 38.0f},
            104.0f).second,
    };
}

invite_friend_action_button invite_friend_action_button_for(const invite_friend_row_layout& layout,
                                                            const friend_client::social_user& user,
                                                            const state& state) {
    const bool selected = state.selected_invite_user_id == user.id;
    return {
        .user_id = user.id,
        .rect = layout.invite_button,
        .label = selected || state.pending == pending_operation::invite_friend ? "Sending..." : "Invite",
        .enabled = !busy(state) && !user.id.empty(),
    };
}

constexpr Rectangle invite_friend_row_rect(Rectangle viewport, int index) {
    return ui::vertical_list_row_rect(viewport, index, kInviteFriendRowHeight, kInviteFriendRowGap, 0.0f);
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

Color member_presence_color(const room_member& member) {
    if (member.status == "PLAYING") {
        return g_theme->accent;
    }
    return member.connected ? g_theme->success : g_theme->text_dim;
}

std::string member_avatar_label(const room_member& member) {
    const std::string& source = member.display_name.empty() ? member.user_id : member.display_name;
    std::string result;
    result.reserve(2);
    for (char ch : source) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (result.size() == 2) {
                break;
            }
        }
    }
    return result.empty() ? "P" : result;
}

std::string friend_avatar_label(const friend_client::social_user& user) {
    const std::string& source = user.display_name.empty() ? user.id : user.display_name;
    std::string result;
    result.reserve(2);
    for (char ch : source) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (result.size() == 2) {
                break;
            }
        }
    }
    return result.empty() ? "F" : result;
}

void draw_member_presence_icon(Rectangle rect, const room_member& member) {
    const Color color = member_presence_color(member);
    if (member.status == "PLAYING") {
        raythm_icons::draw_play(ui::inscribed_square(rect, 4.0f), color, 3.0f);
    } else if (member.connected) {
        raythm_icons::draw_note_tap(ui::inscribed_square(rect, 4.0f), color, 3.0f);
    } else {
        raythm_icons::draw_clock_3(ui::inscribed_square(rect, 2.0f), color, 3.0f);
    }
}

ui::button_state draw_refresh_icon_button(Rectangle rect) {
    return ui::icon_button(rect, raythm_icons::draw_refresh, {
        .border_width = 1.5f,
        .bg = g_theme->row_soft,
        .bg_hover = g_theme->row_soft_hover,
        .icon_color = g_theme->text_muted,
        .icon_hover_color = g_theme->text,
        .icon_inset = 13.0f,
        .icon_stroke_width = 3.0f,
    });
}

ui::button_state draw_icon_button(Rectangle rect,
                                  void (*draw_icon)(Rectangle, Color, float),
                                  Color bg,
                                  Color bg_hover,
                                  Color icon,
                                  bool enabled = true,
                                  float inset = 9.0f,
                                  Color icon_hover = {0, 0, 0, 0}) {
    return ui::icon_button(rect, draw_icon, {
        .border_width = 1.5f,
        .enabled = enabled,
        .bg = bg,
        .bg_hover = bg_hover,
        .icon_color = icon,
        .icon_hover_color = icon_hover.a > 0 ? icon_hover : g_theme->text,
        .disabled_bg = g_theme->panel,
        .disabled_bg_hover = g_theme->panel,
        .disabled_icon_color = g_theme->text_dim,
        .disabled_border_color = g_theme->border,
        .icon_inset = inset,
        .icon_stroke_width = 3.0f,
    });
}

ui::button_state draw_queue_row_action_button(const queue_row_action_button& button) {
    return draw_icon_button(button.rect,
                            button.draw_icon,
                            button.bg,
                            button.bg_hover,
                            button.icon,
                            button.enabled,
                            button.inset,
                            button.icon_hover);
}

ui::button_state draw_invite_friend_action_button(const invite_friend_action_button& button) {
    return ui::queued_button(button.rect,
                             localization::tr_literal(button.label), {
                                 .layer = ui::draw_layer::modal,
                                 .font_size = 14,
                             });
}

modal_command_interaction draw_modal_footer_buttons(const std::array<Rectangle, 2>& buttons,
                                                    const char* primary_label,
                                                    ui_command primary_command,
                                                    bool primary_enabled) {
    modal_command_interaction interaction;
    const std::array<ui::action_button_definition<ui_command>, 2> footer_buttons = {{
        {buttons[0], localization::tr(localization::text_key::cancel), ui_command::cancel_modal},
        {buttons[1], primary_label, primary_enabled ? primary_command : ui_command::none},
    }};
    const auto command = ui::queued_draw_action_buttons<ui_command>(footer_buttons, {
        .layer = ui::draw_layer::modal,
        .font_size = 18,
        .border_width = 2.0f,
    });
    if (command.has_value()) {
        interaction.command = *command;
    }
    return interaction;
}

queue_preview_interaction draw_queue_preview_controls(const state& state, const room_detail& room) {
    queue_preview_interaction interaction;
    const queue_preview_layout layout = queue_preview_layout_for();
    const bool has_queue = !room.queue.empty();
    const bool available = has_queue && state.queue_preview_available;
    const Color muted = available ? g_theme->text_muted : with_alpha(g_theme->text_muted, 120);
    const ui::button_state toggle =
        draw_icon_button(layout.toggle,
                         state.queue_preview_playing ? raythm_icons::draw_pause : raythm_icons::draw_play,
                         available ? g_theme->row : g_theme->panel,
                         available ? g_theme->row_hover : g_theme->panel,
                         muted,
                         available,
                         11.0f,
                         g_theme->text);
    if (toggle.clicked && available) {
        interaction.command = ui_command::toggle_queue_preview;
    }

    const double duration = state.queue_preview_duration_seconds;
    const double position = state.queue_preview_position_seconds;
    const float ratio = duration > 0.0
        ? std::clamp(static_cast<float>(position / duration), 0.0f, 1.0f)
        : 0.0f;
    ui::progress_bar(layout.bar, ratio, {
        .bg = with_alpha(g_theme->bg_alt, 220),
        .fill = available ? with_alpha(g_theme->accent, 220) : with_alpha(g_theme->text_muted, 80),
        .border_color = with_alpha(g_theme->border_light, 190),
        .border_width = 1.0f,
        .custom_colors = true,
    });
    if (available && duration > 0.0 && ui::is_clicked(layout.seek_hit)) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        const float seek_ratio = std::clamp((mouse.x - layout.bar.x) / layout.bar.width, 0.0f, 1.0f);
        interaction.seek_seconds = static_cast<double>(seek_ratio) * duration;
        interaction.seek_requested = true;
    }

    const std::string label = available
        ? format_duration_label(position) + " / " + format_duration_label(duration)
        : (has_queue ? localization::tr_literal("Not installed") : localization::tr_literal("No queued songs"));
    ui::draw_text_in_rect(label.c_str(), 13, layout.time_label, muted, ui::text_align::right);
    return interaction;
}

room_card_interaction draw_room_card(const room_summary& room, int index) {
    const room_card_layout layout = room_card_layout_for(index);
    const ui::row_state row_state = ui::row(layout.card, {
        .border_width = 2.0f,
        .bg = g_theme->row,
        .bg_hover = g_theme->row_hover,
        .border_color = room.locked ? g_theme->slow : g_theme->border,
        .custom_colors = true,
    });
    const std::string lock = room.locked ? "[LOCK] " : "";
    const std::string title = lock + room.name;
    const std::string meta = room_status_label(room);
    ui::draw_text_in_rect(title.c_str(), 25, layout.title, g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(meta.c_str(), 18, layout.meta, g_theme->text_muted, ui::text_align::left);
    ui::draw_text_in_rect(room.chart_title.empty() ? localization::tr_literal("No chart selected") : room.chart_title.c_str(),
                          18, layout.chart,
                          g_theme->text_muted, ui::text_align::left);
    if (row_state.clicked) {
        return {.selected = true, .room_id = room.id, .locked = room.locked};
    }
    return {};
}

room_list_interaction draw_room_list(const state& state) {
    room_list_interaction interaction;
    if (ui::button(kBackButtonRect, localization::tr(localization::text_key::back), {
            .font_size = 16,
            .border_width = 1.5f,
            .bg = g_theme->row_soft,
            .bg_hover = g_theme->row_soft_hover,
            .text_color = g_theme->text,
            .custom_colors = true,
        }).clicked) {
        interaction.command = ui_command::back_to_home;
    }
    if (ui::action_button(kCreateButtonRect, localization::tr_literal("Create Room"), {
            .font_size = 18,
            .enabled = state.auth.logged_in,
            .bg = g_theme->accent,
            .bg_hover = g_theme->row_active,
            .disabled_bg = g_theme->row,
            .disabled_text_color = g_theme->text_muted,
            .disabled_border_color = g_theme->border,
        }).clicked) {
        interaction.command = ui_command::open_create_room;
    }

    ui::panel(kListRect);
    ui::draw_text_in_rect(localization::tr_literal("Rooms"), 28, kListTitleRect, g_theme->text, ui::text_align::left);
    if (draw_refresh_icon_button(kRefreshButtonRect).clicked) {
        interaction.command = ui_command::refresh_rooms;
    }
    if (!state.auth.logged_in) {
        ui::draw_text_in_rect(localization::tr_literal("Sign in from the account menu before joining multiplayer."),
                              24, kListViewRect, g_theme->text_muted);
        return interaction;
    }

    int index = 0;
    for (const room_summary& room : state.rooms) {
        if (index >= 8) {
            break;
        }
        const room_card_interaction card = draw_room_card(room, index++);
        if (!card.selected) {
            continue;
        }
        interaction.selected_room_id = card.room_id;
        if (card.locked) {
            interaction.password_required = true;
        } else {
            interaction.command = ui_command::submit_password;
        }
    }
    if (state.rooms.empty() && !state.loading_rooms) {
        ui::draw_text_in_rect(localization::tr_literal("No rooms yet."), 24, kListViewRect, g_theme->text_muted);
    }
    return interaction;
}

member_panel_interaction draw_members(const state& state, const room_detail& room) {
    member_panel_interaction interaction;
    ui::panel(kMemberPanelRect);
    draw_panel_title(kMemberPanelRect, localization::tr_literal("Players"));
    const bool invite_available = can_invite_friends(room);
    const ui::button_state invite_button =
        ui::action_button(kInviteFriendsButtonRect, localization::tr_literal("Invite"), {
            .font_size = 16,
            .enabled = invite_available,
            .disabled_bg = g_theme->row_soft,
            .disabled_bg_hover = g_theme->row_soft,
            .disabled_text_color = g_theme->text_muted,
            .disabled_border_color = g_theme->border,
        });
    if (invite_button.clicked && invite_available && !busy(state)) {
        interaction.invite_requested = true;
    }
    const Rectangle rows_viewport = member_rows_viewport();
    std::vector<const room_member*> visible_members;
    visible_members.reserve(room.members.size());
    for (const room_member& member : room.members) {
        if (visible_member_status(member.status)) {
            visible_members.push_back(&member);
        }
    }
    const ui::index_range visible_rows = ui::vertical_list_fitting_range(
        visible_members.size(), rows_viewport, kMemberRowHeight, kMemberRowGap, 0.0f);
    for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
        const room_member& member = *visible_members[static_cast<size_t>(i)];
        const Rectangle row = member_row_rect(i);
        const bool self = (!state.self_user_id.empty() && member.user_id == state.self_user_id);
        const ui::row_state member_row = ui::row(row, {
            .border_width = 1.5f,
            .bg = self ? g_theme->row_selected : g_theme->row,
            .bg_hover = g_theme->row_hover,
            .border_color = member.role == "HOST" ? g_theme->slow : g_theme->border,
            .custom_colors = true,
        });
        if (member_row.clicked && !member.user_id.empty()) {
            interaction.profile_user_id = member.user_id;
        }
        const bool host = member.role == "HOST";
        const member_row_layout layout = member_row_layout_for(row, host);
        const std::string name = member.display_name;
        avatar_texture_cache::draw_avatar(layout.avatar,
                                          member.avatar_url,
                                          member_avatar_label(member),
                                          g_theme->row_soft,
                                          g_theme->text,
                                          14,
                                          state.auth.server_url);
        ui::draw_text_in_rect(name.c_str(), 19, layout.name, g_theme->text, ui::text_align::left);
        if (host) {
            raythm_icons::draw_crown(ui::inscribed_square(layout.host_icon, 2.0f), g_theme->slow, 3.0f);
        }
        draw_member_presence_icon(layout.presence_icon, member);
        if (member.ready) {
            raythm_icons::draw_circle_check(ui::inscribed_square(layout.ready_icon, 2.0f), g_theme->success, 3.0f);
        } else {
            raythm_icons::draw_clock_3(ui::inscribed_square(layout.ready_icon, 2.0f), g_theme->text_muted, 3.0f);
        }
    }
    return interaction;
}

queue_panel_interaction draw_queue(const state& state, const room_detail& room) {
    queue_panel_interaction interaction;
    ui::panel(kQueuePanelRect);
    draw_panel_title(kQueuePanelRect, localization::tr_literal("Beatmap queue"));
    const bool self_host = is_self_host(state, room);
    if (self_host) {
        const bool host_only = room.queue_permission == "HOST_ONLY";
        const std::string permission_label = localization::tr_literal(
            host_only ? "Queue: host only" : "Queue: all players");
        if (ui::button(kQueuePermissionButtonRect, permission_label.c_str(), {
                .font_size = 15,
                .bg = g_theme->row,
                .bg_hover = g_theme->row_hover,
                .text_color = g_theme->text,
                .custom_colors = true,
            }).clicked &&
            !busy(state)) {
            interaction.command = ui_command::toggle_queue_permission;
        }
    }

    const Rectangle add_rect{kQueuePanelRect.x + 16.0f, kQueuePanelRect.y + 72.0f, kQueuePanelRect.width - 32.0f, 50.0f};
    if (ui::button(add_rect, localization::tr_literal("Add song"), {
            .font_size = 17,
            .bg = g_theme->accent,
            .bg_hover = g_theme->row_hover,
            .text_color = g_theme->text,
            .custom_colors = true,
        }).clicked &&
        !busy(state)) {
        interaction.command = ui_command::open_song_select;
    }
    const queue_preview_interaction preview = draw_queue_preview_controls(state, room);
    if (preview.command != ui_command::none) {
        interaction.command = preview.command;
    }
    if (preview.seek_requested) {
        interaction.seek_requested = true;
        interaction.seek_seconds = preview.seek_seconds;
    }

    if (room.queue.empty()) {
        ui::draw_text_in_rect(localization::tr_literal("No queued songs yet."), 21,
                              {kQueueListRect.x, kQueueListRect.y + 80.0f, kQueueListRect.width, kQueueListRect.height - 80.0f},
                              g_theme->text_muted);
        return interaction;
    }

    const float max_scroll = ui::max_scroll_offset(queue_content_height(room), kQueueListRect);
    float queue_scroll_y = state.queue_scroll_y;
    float queue_scroll_y_target = state.queue_scroll_y_target;
    if (ui::is_hovered(kQueueListRect)) {
        queue_scroll_y_target =
            ui::wheel_scrolled_target(queue_scroll_y_target, ui::mouse_wheel_move(), kQueueWheelStep);
    }
    queue_scroll_y_target = ui::clamp_scroll_offset(queue_scroll_y_target, max_scroll);
    queue_scroll_y = ui::clamp_scroll_offset(queue_scroll_y, max_scroll);
    if (std::abs(queue_scroll_y - queue_scroll_y_target) < 0.5f) {
        queue_scroll_y = queue_scroll_y_target;
    } else {
        queue_scroll_y += (queue_scroll_y_target - queue_scroll_y) * 0.32f;
    }
    interaction.scroll_updated = true;
    interaction.queue_scroll_y = queue_scroll_y;
    interaction.queue_scroll_y_target = queue_scroll_y_target;

    {
        ui::scoped_clip_rect clip(kQueueListRect);
        const ui::index_range visible_rows = ui::vertical_list_visible_range(
            room.queue.size(), kQueueListRect, kQueueRowHeight, kQueueRowGap, queue_scroll_y);
        for (int queue_index = visible_rows.begin; queue_index < visible_rows.end; ++queue_index) {
            const room_queue_item& item = room.queue[static_cast<std::size_t>(queue_index)];
            const Rectangle row = queue_row_rect(queue_index, queue_scroll_y);
            const bool has_level = item.level > 0.0f;
            const Color level_color = has_level ? difficulty_level_color(item.level) : g_theme->accent;
            const bool hovered = ui::is_hovered(row);
            const Color base_fill = lerp_color(g_theme->bg_alt, level_color, has_level ? 0.08f : 0.0f);
            const Color hover_fill = lerp_color(g_theme->row_hover, level_color, has_level ? 0.10f : 0.0f);
            const bool can_remove = self_host || (!state.self_user_id.empty() && item.requested_by_user_id == state.self_user_id);
            const bool installed = queue_item_installed(state, item.id);
            const bool can_download = !installed && !item.song_id.empty() && !item.chart_id.empty();
            const float action_width = (self_host ? 100.0f : 0.0f) +
                                       (can_remove ? 50.0f : 0.0f) +
                                       (can_download ? 50.0f : 0.0f);
            const queue_row_layout row_layout = queue_row_layout_for(row, action_width);
            ui::surface_fill(row_layout.row, hovered ? hover_fill : base_fill);
            ui::accent_bar(row_layout.accent, level_color);
            ui::divider(row_layout.top_divider, g_theme->border_light);
            ui::divider(row_layout.bottom_divider, with_alpha(g_theme->border_light, 170));
            const std::string title = item.song_title.empty() ? item.chart_id : item.song_title;
            ui::draw_text_in_rect(title.c_str(), 19, row_layout.title, g_theme->text, ui::text_align::left);
            const std::string meta = item.difficulty_name + (item.requested_by.empty() ? "" : "  by " + item.requested_by);
            ui::draw_text_in_rect(meta.c_str(), 15, row_layout.meta, g_theme->text_muted, ui::text_align::left);
            if (has_level) {
                draw_difficulty_level_badge(item.level, row_layout.level_badge, 13, 255);
            } else {
                ui::draw_text_in_rect("Lv.-", 13, row_layout.level_badge, g_theme->text_muted, ui::text_align::center);
            }
            ui::draw_text_in_rect(localization::tr_literal(installed ? "Installed" : "Not installed"), 13,
                                  row_layout.install_status,
                                  installed ? g_theme->success : g_theme->text_muted, ui::text_align::left);
            const bool can_move_up = queue_index > 0;
            const bool can_move_down = queue_index + 1 < static_cast<int>(room.queue.size());
            const std::vector<queue_row_action_button> action_buttons =
                queue_row_action_buttons_for(row,
                                             row.x + row.width - 16.0f,
                                             can_remove,
                                             self_host,
                                             can_move_up,
                                             can_move_down,
                                             can_download);
            for (const queue_row_action_button& button : action_buttons) {
                if (!draw_queue_row_action_button(button).clicked || !button.enabled || busy(state)) {
                    continue;
                }
                if (button.draw_icon == raythm_icons::draw_download) {
                    interaction.download_song_id = item.song_id;
                    interaction.download_chart_id = item.chart_id;
                    interaction.download_requested = true;
                    continue;
                }
                if (button.command != ui_command::none) {
                    interaction.selected_queue_item_id = item.id;
                    interaction.command = button.command;
                }
            }
        }
    }
    ui::scrollbar(kQueueScrollbarTrackRect,
                  queue_content_height(room),
                  queue_scroll_y, {
                      .track_color = g_theme->scrollbar_track,
                      .thumb_color = g_theme->scrollbar_thumb,
                      .custom_colors = true,
                  });
    return interaction;
}

chat_panel_interaction draw_chat(state& state, const room_detail& room) {
    chat_panel_interaction interaction;
    ui::panel(kChatPanelRect);
    draw_panel_title(kChatPanelRect, localization::tr_literal("Chat"));
    const chat_panel_layout layout = chat_panel_layout_for();
    struct chat_row {
        std::vector<std::string> lines;
        ui::vertical_stack_item metrics = {};
    };
    std::vector<chat_row> rows;
    const Rectangle message_area = layout.message_area;
    const float text_width = message_area.width - kChatMessagePaddingX * 2.0f;
    float total_height = 0.0f;
    for (int i = static_cast<int>(room.chat.size()) - 1; i >= 0; --i) {
        const chat_message& message = room.chat[static_cast<size_t>(i)];
        chat_row row;
        row.lines = wrap_chat_text(message.display_name + ": " + message.message, text_width, 16);
        row.metrics.height = std::max(42.0f,
                                      kChatMessagePaddingY * 2.0f +
                                      static_cast<float>(row.lines.size()) * kChatLineHeight);
        const float next_total = total_height + (rows.empty() ? 0.0f : kChatMessageGap) + row.metrics.height;
        if (next_total > message_area.height && !rows.empty()) {
            break;
        }
        total_height = next_total;
        rows.push_back(std::move(row));
    }
    std::reverse(rows.begin(), rows.end());
    float row_y = 0.0f;
    for (chat_row& row : rows) {
        row.metrics.y = row_y;
        row_y += row.metrics.height + kChatMessageGap;
    }
    const float stack_height = ui::vertical_stack_content_height(
        static_cast<int>(rows.size()),
        [&](int index) {
            return rows[static_cast<std::size_t>(index)].metrics;
        });
    const Rectangle row_viewport{
        message_area.x,
        message_area.y + std::max(0.0f, message_area.height - stack_height),
        message_area.width,
        message_area.height,
    };
    {
        ui::scoped_clip_rect clip(message_area);
        const ui::index_range visible_rows = ui::vertical_stack_visible_range(
            static_cast<int>(rows.size()),
            [&](int index) {
                return rows[static_cast<std::size_t>(index)].metrics;
            },
            row_viewport,
            0.0f);
        for (int row_index = visible_rows.begin; row_index < visible_rows.end; ++row_index) {
            const chat_row& row = rows[static_cast<std::size_t>(row_index)];
            const Rectangle box = ui::vertical_stack_item_rect(row_viewport, row.metrics, 0.0f);
            ui::surface(box, with_alpha(g_theme->row, 190), with_alpha(g_theme->border_light, 190), 1.0f);
            float text_y = box.y + kChatMessagePaddingY;
            for (const std::string& line : row.lines) {
                ui::draw_text_f(line.c_str(), box.x + kChatMessagePaddingX, text_y, 16, g_theme->text_muted);
                text_y += kChatLineHeight;
            }
        }
    }
    const ui::text_input_result chat_result =
        ui::text_input(layout.composer_input, state.chat_input, "", localization::tr_literal("Message..."), {
            .font_size = 16,
            .max_length = 160,
            .filter = ui::default_text_input_filter,
            .label_width = 0.0f,
            .single_rect = true,
            .submit_deactivates = false,
            .single_rect_align = ui::text_align::left,
        });
    if (chat_result.submitted) {
        interaction.send_requested = true;
    }
    if (ui::button(layout.send_button, localization::tr_literal("Send"), {.font_size = 18}).clicked) {
        interaction.send_requested = true;
    }
    return interaction;
}

room_footer_interaction draw_room_footer(const state& state, const room_detail& room) {
    room_footer_interaction interaction;
    if (ui::button(kLeaveButtonRect, localization::tr_literal("Leave"), {.font_size = 20}).clicked) {
        interaction.command = ui_command::leave_room;
    }
    const bool queue_ready = !room.queue.empty() && state.current_queue_chart_installed;
    const bool can_start = is_self_host(state, room) && queue_ready && all_joined_members_ready(room);
    const std::string action_label = can_start ? localization::tr_literal("Start") : localization::tr_literal(state.local_ready ? "Cancel Ready"
        : (room.queue.empty() ? "Ready (queue empty)"
        : (state.current_queue_chart_installed ? "Ready" : "Ready (download needed)")));
    const Color action_bg = can_start ? g_theme->accent :
        (queue_ready ? (state.local_ready ? g_theme->row_selected : g_theme->success) : g_theme->row);
    const bool action_enabled = can_start || queue_ready;
    if (ui::action_button(kReadyButtonRect, action_label.c_str(), {
            .font_size = 22,
            .enabled = action_enabled,
            .bg = action_bg,
            .bg_hover = g_theme->row_hover,
            .disabled_bg = action_bg,
            .disabled_text_color = g_theme->text_muted,
            .disabled_border_color = g_theme->border,
        }).clicked) {
        interaction.command = can_start ? ui_command::start_match : ui_command::toggle_ready;
    }
    return interaction;
}

draw_result draw_room(state& state) {
    draw_result result;
    if (!state.current_room.has_value()) {
        return result;
    }
    const room_detail& room = *state.current_room;
    notify_status_change(state, room);

    const member_panel_interaction members = draw_members(state, room);
    if (members.invite_requested) {
        result.command = ui_command::open_invite_friends;
    }
    if (!members.profile_user_id.empty()) {
        result.selected_profile_user_id = members.profile_user_id;
        result.command = ui_command::open_profile;
    }

    const queue_panel_interaction queue = draw_queue(state, room);
    if (queue.scroll_updated) {
        result.queue_scroll_changed = true;
        result.queue_scroll_y = queue.queue_scroll_y;
        result.queue_scroll_y_target = queue.queue_scroll_y_target;
    }
    if (queue.seek_requested) {
        result.queue_preview_seek_seconds = queue.seek_seconds;
        result.queue_preview_seek_requested = true;
    }
    if (!queue.selected_queue_item_id.empty()) {
        result.selected_queue_item_id = queue.selected_queue_item_id;
    }
    if (queue.download_requested) {
        result.download_song_id = queue.download_song_id;
        result.download_chart_id = queue.download_chart_id;
        result.queue_download_requested = true;
    }
    if (queue.command != ui_command::none) {
        result.command = queue.command;
    }

    const chat_panel_interaction chat = draw_chat(state, room);
    if (chat.send_requested) {
        result.command = ui_command::send_chat;
    }

    const room_footer_interaction footer = draw_room_footer(state, room);
    if (footer.command != ui_command::none) {
        result.command = footer.command;
    }
    return result;
}

create_room_modal_interaction draw_create_modal(state& state) {
    create_room_modal_interaction interaction;
    const create_room_modal_layout layout = create_room_modal_layout_for();
    ui::register_hit_region(kCreateModalRect, ui::draw_layer::modal);
    ui::draw_fullscreen_overlay(with_alpha(BLACK, 130));
    ui::panel(kCreateModalRect);
    ui::draw_text_in_rect(localization::tr_literal("Create Room"), 28,
                          layout.title,
                          g_theme->text, ui::text_align::left);
    const ui::text_input_result name_result =
        ui::text_input(layout.name_input,
                       state.create_name_input, localization::tr_literal("Name"), localization::tr_literal("Room name"), {
                           .layer = ui::draw_layer::modal,
                           .font_size = 18,
                           .max_length = 80,
                           .filter = ui::default_text_input_filter,
                           .label_width = 110.0f,
                       });
    const ui::text_input_result password_result =
        ui::text_input(layout.password_input,
                       state.create_password_input, localization::tr_literal("Password"), localization::tr_literal("Optional"), {
                           .layer = ui::draw_layer::modal,
                           .font_size = 18,
                           .max_length = 128,
                           .filter = ui::default_text_input_filter,
                           .label_width = 110.0f,
                           .obscure_value = true,
                       });
    if ((name_result.submitted || password_result.submitted) && !busy(state)) {
        interaction.command = ui_command::submit_create_room;
    }

    const std::string max_label =
        std::string(localization::tr_literal("Players:")) + " " + std::to_string(state.create_max_players);
    const ui::selector_state max_selector = ui::value_selector(layout.max_players, localization::tr_literal("Max"), max_label.c_str(), {
        .layer = ui::draw_layer::modal,
        .font_size = 17,
        .button_size = 32.0f,
        .label_width = 82.0f,
    });
    int next_max_players = state.create_max_players;
    if (max_selector.left.clicked) {
        next_max_players = std::max(2, next_max_players - 1);
    }
    if (max_selector.right.clicked) {
        next_max_players = std::min(8, next_max_players + 1);
    }
    if (next_max_players != state.create_max_players) {
        interaction.max_players_changed = true;
        interaction.max_players = next_max_players;
    }
    const ui::button_state host_button =
        ui::queued_button(layout.host_only,
                          localization::tr_literal(state.create_host_only ? "Queue: host only" : "Queue: all players"), {
                              .layer = ui::draw_layer::modal,
                              .font_size = 17,
                          });
    if (host_button.clicked) {
        interaction.toggle_host_only = true;
    }

    const modal_command_interaction footer =
        draw_modal_footer_buttons(layout.footer_buttons,
                                  busy(state)
                                      ? localization::tr(localization::text_key::creating)
                                      : localization::tr(localization::text_key::create),
                                  ui_command::submit_create_room,
                                  !busy(state));
    if (footer.command != ui_command::none) {
        interaction.command = footer.command;
    }
    return interaction;
}

modal_command_interaction draw_password_modal(state& state) {
    modal_command_interaction interaction;
    const password_modal_layout layout = password_modal_layout_for();
    ui::register_hit_region(kPasswordModalRect, ui::draw_layer::modal);
    ui::draw_fullscreen_overlay(with_alpha(BLACK, 130));
    ui::panel(kPasswordModalRect);
    ui::draw_text_in_rect(localization::tr_literal("Room password"), 28,
                          layout.title,
                          g_theme->text, ui::text_align::left);
    const ui::text_input_result password_result =
        ui::text_input(layout.password_input,
                       state.join_password_input, localization::tr_literal("Password"), "", {
                           .layer = ui::draw_layer::modal,
                           .font_size = 18,
                           .max_length = 128,
                           .filter = ui::default_text_input_filter,
                           .label_width = 120.0f,
                           .obscure_value = true,
                       });
    if (password_result.submitted && !busy(state)) {
        interaction.command = ui_command::submit_password;
    }
    const modal_command_interaction footer =
        draw_modal_footer_buttons(layout.footer_buttons,
                                  busy(state) ? localization::tr_literal("Joining...") : localization::tr_literal("Join"),
                                  ui_command::submit_password,
                                  !busy(state));
    if (footer.command != ui_command::none) {
        interaction.command = footer.command;
    }
    return interaction;
}

invite_friends_modal_interaction draw_invite_friends_modal(const state& state) {
    invite_friends_modal_interaction interaction;
    const invite_modal_layout layout = invite_modal_layout_for();
    ui::register_hit_region(kInviteModalRect, ui::draw_layer::modal);
    ui::draw_fullscreen_overlay(with_alpha(BLACK, 130));
    ui::panel(kInviteModalRect);
    ui::draw_text_in_rect(localization::tr_literal("Invite friends"), 28,
                          layout.title,
                          g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(state.loading_invite_friends ? localization::tr_literal("Loading friends...") : localization::tr_literal("Room invite"),
                          16,
                          layout.subtitle,
                          g_theme->text_muted, ui::text_align::left);

    const ui::button_state close =
        ui::queued_button(layout.close_button,
                          localization::tr(localization::text_key::cancel), {
                              .layer = ui::draw_layer::modal,
                              .font_size = 16,
                          });
    if (close.clicked) {
        interaction.command = ui_command::cancel_modal;
    }

    ui::section(layout.list);
    const Rectangle viewport = layout.viewport;
    if (state.loading_invite_friends && !state.invite_friends_loaded_once) {
        ui::draw_text_in_rect(localization::tr_literal("Loading friends..."), 18, viewport,
                              g_theme->text_muted, ui::text_align::center);
        return interaction;
    }
    if (state.invite_friends.friends.empty()) {
        ui::draw_text_in_rect(localization::tr_literal("No friends yet."), 18, viewport,
                              g_theme->text_muted, ui::text_align::center);
        return interaction;
    }

    ui::scoped_clip_rect clip(viewport);
    const ui::index_range visible_rows = ui::vertical_list_fitting_range(
        state.invite_friends.friends.size(), viewport, kInviteFriendRowHeight, kInviteFriendRowGap, 0.0f);
    for (int i = visible_rows.begin; i < visible_rows.end; ++i) {
        const friend_client::social_user& user = state.invite_friends.friends[static_cast<size_t>(i)];
        const Rectangle row = invite_friend_row_rect(viewport, i);
        ui::row(row, {
            .layer = ui::draw_layer::modal,
            .border_width = 1.5f,
            .bg = g_theme->row,
            .bg_hover = g_theme->row_hover,
            .border_color = g_theme->border_light,
            .custom_colors = true,
        });
        const invite_friend_row_layout row_layout = invite_friend_row_layout_for(row);
        avatar_texture_cache::draw_avatar(row_layout.avatar,
                                          user.avatar_url,
                                          friend_avatar_label(user),
                                          g_theme->row_soft_selected,
                                          g_theme->text,
                                          16,
                                          state.auth.server_url);
        ui::draw_text_in_rect(user.display_name.empty() ? "Unknown Player" : user.display_name.c_str(),
                              20,
                              row_layout.name,
                              g_theme->text, ui::text_align::left);
        const std::string status = user.current_room_name.empty()
            ? (user.online_status.empty() ? "offline" : user.online_status)
            : "in " + user.current_room_name;
        ui::draw_text_in_rect(status.c_str(),
                              14,
                              row_layout.status,
                              user.online_status == "offline" ? g_theme->text_muted : g_theme->success,
                              ui::text_align::left);
        const invite_friend_action_button invite_button =
            invite_friend_action_button_for(row_layout, user, state);
        if (draw_invite_friend_action_button(invite_button).clicked && invite_button.enabled) {
            interaction.selected_user_id = invite_button.user_id;
            interaction.command = ui_command::send_room_invite;
        }
    }
    return interaction;
}

}  // namespace

draw_result draw(state& state) {
    draw_result result;
    if (state.screen == screen_mode::room) {
        result = draw_room(state);
    } else {
        const room_list_interaction list = draw_room_list(state);
        if (!list.selected_room_id.empty()) {
            result.room_selected = true;
            result.selected_room_id = list.selected_room_id;
            result.selected_room_requires_password = list.password_required;
        }
        if (list.command != ui_command::none) {
            result.command = list.command;
        }
    }

    if (state.modal == modal_mode::create_room) {
        const create_room_modal_interaction create = draw_create_modal(state);
        if (create.max_players_changed) {
            result.create_max_players_changed = true;
            result.create_max_players = create.max_players;
        }
        if (create.toggle_host_only) {
            result.toggle_create_host_only = true;
        }
        if (create.command != ui_command::none) {
            result.command = create.command;
        }
    } else if (state.modal == modal_mode::password) {
        const modal_command_interaction password = draw_password_modal(state);
        if (password.command != ui_command::none) {
            result.command = password.command;
        }
    } else if (state.modal == modal_mode::invite_friends) {
        const invite_friends_modal_interaction invite = draw_invite_friends_modal(state);
        if (!invite.selected_user_id.empty()) {
            result.selected_invite_user_id = invite.selected_user_id;
        }
        if (invite.command != ui_command::none) {
            result.command = invite.command;
        }
    }
    return result;
}

}  // namespace multiplayer::view
