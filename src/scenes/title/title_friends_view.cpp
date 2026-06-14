#include "title/title_friends_view.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace title_friends_view {
namespace {

constexpr Rectangle kModalRect{470.0f, 170.0f, 980.0f, 720.0f};
constexpr float kRowHeight = 72.0f;
constexpr float kRowGap = 10.0f;

struct friends_layout {
    Rectangle modal{};
    Rectangle close_button{};
    Rectangle refresh_button{};
    Rectangle friends_tab{};
    Rectangle requests_tab{};
    Rectangle invites_tab{};
    Rectangle list{};
};

Rectangle animated_bounds(float open_anim) {
    const float t = tween::ease_out_cubic(std::clamp(open_anim, 0.0f, 1.0f));
    const float scale = 0.94f + 0.06f * t;
    const Vector2 center{kModalRect.x + kModalRect.width * 0.5f,
                         kModalRect.y + kModalRect.height * 0.5f};
    return {
        center.x - kModalRect.width * scale * 0.5f,
        center.y - kModalRect.height * scale * 0.5f,
        kModalRect.width * scale,
        kModalRect.height * scale,
    };
}

friends_layout make_layout(float open_anim) {
    const Rectangle modal = animated_bounds(open_anim);
    const Rectangle content = ui::inset(modal, ui::edge_insets::uniform(28.0f));
    const float tab_y = content.y + 76.0f;
    return {
        .modal = modal,
        .close_button = {content.x + content.width - 92.0f, content.y, 92.0f, 42.0f},
        .refresh_button = {content.x + content.width - 202.0f, content.y, 96.0f, 42.0f},
        .friends_tab = {content.x, tab_y, 150.0f, 44.0f},
        .requests_tab = {content.x + 164.0f, tab_y, 164.0f, 44.0f},
        .invites_tab = {content.x + 342.0f, tab_y, 150.0f, 44.0f},
        .list = {content.x, tab_y + 62.0f, content.width, content.height - 138.0f},
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
    const bool hovered = ui::is_hovered(rect, layer);
    const bool pressed = ui::is_pressed(rect, layer);
    ui::detail::draw_button_visual(
        rect,
        hovered || selected,
        pressed,
        label,
        16,
        selected ? t.row_selected : t.row,
        selected ? t.row_active : t.row_hover,
        selected ? t.accent : t.text,
        selected ? 2.4f : 1.5f);
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

void draw_user_row(Rectangle row, const friend_client::social_user& user, ui::draw_layer layer) {
    const auto& t = *g_theme;
    const ui::row_state row_state = ui::draw_row(row, t.row, t.row_hover, t.border_light, 1.5f);
    const Rectangle avatar_rect{row.x + 14.0f, row.y + 12.0f, 48.0f, 48.0f};
    avatar_texture_cache::draw_avatar(avatar_rect, user.avatar_url, avatar_label(user), t.row_soft_selected, t.text, 16);
    ui::draw_text_in_rect(user.display_name.empty() ? "Unknown Player" : user.display_name.c_str(),
                          20,
                          {row.x + 78.0f, row.y + 11.0f, row.width - 420.0f, 28.0f},
                          t.text,
                          ui::text_align::left);
    const std::string detail = presence_label(user);
    ui::draw_text_in_rect(detail.c_str(),
                          14,
                          {row.x + 78.0f, row.y + 41.0f, row.width - 420.0f, 20.0f},
                          presence_active(detail) ? t.success : t.text_muted,
                          ui::text_align::left);
    (void)row_state;
    (void)layer;
}

template <typename Items, typename DrawRow>
void draw_list(Rectangle list, const Items& items, const char* empty_text, DrawRow draw_row) {
    const auto& t = *g_theme;
    ui::draw_section(list);
    const Rectangle viewport = ui::inset(list, ui::edge_insets::uniform(14.0f));
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
        draw_row({viewport.x, y, viewport.width, kRowHeight}, item);
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
    return animated_bounds(open_anim);
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

    const Rectangle viewport = ui::inset(layout.list, ui::edge_insets::uniform(14.0f));
    float y = viewport.y;
    if (state.selected_tab == tab::friends && !state.operation_active) {
        for (const friend_client::social_user& user : state.social.friends.friends) {
            const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
            const Rectangle profile_button{row.x + row.width - 314.0f, row.y + 17.0f, 92.0f, 38.0f};
            const Rectangle remove_button{row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f};
            const Rectangle block_button{row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f};
            if (ui::is_clicked(profile_button, layer)) {
                return make_command(command_type::open_profile, user.id);
            }
            if (ui::is_clicked(remove_button, layer)) {
                return make_command(command_type::remove_friend, user.id);
            }
            if (ui::is_clicked(block_button, layer)) {
                return make_command(command_type::block_user, user.id);
            }
            y += kRowHeight + kRowGap;
        }
    } else if (state.selected_tab == tab::requests && !state.operation_active) {
        if (!state.social.requests.incoming.empty()) {
            y += 30.0f;
            for (const friend_client::friend_request& request : state.social.requests.incoming) {
                const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
                const Rectangle profile_button{row.x + row.width - 314.0f, row.y + 17.0f, 92.0f, 38.0f};
                const Rectangle accept_button{row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f};
                const Rectangle decline_button{row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f};
                if (ui::is_clicked(profile_button, layer)) {
                    return make_command(command_type::open_profile, request.requester.id);
                }
                if (ui::is_clicked(accept_button, layer)) {
                    return make_command(command_type::accept_request, request.id);
                }
                if (ui::is_clicked(decline_button, layer)) {
                    return make_command(command_type::decline_request, request.id);
                }
                y += kRowHeight + kRowGap;
            }
        }
        if (!state.social.requests.outgoing.empty()) {
            y += state.social.requests.incoming.empty() ? 30.0f : 38.0f;
            for (const friend_client::friend_request& request : state.social.requests.outgoing) {
                const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
                const Rectangle profile_button{row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f};
                if (ui::is_clicked(profile_button, layer)) {
                    return make_command(command_type::open_profile, request.addressee.id);
                }
                y += kRowHeight + kRowGap;
            }
        }
    } else if (state.selected_tab == tab::invites && !state.operation_active) {
        for (const friend_client::room_invite& invite : state.social.invites.invites) {
            const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
            const Rectangle accept_button{row.x + row.width - 314.0f, row.y + 17.0f, 92.0f, 38.0f};
            const Rectangle read_button{row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f};
            const Rectangle decline_button{row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f};
            if (ui::is_clicked(accept_button, layer)) {
                return make_command(command_type::accept_invite, invite.id);
            }
            if (!invite.read && ui::is_clicked(read_button, layer)) {
                return make_command(command_type::mark_invite_read, invite.id);
            }
            if (ui::is_clicked(decline_button, layer)) {
                return make_command(command_type::decline_invite, invite.id);
            }
            y += kRowHeight + kRowGap;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), layout.modal)) {
        return make_command(command_type::close);
    }
    return {};
}

void draw(const model& state, ui::draw_layer layer) {
    ui::enqueue_draw_command(layer, [state, layer]() {
        const auto& t = *g_theme;
        const friends_layout layout = make_layout(state.open_anim);
        ui::draw_fullscreen_overlay(with_alpha(BLACK, 132));
        ui::draw_panel(layout.modal);
        const Rectangle content = ui::inset(layout.modal, ui::edge_insets::uniform(28.0f));
        ui::draw_text_in_rect("Friends", 32, {content.x, content.y, content.width - 240.0f, 44.0f}, t.text, ui::text_align::left);
        ui::draw_text_in_rect(state.loading ? "Loading social data..." : "Social", 14,
                              {content.x, content.y + 42.0f, content.width - 240.0f, 24.0f},
                              t.text_muted,
                              ui::text_align::left);
        ui::detail::draw_button_visual(layout.refresh_button, ui::is_hovered(layout.refresh_button, layer),
                                       ui::is_pressed(layout.refresh_button, layer), "REFRESH", 13,
                                       t.row, t.row_hover, t.text_muted, 1.5f);
        ui::detail::draw_button_visual(layout.close_button, ui::is_hovered(layout.close_button, layer),
                                       ui::is_pressed(layout.close_button, layer), "CLOSE", 13,
                                       t.row, t.row_hover, t.text_muted, 1.5f);
        draw_tab(layout.friends_tab, "Friends", state.selected_tab == tab::friends, layer);
        draw_tab(layout.requests_tab, "Requests", state.selected_tab == tab::requests, layer);
        draw_tab(layout.invites_tab, "Invites", state.selected_tab == tab::invites, layer);

        if (state.loading && !state.loaded_once) {
            ui::draw_section(layout.list);
            ui::draw_text_in_rect("Loading friends...", 18, layout.list, t.text_muted, ui::text_align::center);
            return;
        }

        if (state.selected_tab == tab::friends) {
            draw_list(layout.list, state.social.friends.friends, "No friends yet.", [&](Rectangle row, const friend_client::social_user& user) {
                draw_user_row(row, user, layer);
                ui::detail::draw_button_visual({row.x + row.width - 314.0f, row.y + 17.0f, 92.0f, 38.0f},
                                               false, false, "PROFILE", 12, t.row, t.row_hover, t.text_muted, 1.5f);
                ui::detail::draw_button_visual({row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f},
                                               false, false, "REMOVE", 12, t.row, t.row_hover, t.text_muted, 1.5f);
                ui::detail::draw_button_visual({row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f},
                                               false, false, "BLOCK", 12, t.row, t.row_hover, t.error, 1.5f);
            });
            return;
        }
        if (state.selected_tab == tab::requests) {
            ui::draw_section(layout.list);
            const Rectangle viewport = ui::inset(layout.list, ui::edge_insets::uniform(14.0f));
            if (state.social.requests.incoming.empty() && state.social.requests.outgoing.empty()) {
                ui::draw_text_in_rect("No friend requests.", 18, viewport, t.text_muted, ui::text_align::center);
                return;
            }
            ui::scoped_clip_rect clip(viewport);
            float y = viewport.y;
            if (!state.social.requests.incoming.empty()) {
                draw_request_section_header({viewport.x, y, viewport.width, 22.0f}, "INCOMING");
                y += 30.0f;
                for (const friend_client::friend_request& request : state.social.requests.incoming) {
                    if (y + kRowHeight > viewport.y + viewport.height) {
                        break;
                    }
                    const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
                    draw_user_row(row, request.requester, layer);
                    ui::detail::draw_button_visual({row.x + row.width - 314.0f, row.y + 17.0f, 92.0f, 38.0f},
                                                   false, false, "PROFILE", 12, t.row, t.row_hover, t.text_muted, 1.5f);
                    ui::detail::draw_button_visual({row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f},
                                                   false, false, "ACCEPT", 13, t.row_selected, t.row_active, t.accent, 1.5f);
                    ui::detail::draw_button_visual({row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f},
                                                   false, false, "DECLINE", 13, t.row, t.row_hover, t.text_muted, 1.5f);
                    y += kRowHeight + kRowGap;
                }
            }
            if (!state.social.requests.outgoing.empty() && y + 30.0f <= viewport.y + viewport.height) {
                y += state.social.requests.incoming.empty() ? 0.0f : 8.0f;
                draw_request_section_header({viewport.x, y, viewport.width, 22.0f}, "OUTGOING");
                y += 30.0f;
                for (const friend_client::friend_request& request : state.social.requests.outgoing) {
                    if (y + kRowHeight > viewport.y + viewport.height) {
                        break;
                    }
                    const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
                    draw_user_row(row, request.addressee, layer);
                    ui::detail::draw_button_visual({row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f},
                                                   false, false, "PENDING", 12, t.row, t.row_hover, t.text_muted, 1.5f);
                    ui::detail::draw_button_visual({row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f},
                                                   false, false, "PROFILE", 12, t.row, t.row_hover, t.text_muted, 1.5f);
                    y += kRowHeight + kRowGap;
                }
            }
            return;
        }
        draw_list(layout.list, state.social.invites.invites, "No room invites.", [&](Rectangle row, const friend_client::room_invite& invite) {
            draw_user_row(row, invite.sender, layer);
            ui::draw_text_in_rect(invite.room_name.empty() ? "Room invite" : invite.room_name.c_str(),
                                  14,
                                  {row.x + 78.0f, row.y + 41.0f, row.width - 420.0f, 20.0f},
                                  t.text_muted,
                                  ui::text_align::left);
            ui::detail::draw_button_visual({row.x + row.width - 314.0f, row.y + 17.0f, 92.0f, 38.0f},
                                           false, false, "JOIN", 13, t.row_selected, t.row_active, t.accent, 1.5f);
            ui::detail::draw_button_visual({row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f},
                                           false, false, invite.read ? "SEEN" : "READ", 12, t.row, t.row_hover,
                                           invite.read ? t.text_muted : t.accent, 1.5f);
            ui::detail::draw_button_visual({row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f},
                                           false, false, "DECLINE", 13, t.row, t.row_hover, t.text_muted, 1.5f);
        });
    });
}

}  // namespace title_friends_view
