#include "title/title_friends_view.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_modal.h"
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

enum class row_action_slot {
    right = 0,
    middle = 1,
    left = 2,
};

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

struct user_row_layout {
    Rectangle avatar{};
    Rectangle name{};
    Rectangle detail{};
};

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

constexpr Rectangle list_row_rect_for(Rectangle viewport, float y) {
    return {viewport.x, y, viewport.width, kRowHeight};
}

constexpr Rectangle request_section_header_rect_for(Rectangle viewport, float y) {
    return {viewport.x, y, viewport.width, kRequestHeaderHeight};
}

constexpr bool row_fits(Rectangle viewport, float y) {
    return y + kRowHeight <= viewport.y + viewport.height;
}

friends_layout make_layout(float open_anim) {
    const Rectangle modal = ui::animated_modal_rect(kModalRect, open_anim);
    const Rectangle content = ui::inset(modal, ui::edge_insets::uniform(kModalPadding));
    const Rectangle header_actions{content.x, content.y, content.width, kHeaderButtonHeight};
    const ui::rect_pair close_split = ui::split_trailing(header_actions, kCloseButtonWidth, kHeaderButtonGap);
    const ui::rect_pair refresh_split = ui::split_trailing(close_split.first, kRefreshButtonWidth, kHeaderButtonGap);
    const float tab_widths[] = {kFriendsTabWidth, kRequestsTabWidth, kInvitesTabWidth};
    Rectangle tab_rects[3]{};
    ui::hstack_widths({content.x, content.y + kTabTopOffset, content.width, kTabHeight},
                      tab_widths,
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

int row_action_slot_index(row_action_slot slot) {
    return static_cast<int>(slot);
}

bool row_action_clicked(Rectangle row, row_action_slot slot, ui::draw_layer layer) {
    return ui::is_clicked(ui::row_action_rect(row, row_action_slot_index(slot)), layer);
}

void draw_row_action_button(Rectangle row,
                            row_action_slot slot,
                            const char* label,
                            int font_size,
                            ui::draw_layer layer,
                            Color bg,
                            Color bg_hover,
                            Color text_color) {
    ui::row_action_button(row, row_action_slot_index(slot), label, {
        .layer = layer,
        .font_size = font_size,
        .border_width = 1.5f,
        .bg = bg,
        .bg_hover = bg_hover,
        .text_color = text_color,
        .custom_colors = true,
        .interactive = false,
    });
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
    float y = viewport.y;
    for (const auto& item : items) {
        if (y + kRowHeight > viewport.y + viewport.height) {
            break;
        }
        draw_row(list_row_rect_for(viewport, y), item);
        y += kRowHeight + kRowGap;
    }
}

void draw_request_section_header(Rectangle rect, const char* label) {
    const auto& t = *g_theme;
    ui::draw_text_in_rect(label, 14, rect, t.accent, ui::text_align::left);
}

command make_command(command_type type, std::string id = {}) {
    return command{.type = type, .id = std::move(id)};
}

command make_tab_command(tab selected_tab) {
    return command{.type = command_type::select_tab, .selected_tab = selected_tab};
}

}  // namespace

Rectangle modal_bounds(float open_anim) {
    return ui::animated_modal_rect(kModalRect, open_anim);
}

command handle_input(const model& state, ui::draw_layer layer) {
    const friends_layout layout = make_layout(state.open_anim);
    if (ui::is_clicked(layout.close_button, layer)) {
        return make_command(command_type::close);
    }
    if (ui::is_clicked(layout.refresh_button, layer)) {
        return make_command(command_type::refresh);
    }
    if (ui::is_clicked(layout.friends_tab, layer)) {
        return make_tab_command(tab::friends);
    }
    if (ui::is_clicked(layout.requests_tab, layer)) {
        return make_tab_command(tab::requests);
    }
    if (ui::is_clicked(layout.invites_tab, layer)) {
        return make_tab_command(tab::invites);
    }

    const Rectangle viewport = layout.list_viewport;
    float y = viewport.y;
    if (state.selected_tab == tab::friends && !state.operation_active) {
        for (const friend_client::social_user& user : state.social.friends.friends) {
            const Rectangle row = list_row_rect_for(viewport, y);
            if (row_action_clicked(row, row_action_slot::left, layer)) {
                return make_command(command_type::open_profile, user.id);
            }
            if (row_action_clicked(row, row_action_slot::middle, layer)) {
                return make_command(command_type::remove_friend, user.id);
            }
            if (row_action_clicked(row, row_action_slot::right, layer)) {
                return make_command(command_type::block_user, user.id);
            }
            y += kRowHeight + kRowGap;
        }
    } else if (state.selected_tab == tab::requests && !state.operation_active) {
        if (!state.social.requests.incoming.empty()) {
            y += kRequestHeaderAdvance;
            for (const friend_client::friend_request& request : state.social.requests.incoming) {
                const Rectangle row = list_row_rect_for(viewport, y);
                if (row_action_clicked(row, row_action_slot::left, layer)) {
                    return make_command(command_type::open_profile, request.requester.id);
                }
                if (row_action_clicked(row, row_action_slot::middle, layer)) {
                    return make_command(command_type::accept_request, request.id);
                }
                if (row_action_clicked(row, row_action_slot::right, layer)) {
                    return make_command(command_type::decline_request, request.id);
                }
                y += kRowHeight + kRowGap;
            }
        }
        if (!state.social.requests.outgoing.empty()) {
            y += state.social.requests.incoming.empty()
                ? kRequestHeaderAdvance
                : kRequestHeaderAdvance + kRequestSectionGap;
            for (const friend_client::friend_request& request : state.social.requests.outgoing) {
                const Rectangle row = list_row_rect_for(viewport, y);
                if (row_action_clicked(row, row_action_slot::right, layer)) {
                    return make_command(command_type::open_profile, request.addressee.id);
                }
                y += kRowHeight + kRowGap;
            }
        }
    } else if (state.selected_tab == tab::invites && !state.operation_active) {
        for (const friend_client::room_invite& invite : state.social.invites.invites) {
            const Rectangle row = list_row_rect_for(viewport, y);
            if (row_action_clicked(row, row_action_slot::left, layer)) {
                return make_command(command_type::accept_invite, invite.id);
            }
            if (!invite.read && row_action_clicked(row, row_action_slot::middle, layer)) {
                return make_command(command_type::mark_invite_read, invite.id);
            }
            if (row_action_clicked(row, row_action_slot::right, layer)) {
                return make_command(command_type::decline_invite, invite.id);
            }
            y += kRowHeight + kRowGap;
        }
    }

    if (ui::modal_outside_released(layout.modal, virtual_screen::get_virtual_mouse())) {
        return make_command(command_type::close);
    }
    return {};
}

void draw(const model& state, ui::draw_layer layer, bool draw_backdrop) {
    ui::enqueue_draw_command(layer, [state, layer, draw_backdrop]() {
        const auto& t = *g_theme;
        const friends_layout layout = make_layout(state.open_anim);
        if (draw_backdrop) {
            ui::draw_fullscreen_overlay(with_alpha(BLACK, 132));
        }
        ui::panel(layout.modal);
        ui::draw_text_in_rect("Friends", 32, layout.header_title, t.text, ui::text_align::left);
        ui::draw_text_in_rect(state.loading ? "Loading social data..." : "Social", 14,
                              layout.header_subtitle,
                              t.text_muted,
                              ui::text_align::left);
        draw_themed_button(layout.refresh_button, "REFRESH", 13, layer, t.row, t.row_hover, t.text_muted);
        draw_themed_button(layout.close_button, "CLOSE", 13, layer, t.row, t.row_hover, t.text_muted);
        draw_tab(layout.friends_tab, "Friends", state.selected_tab == tab::friends, layer);
        draw_tab(layout.requests_tab, "Requests", state.selected_tab == tab::requests, layer);
        draw_tab(layout.invites_tab, "Invites", state.selected_tab == tab::invites, layer);

        if (state.loading && !state.loaded_once) {
            ui::section(layout.list);
            ui::draw_text_in_rect("Loading friends...", 18, layout.list_viewport, t.text_muted, ui::text_align::center);
            return;
        }

        if (state.selected_tab == tab::friends) {
            draw_list(layout.list, state.social.friends.friends, "No friends yet.", [&](Rectangle row, const friend_client::social_user& user) {
                draw_user_row(row, user, state.avatar_base_url, layer);
                draw_row_action_button(row, row_action_slot::left,
                                       "PROFILE", 12, layer, t.row, t.row_hover, t.text_muted);
                draw_row_action_button(row, row_action_slot::middle,
                                       "REMOVE", 12, layer, t.row, t.row_hover, t.text_muted);
                draw_row_action_button(row, row_action_slot::right,
                                       "BLOCK", 12, layer, t.row, t.row_hover, t.error);
            });
            return;
        }
        if (state.selected_tab == tab::requests) {
            ui::section(layout.list);
            const Rectangle viewport = layout.list_viewport;
            if (state.social.requests.incoming.empty() && state.social.requests.outgoing.empty()) {
                ui::draw_text_in_rect("No friend requests.", 18, viewport, t.text_muted, ui::text_align::center);
                return;
            }
            ui::scoped_clip_rect clip(viewport);
            float y = viewport.y;
            if (!state.social.requests.incoming.empty()) {
                draw_request_section_header(request_section_header_rect_for(viewport, y), "INCOMING");
                y += kRequestHeaderAdvance;
                for (const friend_client::friend_request& request : state.social.requests.incoming) {
                    if (!row_fits(viewport, y)) {
                        break;
                    }
                    const Rectangle row = list_row_rect_for(viewport, y);
                    draw_user_row(row, request.requester, state.avatar_base_url, layer);
                    draw_row_action_button(row, row_action_slot::left,
                                           "PROFILE", 12, layer, t.row, t.row_hover, t.text_muted);
                    draw_row_action_button(row, row_action_slot::middle,
                                           "ACCEPT", 13, layer, t.row_selected, t.row_active, t.accent);
                    draw_row_action_button(row, row_action_slot::right,
                                           "DECLINE", 13, layer, t.row, t.row_hover, t.text_muted);
                    y += kRowHeight + kRowGap;
                }
            }
            if (!state.social.requests.outgoing.empty() &&
                y + kRequestHeaderAdvance <= viewport.y + viewport.height) {
                y += state.social.requests.incoming.empty() ? 0.0f : kRequestSectionGap;
                draw_request_section_header(request_section_header_rect_for(viewport, y), "OUTGOING");
                y += kRequestHeaderAdvance;
                for (const friend_client::friend_request& request : state.social.requests.outgoing) {
                    if (!row_fits(viewport, y)) {
                        break;
                    }
                    const Rectangle row = list_row_rect_for(viewport, y);
                    draw_user_row(row, request.addressee, state.avatar_base_url, layer);
                    draw_row_action_button(row, row_action_slot::middle,
                                           "PENDING", 12, layer, t.row, t.row_hover, t.text_muted);
                    draw_row_action_button(row, row_action_slot::right,
                                           "PROFILE", 12, layer, t.row, t.row_hover, t.text_muted);
                    y += kRowHeight + kRowGap;
                }
            }
            return;
        }
        draw_list(layout.list, state.social.invites.invites, "No room invites.", [&](Rectangle row, const friend_client::room_invite& invite) {
            draw_user_row(row, invite.sender, state.avatar_base_url, layer);
            ui::draw_text_in_rect(invite.room_name.empty() ? "Room invite" : invite.room_name.c_str(),
                                  14,
                                  user_row_layout_for(row).detail,
                                  t.text_muted,
                                  ui::text_align::left);
            draw_row_action_button(row, row_action_slot::left,
                                   "JOIN", 13, layer, t.row_selected, t.row_active, t.accent);
            draw_row_action_button(row, row_action_slot::middle,
                                   invite.read ? "SEEN" : "READ", 12, layer, t.row, t.row_hover,
                                   invite.read ? t.text_muted : t.accent);
            draw_row_action_button(row, row_action_slot::right,
                                   "DECLINE", 13, layer, t.row, t.row_hover, t.text_muted);
        });
    });
}

}  // namespace title_friends_view
