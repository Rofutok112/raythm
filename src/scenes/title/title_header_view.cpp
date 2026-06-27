#include "title/title_header_view.h"

#include <algorithm>
#include <string>

#include "localization/localization.h"
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
constexpr float kRatingPanelWidth = 176.0f;
constexpr const char* kTitleText = "raythm";
constexpr const char* kSubtitleText = "trace the line before the beat disappears";

struct rating_panel_layout {
    Rectangle label{};
    Rectangle value{};
    Rectangle subtitle{};
};

struct account_chip_layout {
    Rectangle avatar{};
    Rectangle name{};
    Rectangle logged_in_status{};
    Rectangle logged_out_status{};
    Rectangle rating_panel{};
    Rectangle chevron{};
};

struct top_bar_chip_layout {
    Rectangle badge{};
};

struct header_copy_keyframe {
    Rectangle title{};
    Rectangle subtitle{};
    float title_font_size = 124.0f;
    float subtitle_font_size = 30.0f;
    ui::text_align align = ui::text_align::left;
};

struct header_copy_layout {
    Vector2 title_pos{};
    Vector2 subtitle_pos{};
    float title_font_size = 124.0f;
    float subtitle_font_size = 30.0f;
};

constexpr rating_panel_layout rating_panel_layout_for(Rectangle rect) {
    return {
        .label = {rect.x + 18.0f, rect.y + 5.0f, rect.width - 30.0f, 14.0f},
        .value = {rect.x + 18.0f, rect.y + 18.0f, rect.width - 30.0f, 24.0f},
        .subtitle = {rect.x + 18.0f, rect.y + 40.0f, rect.width - 30.0f, 14.0f},
    };
}

constexpr account_chip_layout account_chip_layout_for(Rectangle chip) {
    const Rectangle avatar = {chip.x + 16.0f, chip.y + 14.0f, kAvatarSize, kAvatarSize};
    const float text_x = avatar.x + avatar.width + 14.0f;
    return {
        .avatar = avatar,
        .name = {text_x, chip.y + 8.0f, chip.width - kRatingPanelWidth - 96.0f, 30.0f},
        .logged_in_status = {text_x, chip.y + 41.0f, chip.width - kRatingPanelWidth - 96.0f, 20.0f},
        .logged_out_status = {text_x, chip.y + 41.0f, chip.width - 104.0f, 20.0f},
        .rating_panel = {chip.x + chip.width - kRatingPanelWidth - 34.0f, chip.y + 8.0f,
                         kRatingPanelWidth, 54.0f},
        .chevron = {chip.x + chip.width - 28.0f, chip.y, 28.0f, chip.height},
    };
}

constexpr top_bar_chip_layout friends_chip_layout_for(Rectangle chip) {
    return {
        .badge = {chip.x + chip.width - 26.0f, chip.y + 12.0f, 18.0f, 18.0f},
    };
}

constexpr Rectangle top_bar_rect_for_visible(Rectangle visible) {
    return {visible.x, visible.y, visible.width, kTopBarHeight};
}

constexpr header_copy_keyframe closed_header_copy_keyframe(Rectangle header) {
    return {
        .title = {header.x, header.y, header.width, kClosedTitleHeight},
        .subtitle = {
            header.x + kSubtitleInsetX,
            header.y + kClosedSubtitleOffsetY,
            header.width - kSubtitleInsetX,
            kSubtitleHeight,
        },
        .title_font_size = 124.0f,
        .subtitle_font_size = 30.0f,
        .align = ui::text_align::center,
    };
}

constexpr header_copy_keyframe home_header_copy_keyframe(Rectangle header) {
    return {
        .title = {header.x, header.y, header.width, kOpenTitleHeight},
        .subtitle = {
            header.x + kSubtitleInsetX,
            header.y + kOpenSubtitleOffsetY,
            header.width - kSubtitleInsetX,
            kSubtitleHeight,
        },
        .title_font_size = 124.0f,
        .subtitle_font_size = 30.0f,
        .align = ui::text_align::left,
    };
}

constexpr header_copy_keyframe play_header_copy_keyframe(Rectangle header) {
    return {
        .title = {header.x, header.y, header.width, kPlayTitleHeight},
        .subtitle = {
            header.x + kPlaySubtitleInsetX,
            header.y + kPlaySubtitleOffsetY,
            header.width - kPlaySubtitleInsetX,
            kPlaySubtitleHeight,
        },
        .title_font_size = 108.0f,
        .subtitle_font_size = 24.0f,
        .align = ui::text_align::left,
    };
}

Vector2 title_text_position(const header_copy_keyframe& keyframe) {
    return ui::display_text_position(kTitleText, keyframe.title_font_size,
                                     keyframe.title, keyframe.align);
}

Vector2 subtitle_text_position(const header_copy_keyframe& keyframe) {
    return ui::display_text_position(kSubtitleText, keyframe.subtitle_font_size,
                                     keyframe.subtitle, keyframe.align);
}

header_copy_layout header_copy_layout_for(const title_header_view::draw_config& config) {
    const header_copy_keyframe closed = closed_header_copy_keyframe(config.closed_header_rect);
    const header_copy_keyframe home = home_header_copy_keyframe(config.open_header_rect);
    const header_copy_keyframe play = play_header_copy_keyframe(title_layout::play_header_rect());
    return {
        .title_pos = tween::lerp(tween::lerp(title_text_position(closed),
                                             title_text_position(home),
                                             config.menu_t),
                                 title_text_position(play),
                                 config.play_t),
        .subtitle_pos = tween::lerp(tween::lerp(subtitle_text_position(closed),
                                                subtitle_text_position(home),
                                                config.menu_t),
                                    subtitle_text_position(play),
                                    config.play_t),
        .title_font_size = std::lerp(home.title_font_size, play.title_font_size, config.play_t),
        .subtitle_font_size = std::lerp(home.subtitle_font_size, play.subtitle_font_size, config.play_t),
    };
}

void draw_top_bar_item_background(Rectangle rect, Color bg, unsigned char alpha) {
    const bool hovered = ui::is_hovered(rect);
    const bool pressed = ui::is_pressed(rect);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    if (hovered || pressed) {
        const float intensity = pressed ? 0.52f : 0.38f;
        ui::surface_fill(visual, with_alpha(bg, static_cast<unsigned char>(static_cast<float>(alpha) * intensity)));
    }
}

void draw_top_bar_icon_button(Rectangle rect,
                              ui::icon_draw_fn draw_icon,
                              float icon_inset,
                              Color icon_color,
                              Color icon_hover_color,
                              unsigned char alpha) {
    const auto& t = *g_theme;
    draw_top_bar_item_background(rect, t.row_hover, alpha);
    ui::icon_button(rect, draw_icon, {
        .border_width = 0.0f,
        .bg = {0, 0, 0, 1},
        .bg_hover = {0, 0, 0, 1},
        .icon_color = with_alpha(icon_color, alpha),
        .icon_hover_color = with_alpha(icon_hover_color, alpha),
        .border_color = {0, 0, 0, 1},
        .icon_inset = icon_inset,
        .icon_stroke_width = 3.0f,
        .border_alpha_tracks_fill = false,
    });
}

void draw_profile_chevron(Rectangle rect, Color color, unsigned char alpha) {
    raythm_icons::draw_chevron_right(ui::icon_rect(rect, 5.0f), with_alpha(color, alpha), 3.0f);
}

std::string rating_value_text(const auth::rating_summary& rating) {
    return TextFormat("%.0f", rating.rating);
}

std::string rating_subtitle_text(const auth::rating_summary& rating) {
    if (rating.rank > 0) {
        return TextFormat("%s #%d", localization::tr_literal("GLOBAL"), rating.rank);
    }
    if (rating.best_play_count > 0) {
        if (localization::current_locale() == localization::locale::japanese) {
            return TextFormat("%s %d", localization::tr_literal("BEST PLAYS"), rating.best_play_count);
        }
        return TextFormat("BEST %d PLAYS", rating.best_play_count);
    }
    return localization::tr_literal("UNRATED");
}

void draw_rating_panel(Rectangle rect,
                       const auth::rating_summary& rating,
                       unsigned char alpha,
                       Color bar_text,
                       Color bar_muted) {
    const auto& t = *g_theme;
    const Color tone = t.accent;
    const float alpha_t = static_cast<float>(alpha) / 255.0f;
    const Color fill = with_alpha(lerp_color(t.panel, tone, 0.10f),
                                  static_cast<unsigned char>(210.0f * alpha_t));
    const Color border = with_alpha(lerp_color(t.border, tone, 0.42f),
                                    static_cast<unsigned char>(190.0f * alpha_t));
    const Color label_color = with_alpha(lerp_color(bar_text, tone, 0.26f), alpha);
    const Color value_color = with_alpha(lerp_color(bar_text, WHITE, 0.55f), alpha);

    ui::surface_fill(rect, fill);
    ui::accent_bar({rect.x, rect.y, 4.0f, rect.height}, with_alpha(tone, alpha));
    ui::divider({rect.x + 4.0f, rect.y, rect.width - 4.0f, 1.0f},
                with_alpha(WHITE, static_cast<unsigned char>(26.0f * alpha_t)));
    ui::frame(rect, border, 1.0f);

    const rating_panel_layout layout = rating_panel_layout_for(rect);
    ui::draw_text_in_rect(localization::tr_literal("RATING"), 11, layout.label, label_color, ui::text_align::left);
    const std::string rating_value = rating_value_text(rating);
    ui::draw_text_in_rect(rating_value.c_str(), 23, layout.value, value_color, ui::text_align::left);
    const std::string subtitle = rating_subtitle_text(rating);
    ui::draw_text_in_rect(subtitle.c_str(), 10, layout.subtitle,
                          with_alpha(bar_muted, static_cast<unsigned char>((static_cast<int>(alpha) * 230) / 255)),
                          ui::text_align::left);
}

void draw_top_bar_controls(const title_header_view::draw_config& config) {
    const auto& t = *g_theme;
    const float bar_t = std::max(config.menu_t, config.play_t);
    const unsigned char account_alpha = static_cast<unsigned char>(255.0f * bar_t);
    const Rectangle visible = virtual_screen::visible_rect();
    const Rectangle top_bar = top_bar_rect_for_visible(visible);
    const Color bar_color = lerp_color(t.panel, BLACK, 0.58f);
    const Color bar_text = lerp_color(t.text, WHITE, 0.76f);
    const Color bar_muted = lerp_color(t.text_muted, WHITE, 0.54f);
    ui::bar_surface(top_bar,
                    with_alpha(bar_color, static_cast<unsigned char>(235.0f * bar_t)),
                    with_alpha(t.border, static_cast<unsigned char>(150.0f * bar_t)),
                    2.0f);

    draw_top_bar_icon_button(config.settings_chip_rect, raythm_icons::draw_settings_gear,
                             17.0f, bar_muted, bar_text, account_alpha);
    draw_top_bar_icon_button(config.refresh_chip_rect, raythm_icons::draw_refresh,
                             17.0f, bar_muted, bar_text, account_alpha);
    draw_top_bar_icon_button(config.rating_rankings_chip_rect, raythm_icons::draw_trophy,
                             17.0f, bar_muted, bar_text, account_alpha);
    draw_top_bar_icon_button(config.friends_chip_rect, raythm_icons::draw_friends,
                             17.0f, bar_muted, bar_text, account_alpha);
    if (config.friends_badge_count > 0) {
        const top_bar_chip_layout friends_layout = friends_chip_layout_for(config.friends_chip_rect);
        ui::surface_fill(friends_layout.badge, with_alpha(t.error, account_alpha));
        ui::draw_text_in_rect(std::to_string(std::min(config.friends_badge_count, 9)).c_str(),
                              12,
                              friends_layout.badge,
                              with_alpha(WHITE, account_alpha),
                              ui::text_align::center);
    }

    draw_top_bar_item_background(config.account_chip_rect, t.row_hover, account_alpha);
    const account_chip_layout account_layout = account_chip_layout_for(config.account_chip_rect);
    avatar_texture_cache::draw_avatar(
        account_layout.avatar,
        std::string(config.avatar_url),
        std::string(config.avatar_label),
        with_alpha(config.logged_in ? t.accent : t.row_selected, account_alpha),
        with_alpha(config.logged_in ? t.panel : bar_text, account_alpha),
        14,
        std::string(config.avatar_base_url));
    ui::draw_text_in_rect(std::string(config.account_name).c_str(), 20, account_layout.name,
                          with_alpha(bar_text, account_alpha), ui::text_align::left);
    if (config.logged_in) {
        draw_rating_panel(account_layout.rating_panel, config.rating, account_alpha, bar_text, bar_muted);
        ui::draw_text_in_rect(config.email_verified ? "Season 0" : "Verify email",
                              12,
                              account_layout.logged_in_status,
                              with_alpha(config.email_verified ? bar_muted : t.error, account_alpha),
                              ui::text_align::left);
    } else {
        ui::draw_text_in_rect(std::string(config.account_status).c_str(),
                              13,
                              account_layout.logged_out_status,
                              with_alpha(bar_muted, account_alpha),
                              ui::text_align::left);
    }
    draw_profile_chevron(account_layout.chevron, bar_muted, account_alpha);
}

}  // namespace

namespace title_header_view {

void draw(const draw_config& config) {
    const auto& t = *g_theme;

    const header_copy_layout copy = header_copy_layout_for(config);

    const float title_visibility = std::clamp(1.0f - config.play_t * 1.35f, 0.0f, 1.0f);
    const Color title_color = with_alpha(t.text, static_cast<unsigned char>(255.0f * title_visibility));
    const Color subtitle_color = with_alpha(t.text_dim, static_cast<unsigned char>(255.0f * title_visibility));
    if (title_visibility > 0.01f) {
        ui::draw_text_display(kTitleText, copy.title_pos, copy.title_font_size, 0.0f, title_color);
        ui::draw_text_display(kSubtitleText,
                              copy.subtitle_pos, copy.subtitle_font_size, 0.0f, subtitle_color);
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
