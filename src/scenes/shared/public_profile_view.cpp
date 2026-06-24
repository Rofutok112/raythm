#include "shared/public_profile_view.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "shared/avatar_texture_cache.h"
#include "shared/public_profile_state.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace public_profile_view {
namespace {

constexpr Rectangle kModalRect{610.0f, 280.0f, 700.0f, 430.0f};
constexpr float kAvatarSize = 96.0f;

struct modal_layout {
    Rectangle modal_rect{};
    Rectangle close_button_rect{};
    Rectangle relationship_button_rect{};
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

modal_layout make_layout(float open_anim, ui::draw_layer layer = ui::draw_layer::modal) {
    const Rectangle modal = animated_bounds(open_anim);
    return {
        .modal_rect = modal,
        .close_button_rect = {modal.x + modal.width - 122.0f, modal.y + 24.0f, 92.0f, 42.0f},
        .relationship_button_rect = {modal.x + modal.width - 232.0f, modal.y + 24.0f, 96.0f, 42.0f},
        .layer = layer,
    };
}

void draw_profile_body(const model& state, Rectangle modal) {
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
    const modal_layout layout = make_layout(state.open_anim);
    const public_profile_state::relationship_action_view relationship_action =
        public_profile_state::relationship_action_for(profile, state.relationship_operation_active);
    const Rectangle avatar_rect{content.x, content.y + 8.0f, kAvatarSize, kAvatarSize};
    avatar_texture_cache::draw_avatar(
        avatar_rect,
        profile.avatar_url,
        avatar_label(profile),
        t.row_soft_selected,
        t.text,
        28,
        state.avatar_base_url);
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
    const Rectangle rating_panel = {content.x + 124.0f, content.y + 94.0f, 260.0f, 36.0f};
    ui::draw_rect_f(rating_panel, with_alpha(t.row_soft, 226));
    ui::draw_rect_lines(rating_panel, 1.0f, t.border_light);
    ui::draw_text_in_rect("SEASON 0 RATING", 11,
                          {rating_panel.x + 12.0f, rating_panel.y + 5.0f, 128.0f, 14.0f},
                          t.text_muted,
                          ui::text_align::left);
    const std::string rating_text = TextFormat("%.2f", profile.rating.rating);
    ui::draw_text_in_rect(rating_text.c_str(), 19,
                          {rating_panel.x + 12.0f, rating_panel.y + 14.0f, 112.0f, 20.0f},
                          t.accent,
                          ui::text_align::left);
    const std::string plays_text = TextFormat("%d plays", profile.rating.eligible_play_count);
    ui::draw_text_in_rect(plays_text.c_str(), 12,
                          {rating_panel.x + 142.0f, rating_panel.y + 13.0f, 104.0f, 18.0f},
                          t.text_muted,
                          ui::text_align::right);
    ui::detail::draw_button_visual(layout.relationship_button_rect,
                                   relationship_action.enabled &&
                                       ui::is_hovered(layout.relationship_button_rect, layout.layer),
                                   relationship_action.enabled &&
                                       ui::is_pressed(layout.relationship_button_rect, layout.layer),
                                   relationship_action.label,
                                   13,
                                   relationship_action.enabled ? t.row_selected : t.row,
                                   relationship_action.enabled ? t.row_active : t.row_hover,
                                   relationship_action.enabled ? t.accent : t.text_muted,
                                   1.5f);

    const Rectangle links_area{content.x, content.y + 152.0f, content.width, content.height - 152.0f};
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

Rectangle bounds(float open_anim) {
    return make_layout(open_anim).modal_rect;
}

command handle_input(const model& state, ui::draw_layer layer) {
    const modal_layout layout = make_layout(state.open_anim, layer);
    if (ui::is_clicked(layout.close_button_rect, layout.layer)) {
        return {.type = command_type::close};
    }
    if (state.result.success &&
        state.result.profile.has_value() &&
        public_profile_state::relationship_action_for(
            *state.result.profile,
            state.relationship_operation_active).enabled &&
        ui::is_clicked(layout.relationship_button_rect, layout.layer)) {
        return {.type = command_type::start_relationship_action};
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), layout.modal_rect)) {
        return {.type = command_type::close};
    }
    return {};
}

void draw(const model& state, ui::draw_layer layer, bool draw_backdrop) {
    ui::enqueue_draw_command(layer, [state, layer, draw_backdrop]() {
        if (draw_backdrop) {
            ui::draw_fullscreen_overlay(with_alpha(BLACK, 132));
        }
        const modal_layout layout = make_layout(state.open_anim, layer);
        ui::draw_panel(layout.modal_rect);
        draw_close_button(layout.close_button_rect, layout.layer);
        draw_profile_body(state, layout.modal_rect);
    });
}

}  // namespace public_profile_view
