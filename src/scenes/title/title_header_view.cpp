#include "title/title_header_view.h"

#include <algorithm>
#include <string>

#include "title/title_layout.h"
#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"
#include "ui/icons/raythm_icons.h"
#include "virtual_screen.h"

#include "rlgl.h"

namespace {

constexpr float kClosedTitleHeight = 186.0f;
constexpr float kOpenTitleHeight = 186.0f;
constexpr float kPlayTitleHeight = 162.0f;
constexpr float kSubtitleInsetX = 15.0f;
constexpr float kClosedSubtitleOffsetY = 192.0f;
constexpr float kOpenSubtitleOffsetY = 192.0f;
constexpr float kSubtitleHeight = 45.0f;
constexpr float kPlaySubtitleInsetX = 12.0f;
constexpr float kPlaySubtitleOffsetY = 153.0f;
constexpr float kPlaySubtitleHeight = 36.0f;
constexpr float kTopBarHeight = 70.0f;
constexpr float kAvatarSize = 42.0f;

void draw_top_bar_item_background(Rectangle rect, Color bg, unsigned char alpha) {
    const bool hovered = ui::is_hovered(rect);
    const bool pressed = ui::is_pressed(rect);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    if (hovered || pressed) {
        const float intensity = pressed ? 0.52f : 0.38f;
        ui::draw_rect_f(visual, with_alpha(bg, static_cast<unsigned char>(static_cast<float>(alpha) * intensity)));
    }
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

void draw_refresh_icon(Rectangle rect, Color color, unsigned char alpha) {
    raythm_icons::draw_refresh(ui::inset(rect, 17.0f), with_alpha(color, alpha), 3.0f);
}

void draw_settings_icon(Rectangle rect, Color color, unsigned char alpha) {
    raythm_icons::draw_settings_gear(ui::inset(rect, 17.0f), with_alpha(color, alpha), 3.0f);
}

void draw_friends_icon(Rectangle rect, Color color, unsigned char alpha) {
    raythm_icons::draw_friends(centered_icon_rect(rect, 14.0f), with_alpha(color, alpha), 3.0f);
}

void draw_profile_chevron(Rectangle rect, Color color, unsigned char alpha) {
    raythm_icons::draw_chevron_right(centered_icon_rect(rect, 5.0f), with_alpha(color, alpha), 3.0f);
}

std::string rating_value_text(const auth::rating_summary& rating) {
    return TextFormat("%.2f", rating.rating);
}

void draw_top_bar_controls(const title_header_view::draw_config& config) {
    const auto& t = *g_theme;
    const float bar_t = std::max(config.menu_t, config.play_t);
    const unsigned char account_alpha = static_cast<unsigned char>(255.0f * bar_t);
    const Rectangle visible = virtual_screen::visible_rect();
    const Rectangle top_bar = {visible.x, visible.y, visible.width, kTopBarHeight};
    const Color bar_color = lerp_color(t.panel, BLACK, 0.58f);
    const Color bar_text = lerp_color(t.text, WHITE, 0.76f);
    const Color bar_muted = lerp_color(t.text_muted, WHITE, 0.54f);
    ui::draw_rect_f(top_bar, with_alpha(bar_color, static_cast<unsigned char>(235.0f * bar_t)));
    ui::draw_rect_f({top_bar.x, top_bar.y + top_bar.height - 2.0f, top_bar.width, 2.0f},
                    with_alpha(t.border, static_cast<unsigned char>(150.0f * bar_t)));

    draw_top_bar_item_background(config.settings_chip_rect, t.row_hover, account_alpha);
    const Rectangle settings_visual = ui::is_pressed(config.settings_chip_rect)
        ? ui::inset(config.settings_chip_rect, 1.5f)
        : config.settings_chip_rect;
    draw_settings_icon(settings_visual,
                       ui::is_hovered(config.settings_chip_rect) ? bar_text : bar_muted,
                       account_alpha);

    draw_top_bar_item_background(config.refresh_chip_rect, t.row_hover, account_alpha);
    const Rectangle refresh_visual = ui::is_pressed(config.refresh_chip_rect)
        ? ui::inset(config.refresh_chip_rect, 1.5f)
        : config.refresh_chip_rect;
    draw_refresh_icon(refresh_visual,
                      ui::is_hovered(config.refresh_chip_rect) ? bar_text : bar_muted, account_alpha);

    draw_top_bar_item_background(config.friends_chip_rect, t.row_hover, account_alpha);
    const Rectangle friends_visual = ui::is_pressed(config.friends_chip_rect)
        ? ui::inset(config.friends_chip_rect, 1.5f)
        : config.friends_chip_rect;
    draw_friends_icon(friends_visual,
                      ui::is_hovered(config.friends_chip_rect) ? bar_text : bar_muted,
                      account_alpha);
    if (config.friends_badge_count > 0) {
        const Rectangle badge_rect = {config.friends_chip_rect.x + config.friends_chip_rect.width - 26.0f,
                                      config.friends_chip_rect.y + 12.0f,
                                      18.0f,
                                      18.0f};
        ui::draw_rect_f(badge_rect, with_alpha(t.error, account_alpha));
        ui::draw_text_in_rect(std::to_string(std::min(config.friends_badge_count, 9)).c_str(),
                              12,
                              badge_rect,
                              with_alpha(WHITE, account_alpha),
                              ui::text_align::center);
    }

    draw_top_bar_item_background(config.account_chip_rect, t.row_hover, account_alpha);
    const Rectangle avatar_rect = {config.account_chip_rect.x + 16.0f,
                                   config.account_chip_rect.y + 14.0f,
                                   kAvatarSize, kAvatarSize};
    avatar_texture_cache::draw_avatar(
        avatar_rect,
        std::string(config.avatar_url),
        std::string(config.avatar_label),
        with_alpha(config.logged_in ? t.accent : t.row_selected, account_alpha),
        with_alpha(config.logged_in ? t.panel : bar_text, account_alpha),
        14,
        std::string(config.avatar_base_url));
    const Rectangle account_name_rect = {
        avatar_rect.x + avatar_rect.width + 14.0f, config.account_chip_rect.y + 8.0f,
        config.account_chip_rect.width - 168.0f, 30.0f
    };
    ui::draw_text_in_rect(std::string(config.account_name).c_str(), 20, account_name_rect,
                          with_alpha(bar_text, account_alpha), ui::text_align::left);
    if (config.logged_in) {
        const Rectangle rating_label_rect = {
            config.account_chip_rect.x + config.account_chip_rect.width - 96.0f,
            config.account_chip_rect.y + 9.0f,
            58.0f,
            18.0f
        };
        const Rectangle rating_value_rect = {
            config.account_chip_rect.x + config.account_chip_rect.width - 114.0f,
            config.account_chip_rect.y + 28.0f,
            76.0f,
            26.0f
        };
        ui::draw_text_in_rect("RATING", 10, rating_label_rect,
                              with_alpha(t.accent, account_alpha), ui::text_align::right);
        const std::string rating_value = rating_value_text(config.rating);
        ui::draw_text_in_rect(rating_value.c_str(), 18, rating_value_rect,
                              with_alpha(bar_text, account_alpha), ui::text_align::right);
        ui::draw_text_in_rect(config.email_verified ? "Season 0" : "Verify email",
                              12,
                              {avatar_rect.x + avatar_rect.width + 14.0f, config.account_chip_rect.y + 41.0f,
                               config.account_chip_rect.width - 160.0f, 20.0f},
                              with_alpha(config.email_verified ? bar_muted : t.error, account_alpha),
                              ui::text_align::left);
    } else {
        ui::draw_text_in_rect(std::string(config.account_status).c_str(),
                              13,
                              {avatar_rect.x + avatar_rect.width + 14.0f, config.account_chip_rect.y + 41.0f,
                               config.account_chip_rect.width - 104.0f, 20.0f},
                              with_alpha(bar_muted, account_alpha),
                              ui::text_align::left);
    }
    draw_profile_chevron({config.account_chip_rect.x + config.account_chip_rect.width - 28.0f,
                          config.account_chip_rect.y, 28.0f, config.account_chip_rect.height},
                         bar_muted, account_alpha);
}

}  // namespace

namespace title_header_view {

void draw(const draw_config& config) {
    const auto& t = *g_theme;

    const Vector2 closed_title_pos = ui::display_text_position("raythm", 124,
                                                               {config.closed_header_rect.x, config.closed_header_rect.y,
                                                                config.closed_header_rect.width, kClosedTitleHeight},
                                                               ui::text_align::center);
    const Rectangle title_play_header_rect = title_layout::play_header_rect();
    const Vector2 home_title_pos = ui::display_text_position("raythm", 124,
                                                             {config.open_header_rect.x, config.open_header_rect.y,
                                                              config.open_header_rect.width, kOpenTitleHeight},
                                                             ui::text_align::left);
    const Vector2 play_title_pos = ui::display_text_position("raythm", 108,
                                                             {title_play_header_rect.x, title_play_header_rect.y,
                                                              title_play_header_rect.width, kPlayTitleHeight},
                                                             ui::text_align::left);
    const Vector2 title_pos =
        tween::lerp(tween::lerp(closed_title_pos, home_title_pos, config.menu_t), play_title_pos, config.play_t);

    const Vector2 closed_subtitle_pos = ui::display_text_position("trace the line before the beat disappears", 30,
                                                                  {config.closed_header_rect.x + kSubtitleInsetX,
                                                                   config.closed_header_rect.y + kClosedSubtitleOffsetY,
                                                                   config.closed_header_rect.width - kSubtitleInsetX,
                                                                   kSubtitleHeight},
                                                                  ui::text_align::center);
    const Vector2 home_subtitle_pos = ui::display_text_position("trace the line before the beat disappears", 30,
                                                                {config.open_header_rect.x + kSubtitleInsetX,
                                                                 config.open_header_rect.y + kOpenSubtitleOffsetY,
                                                                 config.open_header_rect.width - kSubtitleInsetX,
                                                                 kSubtitleHeight},
                                                                ui::text_align::left);
    const Vector2 play_subtitle_pos = ui::display_text_position("trace the line before the beat disappears", 24,
                                                                {title_play_header_rect.x + kPlaySubtitleInsetX,
                                                                 title_play_header_rect.y + kPlaySubtitleOffsetY,
                                                                 title_play_header_rect.width - kPlaySubtitleInsetX,
                                                                 kPlaySubtitleHeight},
                                                                ui::text_align::left);
    const Vector2 subtitle_pos =
        tween::lerp(tween::lerp(closed_subtitle_pos, home_subtitle_pos, config.menu_t), play_subtitle_pos, config.play_t);
    const float title_font_size = std::lerp(124.0f, 108.0f, config.play_t);
    const float subtitle_font_size = std::lerp(30.0f, 24.0f, config.play_t);

    const float title_visibility = std::clamp(1.0f - config.play_t * 1.35f, 0.0f, 1.0f);
    const Color title_color = with_alpha(t.text, static_cast<unsigned char>(255.0f * title_visibility));
    const Color subtitle_color = with_alpha(t.text_dim, static_cast<unsigned char>(255.0f * title_visibility));
    if (title_visibility > 0.01f) {
        ui::draw_text_display("raythm", title_pos, title_font_size, 0.0f, title_color);
        ui::draw_text_display("trace the line before the beat disappears",
                              subtitle_pos, subtitle_font_size, 0.0f, subtitle_color);
    }

}

void draw_screen_overlay(const draw_config& config) {
    if (config.menu_t <= 0.01f) {
        return;
    }

    const float scale = std::max(0.001f, virtual_screen::design_to_screen_scale());
    const Rectangle visible = virtual_screen::visible_rect();
    const float offset_x = -visible.x * scale;
    const float offset_y = static_cast<float>(virtual_screen::top_reserved_pixels()) - visible.y * scale;

    rlPushMatrix();
    rlTranslatef(offset_x, offset_y, 0.0f);
    rlScalef(scale, scale, 1.0f);
    draw_top_bar_controls(config);
    rlPopMatrix();
}

}  // namespace title_header_view
