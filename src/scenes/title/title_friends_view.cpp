#include "title/title_friends_view.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <span>
#include <utility>

#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_modal.h"
#include "ui_scroll.h"
#include "virtual_screen.h"

namespace title_friends_view {
namespace {

constexpr Rectangle kModalRect{470.0f, 170.0f, 980.0f, 720.0f};
constexpr float kRowHeight = 72.0f;
constexpr float kRowGap = 10.0f;
constexpr float kModalPadding = 28.0f;
constexpr float kHeaderButtonHeight = 42.0f;
constexpr float kHeaderButtonGap = 14.0f;
constexpr float kCloseButtonWidth = 92.0f;
constexpr float kRefreshButtonWidth = 96.0f;
constexpr float kHeaderActionReservedWidth = 240.0f;
constexpr float kTabTopOffset = 76.0f;
constexpr float kTabHeight = 44.0f;
constexpr float kTabGap = 14.0f;
constexpr float kFriendsTabWidth = 150.0f;
constexpr float kRequestsTabWidth = 164.0f;
constexpr float kInvitesTabWidth = 150.0f;
constexpr float kListTopGap = 62.0f;
constexpr float kListBottomInset = 138.0f;
constexpr float kListViewportPadding = 14.0f;
constexpr float kUserAvatarSize = 48.0f;
constexpr float kUserAvatarInsetX = 14.0f;
constexpr float kUserAvatarInsetY = 12.0f;
constexpr float kUserTextInsetX = 78.0f;
constexpr float kUserActionReservedWidth = 420.0f;
constexpr float kUserNameTop = 11.0f;
constexpr float kUserNameHeight = 28.0f;
constexpr float kUserDetailTop = 41.0f;
constexpr float kUserDetailHeight = 20.0f;
constexpr float kRequestHeaderHeight = 22.0f;
constexpr float kRequestHeaderAdvance = 30.0f;
constexpr float kRequestSectionGap = 8.0f;

struct friends_layout {
    Rectangle modal{};
    Rectangle close_button{};
    Rectangle refresh_button{};
    Rectangle header_title{};
    Rectangle header_subtitle{};
    Rectangle friends_tab{};
    Rectangle requests_tab{};
    Rectangle invites_tab{};
    Rectangle list{};
    Rectangle list_viewport{};
};

struct tab_definition {
    tab value = tab::friends;
    const char* label = "";
    float width = 0.0f;
};

struct tab_button_layout {
    tab_definition definition{};
    Rectangle rect{};
};

struct header_action_button {
    command_type command = command_type::none;
    const char* label = "";
    Rectangle rect{};
};

enum class row_action_tone {
    normal,
    accent_text,
    primary,
    danger,
};

struct row_action_button {
    ui::row_action_slot slot = ui::row_action_slot::right;
    command_type command = command_type::none;
    const char* label = "";
    int font_size = 12;
    row_action_tone tone = row_action_tone::normal;
};

struct user_row_layout {
    Rectangle avatar{};
    Rectangle name{};
    Rectangle detail{};
};

struct request_section_layout {
    Rectangle header{};
    Rectangle rows_viewport{};
    ui::index_range rows{};
    float after_rows_y = 0.0f;
};

struct request_sections_layout {
    bool has_incoming = false;
    request_section_layout incoming{};
    bool has_outgoing = false;
    request_section_layout outgoing{};
};

constexpr std::array<tab_definition, 3> kTabs{{
    {tab::friends, "Friends", kFriendsTabWidth},
    {tab::requests, "Requests", kRequestsTabWidth},
    {tab::invites, "Invites", kInvitesTabWidth},
}};

constexpr Rectangle list_viewport_for(Rectangle list) {
    return ui::inset(list, ui::edge_insets::uniform(kListViewportPadding));
}

constexpr user_row_layout user_row_layout_for(Rectangle row) {
    return {
        {row.x + kUserAvatarInsetX, row.y + kUserAvatarInsetY, kUserAvatarSize, kUserAvatarSize},
        {row.x + kUserTextInsetX, row.y + kUserNameTop, row.width - kUserActionReservedWidth, kUserNameHeight},
        {row.x + kUserTextInsetX, row.y + kUserDetailTop, row.width - kUserActionReservedWidth, kUserDetailHeight},
    };
}

constexpr Rectangle list_row_rect_for(Rectangle viewport, int index) {
    return ui::vertical_list_row_rect(viewport, index, kRowHeight, kRowGap, 0.0f);
}

constexpr Rectangle request_section_header_rect_for(Rectangle viewport, float y) {
    return ui::vertical_span_rect(viewport, y, kRequestHeaderHeight);
}

request_section_layout request_section_layout_for(Rectangle viewport, float header_y, std::size_t row_count) {
    const float rows_y = header_y + kRequestHeaderAdvance;
    const Rectangle rows_viewport{viewport.x, rows_y, viewport.width, viewport.height};
    const ui::index_range rows =
        ui::vertical_list_fitting_range(row_count, rows_viewport, viewport, kRowHeight, kRowGap, 0.0f);
    return {
        .header = request_section_header_rect_for(viewport, header_y),
        .rows_viewport = rows_viewport,
        .rows = rows,
        .after_rows_y = rows_y + static_cast<float>(rows.end) * (kRowHeight + kRowGap),
    };
}

request_sections_layout request_sections_layout_for(Rectangle viewport,
                                                    std::size_t incoming_count,
                                                    std::size_t outgoing_count) {
    request_sections_layout layout{};
    float next_header_y = viewport.y;
    if (incoming_count > 0) {
        layout.has_incoming = true;
        layout.incoming = request_section_layout_for(viewport, next_header_y, incoming_count);
        next_header_y = layout.incoming.after_rows_y + kRequestSectionGap;
    }
    const float outgoing_fit_y = layout.has_incoming ? next_header_y - kRequestSectionGap : next_header_y;
    if (outgoing_count > 0 &&
        ui::vertical_span_fits_in_viewport(outgoing_fit_y, kRequestHeaderAdvance, viewport)) {
        layout.has_outgoing = true;
        layout.outgoing = request_section_layout_for(viewport, next_header_y, outgoing_count);
    }
    return layout;
}

std::array<float, kTabs.size()> tab_widths() {
    std::array<float, kTabs.size()> widths{};
    for (std::size_t index = 0; index < kTabs.size(); ++index) {
        widths[index] = kTabs[index].width;
    }
    return widths;
}

std::array<tab_button_layout, kTabs.size()> tab_buttons_for(const friends_layout& layout) {
    return {{
        {kTabs[0], layout.friends_tab},
        {kTabs[1], layout.requests_tab},
        {kTabs[2], layout.invites_tab},
    }};
}

std::array<header_action_button, 2> header_action_buttons_for(const friends_layout& layout) {
    return {{
        {command_type::refresh, "REFRESH", layout.refresh_button},
        {command_type::close, "CLOSE", layout.close_button},
    }};
}

constexpr std::array<row_action_button, 3> friend_row_actions() {
    return {{
        {ui::row_action_slot::left, command_type::open_profile, "PROFILE", 12, row_action_tone::normal},
        {ui::row_action_slot::middle, command_type::remove_friend, "REMOVE", 12, row_action_tone::normal},
        {ui::row_action_slot::right, command_type::block_user, "BLOCK", 12, row_action_tone::danger},
    }};
}

constexpr std::array<row_action_button, 3> incoming_request_row_actions() {
    return {{
        {ui::row_action_slot::left, command_type::open_profile, "PROFILE", 12, row_action_tone::normal},
        {ui::row_action_slot::middle, command_type::accept_request, "ACCEPT", 13, row_action_tone::primary},
        {ui::row_action_slot::right, command_type::decline_request, "DECLINE", 13, row_action_tone::normal},
    }};
}

constexpr std::array<row_action_button, 2> outgoing_request_row_actions() {
    return {{
        {ui::row_action_slot::middle, command_type::none, "PENDING", 12, row_action_tone::normal},
        {ui::row_action_slot::right, command_type::open_profile, "PROFILE", 12, row_action_tone::normal},
    }};
}

std::array<row_action_button, 3> invite_row_actions(bool read) {
    return {{
        {ui::row_action_slot::left, command_type::accept_invite, "JOIN", 13, row_action_tone::primary},
        {ui::row_action_slot::middle,
         read ? command_type::none : command_type::mark_invite_read,
         read ? "SEEN" : "READ",
         12,
         read ? row_action_tone::normal : row_action_tone::accent_text},
        {ui::row_action_slot::right, command_type::decline_invite, "DECLINE", 13, row_action_tone::normal},
    }};
}

friends_layout make_layout(float open_anim) {
    const Rectangle modal = ui::animated_modal_rect(kModalRect, open_anim);
    const Rectangle content = ui::inset(modal, ui::edge_insets::uniform(kModalPadding));
    const Rectangle header_actions{content.x, content.y, content.width, kHeaderButtonHeight};
    const ui::rect_pair close_split = ui::split_trailing(header_actions, kCloseButtonWidth, kHeaderButtonGap);
    const ui::rect_pair refresh_split = ui::split_trailing(close_split.first, kRefreshButtonWidth, kHeaderButtonGap);
    const std::array<float, kTabs.size()> tab_width_values = tab_widths();
    Rectangle tab_rects[3]{};
    ui::hstack_widths({content.x, content.y + kTabTopOffset, content.width, kTabHeight},
                      tab_width_values,
                      kTabGap,
                      tab_rects);
    const Rectangle list{content.x,
                         tab_rects[0].y + kListTopGap,
                         content.width,
                         content.height - kListBottomInset};
    return {
        .modal = modal,
        .close_button = close_split.second,
        .refresh_button = refresh_split.second,
        .header_title = {content.x, content.y, content.width - kHeaderActionReservedWidth, 44.0f},
        .header_subtitle = {content.x, content.y + 42.0f, content.width - kHeaderActionReservedWidth, 24.0f},
        .friends_tab = tab_rects[0],
        .requests_tab = tab_rects[1],
        .invites_tab = tab_rects[2],
        .list = list,
        .list_viewport = list_viewport_for(list),
    };
}

std::string avatar_label(const friend_client::social_user& user) {
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

void draw_tab(Rectangle rect, const char* label, bool selected, ui::draw_layer layer) {
    const auto& t = *g_theme;
    ui::tab_button(rect, label, {
        .layer = layer,
        .font_size = 16,
        .selected = selected,
        .style = ui::tab_button_style::raised,
        .border_width = selected ? 2.4f : 1.5f,
        .selected_border_width = 2.4f,
        .bg = t.row,
        .bg_hover = t.row_hover,
        .bg_selected = t.row_active,
        .bg_selected_hover = t.row_active,
        .border = t.border,
        .border_selected = t.border,
        .text_color = t.text,
        .selected_text_color = t.accent,
        .custom_colors = true,
    });
}

void draw_themed_button(Rectangle rect,
                        const char* label,
                        int font_size,
                        ui::draw_layer layer,
                        Color bg,
                        Color bg_hover,
                        Color text_color,
                        float border_width = 1.5f,
                        bool interactive = true) {
    ui::button(rect, label, {
        .layer = layer,
        .font_size = font_size,
        .border_width = border_width,
        .bg = bg,
        .bg_hover = bg_hover,
        .text_color = text_color,
        .custom_colors = true,
        .interactive = interactive,
    });
}

Color row_action_text_color(row_action_tone tone) {
    const auto& t = *g_theme;
    switch (tone) {
    case row_action_tone::accent_text:
    case row_action_tone::primary:
        return t.accent;
    case row_action_tone::danger:
        return t.error;
    case row_action_tone::normal:
    default:
        return t.text_muted;
    }
}

Color row_action_bg(row_action_tone tone) {
    const auto& t = *g_theme;
    return tone == row_action_tone::primary ? t.row_selected : t.row;
}

Color row_action_bg_hover(row_action_tone tone) {
    const auto& t = *g_theme;
    return tone == row_action_tone::primary ? t.row_active : t.row_hover;
}

void draw_row_action_button(Rectangle row, const row_action_button& button, ui::draw_layer layer) {
    ui::row_action_label(row, button.slot, button.label, {
        .layer = layer,
        .font_size = button.font_size,
        .border_width = 1.5f,
        .bg = row_action_bg(button.tone),
        .bg_hover = row_action_bg_hover(button.tone),
        .text_color = row_action_text_color(button.tone),
        .custom_colors = true,
    });
}

command make_command(command_type type, std::string id);

template <typename Actions>
void draw_row_action_buttons(Rectangle row, const Actions& actions, ui::draw_layer layer) {
    for (const row_action_button& button : actions) {
        draw_row_action_button(row, button, layer);
    }
}

template <typename Actions>
command row_action_command_for(Rectangle row,
                               const Actions& actions,
                               ui::draw_layer layer,
                               const std::string& id) {
    for (const row_action_button& button : actions) {
        if (button.command != command_type::none &&
            ui::row_action_clicked(row, button.slot, layer)) {
            return make_command(button.command, id);
        }
    }
    return {};
}

command single_row_action_command_for(Rectangle row,
                                      const row_action_button& button,
                                      ui::draw_layer layer,
                                      const std::string& id) {
    if (button.command != command_type::none &&
        ui::row_action_clicked(row, button.slot, layer)) {
        return make_command(button.command, id);
    }
    return {};
}

std::string presence_label(const friend_client::social_user& user) {
    if (!user.current_room_name.empty()) {
        return "in " + user.current_room_name;
    }
    if (user.online_status == "in_room") {
        return "in room";
    }
    if (user.online_status == "in_match") {
        return "in match";
    }
    if (user.online_status == "away") {
        return "away";
    }
    if (user.online_status == "online") {
        return "online";
    }
    return "offline";
}

bool presence_active(const std::string& label) {
    return label != "offline" && !label.empty();
}

void draw_user_row(Rectangle row,
                   const friend_client::social_user& user,
                   const std::string& avatar_base_url,
                   ui::draw_layer layer) {
    const auto& t = *g_theme;
    const ui::row_state row_state = ui::row(row, {
        .layer = layer,
        .border_width = 1.5f,
        .bg = t.row,
        .bg_hover = t.row_hover,
        .border_color = t.border_light,
        .custom_colors = true,
    });
    const user_row_layout row_layout = user_row_layout_for(row);
    avatar_texture_cache::draw_avatar(
        row_layout.avatar, user.avatar_url, avatar_label(user), t.row_soft_selected, t.text, 16, avatar_base_url);
    ui::draw_text_in_rect(user.display_name.empty() ? "Unknown Player" : user.display_name.c_str(),
                          20,
                          row_layout.name,
                          t.text,
                          ui::text_align::left);
    const std::string detail = presence_label(user);
    ui::draw_text_in_rect(detail.c_str(),
                          14,
                          row_layout.detail,
                          presence_active(detail) ? t.success : t.text_muted,
                          ui::text_align::left);
    (void)row_state;
    (void)layer;
}

template <typename Items, typename DrawRow>
void draw_list(Rectangle list, const Items& items, const char* empty_text, DrawRow draw_row) {
    const auto& t = *g_theme;
    ui::section(list);
    const Rectangle viewport = list_viewport_for(list);
    if (items.empty()) {
        ui::draw_text_in_rect(empty_text, 18, viewport, t.text_muted, ui::text_align::center);
        return;
    }
    ui::scoped_clip_rect clip(viewport);
    const ui::index_range fitting_rows =
        ui::vertical_list_fitting_range(items.size(), viewport, kRowHeight, kRowGap, 0.0f);
    for (int row_index = fitting_rows.begin; row_index < fitting_rows.end; ++row_index) {
        const Rectangle row = list_row_rect_for(viewport, row_index);
        draw_row(row, items[static_cast<size_t>(row_index)]);
    }
}

void draw_request_section_header(Rectangle rect, const char* label) {
    const auto& t = *g_theme;
    ui::draw_text_in_rect(label, 14, rect, t.accent, ui::text_align::left);
}

void draw_header_chrome(const friends_layout& layout,
                        const model& state,
                        ui::draw_layer layer) {
    const auto& t = *g_theme;
    ui::draw_text_in_rect("Friends", 32, layout.header_title, t.text, ui::text_align::left);
    ui::draw_text_in_rect(state.loading ? "Loading social data..." : "Social", 14,
                          layout.header_subtitle,
                          t.text_muted,
                          ui::text_align::left);
    for (const header_action_button& button : header_action_buttons_for(layout)) {
        draw_themed_button(button.rect, button.label, 13, layer, t.row, t.row_hover, t.text_muted);
    }
    for (const tab_button_layout& button : tab_buttons_for(layout)) {
        draw_tab(button.rect, button.definition.label, state.selected_tab == button.definition.value, layer);
    }
}

void draw_loading_list(const friends_layout& layout) {
    ui::section(layout.list);
    ui::draw_text_in_rect("Loading friends...", 18, layout.list_viewport,
                          g_theme->text_muted, ui::text_align::center);
}

void draw_friends_tab(const friends_layout& layout,
                      const model& state,
                      ui::draw_layer layer) {
    draw_list(layout.list, state.social.friends.friends, "No friends yet.",
              [&](Rectangle row, const friend_client::social_user& user) {
        draw_user_row(row, user, state.avatar_base_url, layer);
        draw_row_action_buttons(row, friend_row_actions(), layer);
    });
}

void draw_request_section_rows(const request_section_layout& section,
                               std::span<const friend_client::friend_request> requests,
                               const std::string& avatar_base_url,
                               bool incoming,
                               ui::draw_layer layer) {
    for (int row_index = section.rows.begin; row_index < section.rows.end; ++row_index) {
        const friend_client::friend_request& request = requests[static_cast<size_t>(row_index)];
        const Rectangle row = list_row_rect_for(section.rows_viewport, row_index);
        draw_user_row(row, incoming ? request.requester : request.addressee, avatar_base_url, layer);
        if (incoming) {
            draw_row_action_buttons(row, incoming_request_row_actions(), layer);
        } else {
            draw_row_action_buttons(row, outgoing_request_row_actions(), layer);
        }
    }
}

void draw_requests_tab(const friends_layout& layout,
                       const model& state,
                       ui::draw_layer layer) {
    const auto& t = *g_theme;
    ui::section(layout.list);
    const Rectangle viewport = layout.list_viewport;
    if (state.social.requests.incoming.empty() && state.social.requests.outgoing.empty()) {
        ui::draw_text_in_rect("No friend requests.", 18, viewport, t.text_muted, ui::text_align::center);
        return;
    }

    ui::scoped_clip_rect clip(viewport);
    const request_sections_layout sections =
        request_sections_layout_for(viewport,
                                    state.social.requests.incoming.size(),
                                    state.social.requests.outgoing.size());
    if (sections.has_incoming) {
        draw_request_section_header(sections.incoming.header, "INCOMING");
        draw_request_section_rows(sections.incoming,
                                  std::span<const friend_client::friend_request>(state.social.requests.incoming),
                                  state.avatar_base_url,
                                  true,
                                  layer);
    }
    if (sections.has_outgoing) {
        draw_request_section_header(sections.outgoing.header, "OUTGOING");
        draw_request_section_rows(sections.outgoing,
                                  std::span<const friend_client::friend_request>(state.social.requests.outgoing),
                                  state.avatar_base_url,
                                  false,
                                  layer);
    }
}

void draw_invites_tab(const friends_layout& layout,
                      const model& state,
                      ui::draw_layer layer) {
    const auto& t = *g_theme;
    draw_list(layout.list, state.social.invites.invites, "No room invites.",
              [&](Rectangle row, const friend_client::room_invite& invite) {
        draw_user_row(row, invite.sender, state.avatar_base_url, layer);
        ui::draw_text_in_rect(invite.room_name.empty() ? "Room invite" : invite.room_name.c_str(),
                              14,
                              user_row_layout_for(row).detail,
                              t.text_muted,
                              ui::text_align::left);
        draw_row_action_buttons(row, invite_row_actions(invite.read), layer);
    });
}

command make_command(command_type type, std::string id = {}) {
    return command{.type = type, .id = std::move(id)};
}

command make_tab_command(tab selected_tab) {
    return command{.type = command_type::select_tab, .selected_tab = selected_tab};
}

command header_action_command_for(const friends_layout& layout, ui::draw_layer layer) {
    for (const header_action_button& button : header_action_buttons_for(layout)) {
        if (ui::is_clicked(button.rect, layer)) {
            return make_command(button.command);
        }
    }
    return {};
}

command tab_command_for(const friends_layout& layout, ui::draw_layer layer) {
    for (const tab_button_layout& button : tab_buttons_for(layout)) {
        if (ui::is_clicked(button.rect, layer)) {
            return make_tab_command(button.definition.value);
        }
    }
    return {};
}

command friends_tab_command_for(const friends_layout& layout, const model& state, ui::draw_layer layer) {
    const Rectangle viewport = layout.list_viewport;
    const ui::index_range fitting_rows = ui::vertical_list_fitting_range(
        state.social.friends.friends.size(), viewport, kRowHeight, kRowGap, 0.0f);
    for (int row_index = fitting_rows.begin; row_index < fitting_rows.end; ++row_index) {
        const friend_client::social_user& user = state.social.friends.friends[static_cast<size_t>(row_index)];
        const Rectangle row = list_row_rect_for(viewport, row_index);
        const command row_command = row_action_command_for(row, friend_row_actions(), layer, user.id);
        if (row_command.type != command_type::none) {
            return row_command;
        }
    }
    return {};
}

command incoming_request_command_for(const request_section_layout& section,
                                     std::span<const friend_client::friend_request> requests,
                                     ui::draw_layer layer) {
    for (int row_index = section.rows.begin; row_index < section.rows.end; ++row_index) {
        const friend_client::friend_request& request = requests[static_cast<size_t>(row_index)];
        const Rectangle row = list_row_rect_for(section.rows_viewport, row_index);
        for (const row_action_button& button : incoming_request_row_actions()) {
            const std::string& command_id =
                button.command == command_type::open_profile ? request.requester.id : request.id;
            const command row_command = single_row_action_command_for(row, button, layer, command_id);
            if (row_command.type != command_type::none) {
                return row_command;
            }
        }
    }
    return {};
}

command outgoing_request_command_for(const request_section_layout& section,
                                     std::span<const friend_client::friend_request> requests,
                                     ui::draw_layer layer) {
    for (int row_index = section.rows.begin; row_index < section.rows.end; ++row_index) {
        const friend_client::friend_request& request = requests[static_cast<size_t>(row_index)];
        const Rectangle row = list_row_rect_for(section.rows_viewport, row_index);
        const command row_command =
            row_action_command_for(row, outgoing_request_row_actions(), layer, request.addressee.id);
        if (row_command.type != command_type::none) {
            return row_command;
        }
    }
    return {};
}

command requests_tab_command_for(const friends_layout& layout, const model& state, ui::draw_layer layer) {
    const request_sections_layout sections =
        request_sections_layout_for(layout.list_viewport,
                                    state.social.requests.incoming.size(),
                                    state.social.requests.outgoing.size());
    if (sections.has_incoming) {
        const command row_command =
            incoming_request_command_for(sections.incoming,
                                         std::span<const friend_client::friend_request>(
                                             state.social.requests.incoming),
                                         layer);
        if (row_command.type != command_type::none) {
            return row_command;
        }
    }
    if (sections.has_outgoing) {
        return outgoing_request_command_for(sections.outgoing,
                                            std::span<const friend_client::friend_request>(
                                                state.social.requests.outgoing),
                                            layer);
    }
    return {};
}

command invites_tab_command_for(const friends_layout& layout, const model& state, ui::draw_layer layer) {
    const Rectangle viewport = layout.list_viewport;
    const ui::index_range fitting_rows = ui::vertical_list_fitting_range(
        state.social.invites.invites.size(), viewport, kRowHeight, kRowGap, 0.0f);
    for (int row_index = fitting_rows.begin; row_index < fitting_rows.end; ++row_index) {
        const friend_client::room_invite& invite = state.social.invites.invites[static_cast<size_t>(row_index)];
        const Rectangle row = list_row_rect_for(viewport, row_index);
        const command row_command = row_action_command_for(row, invite_row_actions(invite.read), layer, invite.id);
        if (row_command.type != command_type::none) {
            return row_command;
        }
    }
    return {};
}

command active_tab_command_for(const friends_layout& layout, const model& state, ui::draw_layer layer) {
    if (state.operation_active) {
        return {};
    }
    if (state.selected_tab == tab::friends) {
        return friends_tab_command_for(layout, state, layer);
    }
    if (state.selected_tab == tab::requests) {
        return requests_tab_command_for(layout, state, layer);
    }
    if (state.selected_tab == tab::invites) {
        return invites_tab_command_for(layout, state, layer);
    }
    return {};
}

}  // namespace

Rectangle modal_bounds(float open_anim) {
    return ui::animated_modal_rect(kModalRect, open_anim);
}

command handle_input(const model& state, ui::draw_layer layer) {
    const friends_layout layout = make_layout(state.open_anim);
    if (const command header_command = header_action_command_for(layout, layer);
        header_command.type != command_type::none) {
        return header_command;
    }
    if (const command tab_command = tab_command_for(layout, layer);
        tab_command.type != command_type::none) {
        return tab_command;
    }
    if (const command row_command = active_tab_command_for(layout, state, layer);
        row_command.type != command_type::none) {
        return row_command;
    }

    if (ui::modal_outside_released(layout.modal, virtual_screen::get_virtual_mouse())) {
        return make_command(command_type::close);
    }
    return {};
}

void draw(const model& state, ui::draw_layer layer, bool draw_backdrop) {
    ui::enqueue_draw_command(layer, [state, layer, draw_backdrop]() {
        const friends_layout layout = make_layout(state.open_anim);
        if (draw_backdrop) {
            ui::draw_fullscreen_overlay(with_alpha(BLACK, 132));
        }
        ui::panel(layout.modal);
        draw_header_chrome(layout, state, layer);

        if (state.loading && !state.loaded_once) {
            draw_loading_list(layout);
            return;
        }

        if (state.selected_tab == tab::friends) {
            draw_friends_tab(layout, state, layer);
            return;
        }
        if (state.selected_tab == tab::requests) {
            draw_requests_tab(layout, state, layer);
            return;
        }
        draw_invites_tab(layout, state, layer);
    });
}

}  // namespace title_friends_view
