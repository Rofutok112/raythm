#include "title/title_friends_controller.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <string>
#include <thread>
#include <utility>

#include "scene_common.h"
#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_notice.h"
#include "virtual_screen.h"

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
    const std::string status = user.online_status.empty() ? "offline" : user.online_status;
    const std::string detail = user.current_room_name.empty() ? status : "in " + user.current_room_name;
    ui::draw_text_in_rect(detail.c_str(),
                          14,
                          {row.x + 78.0f, row.y + 41.0f, row.width - 420.0f, 20.0f},
                          detail == "offline" ? t.text_muted : t.success,
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

}  // namespace

void title_friends_controller::reset() {
    state_ = {};
}

void title_friends_controller::open() {
    if (state_.open && !state_.closing) {
        return;
    }
    state_.open = true;
    state_.closing = false;
    state_.suppress_background_close_until_release = true;
    request_reload();
}

void title_friends_controller::close() {
    if (state_.open) {
        state_.closing = true;
    }
}

void title_friends_controller::tick(float dt) {
    if (state_.open && state_.closing) {
        state_.open_anim = tween::retreat(state_.open_anim, dt, 8.0f);
        if (state_.open_anim <= 0.0f) {
            state_.open = false;
            state_.closing = false;
        }
    } else if (state_.open) {
        state_.open_anim = tween::advance(state_.open_anim, dt, 8.0f);
    } else {
        state_.open_anim = 0.0f;
    }
}

void title_friends_controller::poll() {
    auto ready = [](auto& future) {
        return future.valid() && future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
    };
    try {
        if (ready(friends_future_)) {
            const friend_client::friend_listing_result result = friends_future_.get();
            if (result.success && result.listing.has_value()) {
                state_.friends = *result.listing;
            } else {
                state_.message = result.message;
                ui::notify(result.message, ui::notice_tone::error, 2.8f);
            }
        }
        if (ready(requests_future_)) {
            const friend_client::request_listing_result result = requests_future_.get();
            if (result.success && result.listing.has_value()) {
                state_.requests = *result.listing;
            } else {
                state_.message = result.message;
                ui::notify(result.message, ui::notice_tone::error, 2.8f);
            }
        }
        if (ready(invites_future_)) {
            const friend_client::invite_listing_result result = invites_future_.get();
            if (result.success && result.listing.has_value()) {
                state_.invites = *result.listing;
            } else {
                state_.message = result.message;
                ui::notify(result.message, ui::notice_tone::error, 2.8f);
            }
            state_.loading = false;
            state_.loaded_once = true;
        }
        if (ready(operation_future_)) {
            const friend_client::operation_result result = operation_future_.get();
            state_.operation_active = false;
            apply_operation_result(result);
        }
        if (ready(invite_operation_future_)) {
            const friend_client::invite_operation_result result = invite_operation_future_.get();
            state_.operation_active = false;
            if (result.success && !result.join_room_id.empty() && !result.join_invite_id.empty()) {
                state_.pending_room_join = room_join_request{
                    .room_id = result.join_room_id,
                    .invite_id = result.join_invite_id,
                };
                close();
            }
            apply_operation_result(result);
        }
    } catch (const std::exception& ex) {
        state_.loading = false;
        state_.operation_active = false;
        state_.message = ex.what();
        ui::notify(state_.message, ui::notice_tone::error, 2.8f);
    } catch (...) {
        state_.loading = false;
        state_.operation_active = false;
        state_.message = "Friends operation failed.";
        ui::notify(state_.message, ui::notice_tone::error, 2.8f);
    }
}

bool title_friends_controller::handle_input() {
    if (!state_.open) {
        return false;
    }
    if (state_.suppress_background_close_until_release) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            return true;
        }
        state_.suppress_background_close_until_release = false;
    }
    const friends_layout layout = make_layout(state_.open_anim);
    if ((IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) && !state_.operation_active) {
        close();
        return true;
    }
    if (ui::is_clicked(layout.close_button, ui::draw_layer::modal)) {
        close();
        return true;
    }
    if (ui::is_clicked(layout.refresh_button, ui::draw_layer::modal)) {
        request_reload();
        return true;
    }
    if (ui::is_clicked(layout.friends_tab, ui::draw_layer::modal)) {
        state_.selected_tab = tab::friends;
        return true;
    }
    if (ui::is_clicked(layout.requests_tab, ui::draw_layer::modal)) {
        state_.selected_tab = tab::requests;
        return true;
    }
    if (ui::is_clicked(layout.invites_tab, ui::draw_layer::modal)) {
        state_.selected_tab = tab::invites;
        return true;
    }

    const Rectangle viewport = ui::inset(layout.list, ui::edge_insets::uniform(14.0f));
    float y = viewport.y;
    if (state_.selected_tab == tab::friends && !state_.operation_active) {
        for (const friend_client::social_user& user : state_.friends.friends) {
            const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
            const Rectangle profile_button{row.x + row.width - 314.0f, row.y + 17.0f, 92.0f, 38.0f};
            const Rectangle remove_button{row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f};
            const Rectangle block_button{row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f};
            if (ui::is_clicked(profile_button, ui::draw_layer::modal)) {
                state_.pending_profile_user_id = user.id;
                close();
                return true;
            }
            if (ui::is_clicked(remove_button, ui::draw_layer::modal)) {
                start_remove_friend(user.id);
                return true;
            }
            if (ui::is_clicked(block_button, ui::draw_layer::modal)) {
                start_block_user(user.id);
                return true;
            }
            y += kRowHeight + kRowGap;
        }
    } else if (state_.selected_tab == tab::requests && !state_.operation_active) {
        for (const friend_client::friend_request& request : state_.requests.incoming) {
            const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
            const Rectangle accept_button{row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f};
            const Rectangle decline_button{row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f};
            if (ui::is_clicked(accept_button, ui::draw_layer::modal)) {
                start_accept_request(request.id);
                return true;
            }
            if (ui::is_clicked(decline_button, ui::draw_layer::modal)) {
                start_decline_request(request.id);
                return true;
            }
            y += kRowHeight + kRowGap;
        }
    } else if (state_.selected_tab == tab::invites && !state_.operation_active) {
        for (const friend_client::room_invite& invite : state_.invites.invites) {
            const Rectangle row{viewport.x, y, viewport.width, kRowHeight};
            const Rectangle accept_button{row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f};
            const Rectangle decline_button{row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f};
            if (ui::is_clicked(accept_button, ui::draw_layer::modal)) {
                start_accept_invite(invite.id);
                return true;
            }
            if (ui::is_clicked(decline_button, ui::draw_layer::modal)) {
                start_decline_invite(invite.id);
                return true;
            }
            y += kRowHeight + kRowGap;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), layout.modal)) {
        close();
        return true;
    }
    return true;
}

void title_friends_controller::draw(ui::draw_layer layer) {
    if (!state_.open) {
        return;
    }
    ui::enqueue_draw_command(layer, [state = state_, layer]() {
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
            draw_list(layout.list, state.friends.friends, "No friends yet.", [&](Rectangle row, const friend_client::social_user& user) {
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
            draw_list(layout.list, state.requests.incoming, "No incoming friend requests.", [&](Rectangle row, const friend_client::friend_request& request) {
                const friend_client::social_user& user = request.requester;
                draw_user_row(row, user, layer);
                ui::detail::draw_button_visual({row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f},
                                               false, false, "ACCEPT", 13, t.row_selected, t.row_active, t.accent, 1.5f);
                ui::detail::draw_button_visual({row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f},
                                               false, false, "DECLINE", 13, t.row, t.row_hover, t.text_muted, 1.5f);
            });
            return;
        }
        draw_list(layout.list, state.invites.invites, "No room invites.", [&](Rectangle row, const friend_client::room_invite& invite) {
            draw_user_row(row, invite.sender, layer);
            ui::draw_text_in_rect(invite.room_name.empty() ? "Room invite" : invite.room_name.c_str(),
                                  14,
                                  {row.x + 78.0f, row.y + 41.0f, row.width - 300.0f, 20.0f},
                                  t.text_muted,
                                  ui::text_align::left);
            ui::detail::draw_button_visual({row.x + row.width - 210.0f, row.y + 17.0f, 92.0f, 38.0f},
                                           false, false, "JOIN", 13, t.row_selected, t.row_active, t.accent, 1.5f);
            ui::detail::draw_button_visual({row.x + row.width - 106.0f, row.y + 17.0f, 92.0f, 38.0f},
                                           false, false, "DECLINE", 13, t.row, t.row_hover, t.text_muted, 1.5f);
        });
    });
}

bool title_friends_controller::is_open() const {
    return state_.open;
}

int title_friends_controller::unread_badge_count() const {
    return state_.friends.pending_request_count + state_.friends.unread_invite_count;
}

std::optional<title_friends_controller::room_join_request> title_friends_controller::consume_room_join_request() {
    std::optional<room_join_request> request = state_.pending_room_join;
    state_.pending_room_join.reset();
    return request;
}

std::optional<std::string> title_friends_controller::consume_profile_request() {
    std::optional<std::string> request = state_.pending_profile_user_id;
    state_.pending_profile_user_id.reset();
    return request;
}

void title_friends_controller::request_reload() {
    if (state_.loading) {
        return;
    }
    state_.loading = true;
    std::promise<friend_client::friend_listing_result> friends_promise;
    std::promise<friend_client::request_listing_result> requests_promise;
    std::promise<friend_client::invite_listing_result> invites_promise;
    friends_future_ = friends_promise.get_future();
    requests_future_ = requests_promise.get_future();
    invites_future_ = invites_promise.get_future();
    std::thread([friends_promise = std::move(friends_promise)]() mutable {
        friends_promise.set_value(friend_client::fetch_friends());
    }).detach();
    std::thread([requests_promise = std::move(requests_promise)]() mutable {
        requests_promise.set_value(friend_client::fetch_friend_requests());
    }).detach();
    std::thread([invites_promise = std::move(invites_promise)]() mutable {
        invites_promise.set_value(friend_client::fetch_room_invites());
    }).detach();
}

void title_friends_controller::start_accept_request(std::string request_id) {
    if (state_.operation_active) {
        return;
    }
    state_.operation_active = true;
    std::promise<friend_client::operation_result> promise;
    operation_future_ = promise.get_future();
    std::thread([promise = std::move(promise), request_id = std::move(request_id)]() mutable {
        promise.set_value(friend_client::accept_friend_request(request_id));
    }).detach();
}

void title_friends_controller::start_decline_request(std::string request_id) {
    if (state_.operation_active) {
        return;
    }
    state_.operation_active = true;
    std::promise<friend_client::operation_result> promise;
    operation_future_ = promise.get_future();
    std::thread([promise = std::move(promise), request_id = std::move(request_id)]() mutable {
        promise.set_value(friend_client::decline_friend_request(request_id));
    }).detach();
}

void title_friends_controller::start_remove_friend(std::string user_id) {
    if (state_.operation_active || user_id.empty()) {
        return;
    }
    state_.operation_active = true;
    std::promise<friend_client::operation_result> promise;
    operation_future_ = promise.get_future();
    std::thread([promise = std::move(promise), user_id = std::move(user_id)]() mutable {
        promise.set_value(friend_client::remove_friend(user_id));
    }).detach();
}

void title_friends_controller::start_block_user(std::string user_id) {
    if (state_.operation_active || user_id.empty()) {
        return;
    }
    state_.operation_active = true;
    std::promise<friend_client::operation_result> promise;
    operation_future_ = promise.get_future();
    std::thread([promise = std::move(promise), user_id = std::move(user_id)]() mutable {
        promise.set_value(friend_client::block_user(user_id));
    }).detach();
}

void title_friends_controller::start_accept_invite(std::string invite_id) {
    if (state_.operation_active) {
        return;
    }
    state_.operation_active = true;
    std::promise<friend_client::invite_operation_result> promise;
    invite_operation_future_ = promise.get_future();
    std::thread([promise = std::move(promise), invite_id = std::move(invite_id)]() mutable {
        promise.set_value(friend_client::accept_room_invite(invite_id));
    }).detach();
}

void title_friends_controller::start_decline_invite(std::string invite_id) {
    if (state_.operation_active) {
        return;
    }
    state_.operation_active = true;
    std::promise<friend_client::operation_result> promise;
    operation_future_ = promise.get_future();
    std::thread([promise = std::move(promise), invite_id = std::move(invite_id)]() mutable {
        promise.set_value(friend_client::decline_room_invite(invite_id));
    }).detach();
}

void title_friends_controller::apply_operation_result(const friend_client::operation_result& result) {
    if (!result.success) {
        ui::notify(result.message.empty() ? "Friends operation failed." : result.message, ui::notice_tone::error, 2.8f);
        return;
    }
    ui::notify("Friends updated.", ui::notice_tone::success, 1.8f);
    state_.loading = false;
    request_reload();
}
