#include "shared/public_profile_controller.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
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

namespace public_profile {
namespace {

constexpr Rectangle kModalRect{610.0f, 280.0f, 700.0f, 430.0f};
constexpr float kAvatarSize = 96.0f;

struct modal_layout {
    Rectangle modal_rect{};
    Rectangle close_button_rect{};
    ui::draw_layer layer = ui::draw_layer::modal;
};

std::string avatar_label(const auth::public_profile& profile) {
    const std::string& source = profile.display_name.empty() ? profile.id : profile.display_name;
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

modal_layout make_layout(const state& state, ui::draw_layer layer = ui::draw_layer::modal) {
    const Rectangle modal = animated_bounds(state.open_anim);
    return {
        .modal_rect = modal,
        .close_button_rect = {modal.x + modal.width - 122.0f, modal.y + 24.0f, 92.0f, 42.0f},
        .layer = layer,
    };
}

void draw_profile_body(const state& state, Rectangle modal) {
    const auto& t = *g_theme;
    const Rectangle content = ui::inset(modal, ui::edge_insets::uniform(30.0f));
    if (state.loading && !state.loaded_once) {
        ui::draw_text_in_rect("Loading profile...", 20, content, t.text_muted, ui::text_align::center);
        return;
    }
    if (!state.result.success || !state.result.profile.has_value()) {
        const std::string message = state.result.message.empty() ? "Could not load profile." : state.result.message;
        ui::draw_text_in_rect(message.c_str(), 20, content, t.text_muted, ui::text_align::center);
        return;
    }

    const auth::public_profile& profile = *state.result.profile;
    const Rectangle avatar_rect{content.x, content.y + 8.0f, kAvatarSize, kAvatarSize};
    avatar_texture_cache::draw_avatar(
        avatar_rect,
        profile.avatar_url,
        avatar_label(profile),
        t.row_soft_selected,
        t.text,
        28,
        auth::load_session_summary().server_url);
    ui::draw_rect_lines(avatar_rect, 1.4f, t.border_light);

    ui::draw_text_in_rect(profile.display_name.empty() ? "Unknown Player" : profile.display_name.c_str(),
                          30,
                          {content.x + 124.0f, content.y + 18.0f, content.width - 124.0f, 42.0f},
                          t.text,
                          ui::text_align::left);
    ui::draw_text_in_rect("PROFILE", 13,
                          {content.x + 124.0f, content.y + 66.0f, content.width - 124.0f, 22.0f},
                          t.accent,
                          ui::text_align::left);

    const Rectangle links_area{content.x, content.y + 142.0f, content.width, content.height - 142.0f};
    if (profile.external_links.empty()) {
        ui::draw_text_in_rect("No public profile links.", 18, links_area, t.text_muted, ui::text_align::left);
        return;
    }

    float y = links_area.y;
    for (const auth::external_link& link : profile.external_links) {
        const Rectangle row{links_area.x, y, links_area.width, 46.0f};
        ui::draw_rect_f(row, t.row);
        ui::draw_rect_lines(row, 1.0f, t.border_light);
        const std::string label = link.label.empty() ? "Link" : link.label;
        ui::draw_text_in_rect(label.c_str(), 17,
                              {row.x + 16.0f, row.y + 5.0f, row.width - 32.0f, 22.0f},
                              t.text,
                              ui::text_align::left);
        ui::draw_text_in_rect(link.url.c_str(), 13,
                              {row.x + 16.0f, row.y + 25.0f, row.width - 32.0f, 18.0f},
                              t.text_muted,
                              ui::text_align::left);
        y += 54.0f;
        if (y + 46.0f > links_area.y + links_area.height) {
            break;
        }
    }
}

void draw_close_button(Rectangle rect, ui::draw_layer layer) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(rect, layer);
    const bool pressed = ui::is_pressed(rect, layer);
    ui::detail::draw_button_visual(rect, hovered, pressed, "CLOSE", 14,
                                   t.row_soft,
                                   t.row_soft_hover,
                                   t.text_muted,
                                   2.0f);
}

}  // namespace

void controller::reset() {
    state_ = {};
}

void controller::open(std::string user_id) {
    if (user_id.empty()) {
        return;
    }
    if (state_.open && state_.requested_user_id == user_id && !state_.closing) {
        return;
    }
    state_.open = true;
    state_.closing = false;
    state_.suppress_background_close_until_release = true;
    state_.requested_user_id = std::move(user_id);
    state_.result = {};
    state_.loaded_once = false;
    request_load();
}

void controller::close() {
    if (state_.open) {
        state_.closing = true;
    }
}

void controller::tick(float dt) {
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

void controller::poll() {
    if (!state_.loading ||
        load_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    try {
        state_.result = load_future_.get();
    } catch (const std::exception& ex) {
        state_.result = {.success = false, .message = ex.what()};
    } catch (...) {
        state_.result = {.success = false, .message = "Failed to load profile."};
    }
    state_.loading = false;
    state_.loaded_once = true;
    if (!state_.result.success) {
        ui::notify(state_.result.message, ui::notice_tone::error, 2.8f);
    }
}

bool controller::handle_input() {
    if (!state_.open) {
        return false;
    }
    if (state_.suppress_background_close_until_release) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            return true;
        }
        state_.suppress_background_close_until_release = false;
    }
    if ((IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) && !state_.loading) {
        close();
        return true;
    }
    const modal_layout layout = make_layout(state_);
    if (ui::is_clicked(layout.close_button_rect, layout.layer)) {
        close();
        return true;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), layout.modal_rect)) {
        close();
        return true;
    }
    return true;
}

void controller::draw(ui::draw_layer layer) {
    if (!state_.open) {
        return;
    }
    ui::enqueue_draw_command(layer, [state = state_, layer]() {
        const auto& t = *g_theme;
        ui::draw_fullscreen_overlay(with_alpha(BLACK, 132));
        const modal_layout layout = make_layout(state, layer);
        ui::draw_panel(layout.modal_rect);
        draw_close_button(layout.close_button_rect, layout.layer);
        draw_profile_body(state, layout.modal_rect);
    });
}

bool controller::is_open() const {
    return state_.open;
}

Rectangle controller::bounds() const {
    return make_layout(state_).modal_rect;
}

void controller::request_load() {
    if (state_.loading) {
        return;
    }
    state_.loading = true;
    std::promise<auth::public_profile_result> promise;
    load_future_ = promise.get_future();
    std::thread([promise = std::move(promise), user_id = state_.requested_user_id]() mutable {
        try {
            promise.set_value(auth::fetch_public_profile(user_id));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

}  // namespace public_profile
