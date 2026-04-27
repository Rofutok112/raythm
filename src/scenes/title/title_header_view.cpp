#include "title/title_header_view.h"

#include <algorithm>
#include <cmath>

#include "title/title_layout.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"

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
constexpr float kAccountBorderWidth = 3.0f;
constexpr float kAvatarOffsetX = 18.0f;
constexpr float kAvatarOffsetY = 13.5f;
constexpr float kAvatarSize = 60.0f;
constexpr float kAvatarRadius = 30.0f;
constexpr float kAccountTextOffsetX = 96.0f;
constexpr float kAccountTextRightReserved = 132.0f;
constexpr float kAccountNameOffsetY = 12.0f;
constexpr float kAccountNameHeight = 33.0f;
constexpr float kAccountStatusOffsetY = 45.0f;
constexpr float kAccountStatusHeight = 24.0f;
constexpr float kAccountArrowRightInset = 36.0f;
constexpr float kAccountArrowOffsetY = 18.0f;
constexpr float kAccountArrowWidth = 18.0f;
constexpr float kAccountArrowHeight = 36.0f;
constexpr float kSettingsBorderWidth = 3.0f;
constexpr float kRefreshIconRadius = 20.0f;
constexpr int kRefreshIconSegments = 24;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kRefreshHeadRotation = 75.0f * kPi / 180.0f;
constexpr int kSettingsGearTeeth = 8;

void draw_chip_background(Rectangle rect, Color bg, Color border, unsigned char alpha) {
    const bool hovered = ui::is_hovered(rect);
    const bool pressed = ui::is_pressed(rect);
    const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
    ui::draw_rect_f(visual, with_alpha(hovered ? bg : g_theme->panel, alpha));
    ui::draw_rect_lines(visual, kSettingsBorderWidth, with_alpha(border, alpha));
}

void draw_refresh_icon(Rectangle rect, Color color, unsigned char alpha) {
    const Vector2 center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
    const Color icon = with_alpha(color, alpha);
    const float start = -0.30f * kPi;
    const float end = 1.42f * kPi;
    Vector2 previous = {
        center.x + std::cos(start) * kRefreshIconRadius,
        center.y + std::sin(start) * kRefreshIconRadius
    };

    for (int i = 1; i <= kRefreshIconSegments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRefreshIconSegments);
        const float angle = start + (end - start) * t;
        const Vector2 current = {
            center.x + std::cos(angle) * kRefreshIconRadius,
            center.y + std::sin(angle) * kRefreshIconRadius
        };
        DrawLineEx(previous, current, 3.0f, icon);
        previous = current;
    }

    const Vector2 tip = {
        center.x + std::cos(end) * kRefreshIconRadius,
        center.y + std::sin(end) * kRefreshIconRadius
    };
    constexpr float kHeadLength = 13.0f;
    constexpr float kHeadSpread = 0.62f;
    const float head_angle = end + kPi + kRefreshHeadRotation;
    const Vector2 wing_a = {
        tip.x + std::cos(head_angle + kHeadSpread) * kHeadLength,
        tip.y + std::sin(head_angle + kHeadSpread) * kHeadLength
    };
    const Vector2 wing_b = {
        tip.x + std::cos(head_angle - kHeadSpread) * kHeadLength,
        tip.y + std::sin(head_angle - kHeadSpread) * kHeadLength
    };
    DrawLineEx(tip, wing_a, 3.0f, icon);
    DrawLineEx(tip, wing_b, 3.0f, icon);
}

void draw_settings_icon(Rectangle rect, Color color, unsigned char alpha) {
    const Vector2 center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
    const Color icon = with_alpha(color, alpha);
    constexpr float ring_radius = 16.0f;
    constexpr float tooth_inner_radius = 15.0f;
    constexpr float tooth_outer_radius = 27.0f;
    constexpr float tooth_width = 10.0f;

    DrawRing(center, ring_radius - 8.0f, ring_radius + 4.0f, 0.0f, 360.0f, 48, icon);

    for (int i = 0; i < kSettingsGearTeeth; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(kSettingsGearTeeth)) * kPi * 2.0f;
        const Vector2 a = {
            center.x + std::cos(angle) * tooth_inner_radius,
            center.y + std::sin(angle) * tooth_inner_radius
        };
        const Vector2 b = {
            center.x + std::cos(angle) * tooth_outer_radius,
            center.y + std::sin(angle) * tooth_outer_radius
        };
        DrawLineEx(a, b, tooth_width, icon);
    }
}

}  // namespace

namespace title_header_view {

void draw(const draw_config& config) {
    const auto& t = *g_theme;

    const Vector2 closed_title_pos = ui::text_position("raythm", 124,
                                                       {config.closed_header_rect.x, config.closed_header_rect.y,
                                                        config.closed_header_rect.width, kClosedTitleHeight},
                                                       ui::text_align::center);
    const Rectangle title_play_header_rect = title_layout::play_header_rect();
    const Vector2 home_title_pos = ui::text_position("raythm", 124,
                                                     {config.open_header_rect.x, config.open_header_rect.y,
                                                      config.open_header_rect.width, kOpenTitleHeight},
                                                     ui::text_align::left);
    const Vector2 play_title_pos = ui::text_position("raythm", 108,
                                                     {title_play_header_rect.x, title_play_header_rect.y,
                                                      title_play_header_rect.width, kPlayTitleHeight},
                                                     ui::text_align::left);
    const Vector2 title_pos =
        tween::lerp(tween::lerp(closed_title_pos, home_title_pos, config.menu_t), play_title_pos, config.play_t);

    const Vector2 closed_subtitle_pos = ui::text_position("trace the line before the beat disappears", 30,
                                                          {config.closed_header_rect.x + kSubtitleInsetX,
                                                           config.closed_header_rect.y + kClosedSubtitleOffsetY,
                                                           config.closed_header_rect.width - kSubtitleInsetX,
                                                           kSubtitleHeight},
                                                          ui::text_align::center);
    const Vector2 home_subtitle_pos = ui::text_position("trace the line before the beat disappears", 30,
                                                        {config.open_header_rect.x + kSubtitleInsetX,
                                                         config.open_header_rect.y + kOpenSubtitleOffsetY,
                                                         config.open_header_rect.width - kSubtitleInsetX,
                                                         kSubtitleHeight},
                                                        ui::text_align::left);
    const Vector2 play_subtitle_pos = ui::text_position("trace the line before the beat disappears", 24,
                                                        {title_play_header_rect.x + kPlaySubtitleInsetX,
                                                         title_play_header_rect.y + kPlaySubtitleOffsetY,
                                                         title_play_header_rect.width - kPlaySubtitleInsetX,
                                                         kPlaySubtitleHeight},
                                                        ui::text_align::left);
    const Vector2 subtitle_pos =
        tween::lerp(tween::lerp(closed_subtitle_pos, home_subtitle_pos, config.menu_t), play_subtitle_pos, config.play_t);

    const float title_visibility = std::clamp(1.0f - config.play_t * 1.35f, 0.0f, 1.0f);
    const Color title_color = with_alpha(t.text, static_cast<unsigned char>(255.0f * title_visibility));
    const Color subtitle_color = with_alpha(t.text_dim, static_cast<unsigned char>(255.0f * title_visibility));
    if (title_visibility > 0.01f) {
        ui::draw_text_f("raythm", title_pos.x, title_pos.y, 124, title_color);
        ui::draw_text_f("trace the line before the beat disappears", subtitle_pos.x, subtitle_pos.y, 30, subtitle_color);
    }

    if (config.menu_t <= 0.01f) {
        return;
    }

    const unsigned char account_alpha = static_cast<unsigned char>(255.0f * std::max(config.menu_t, config.play_t));
    draw_chip_background(config.settings_chip_rect, t.row_hover, t.border, account_alpha);
    const Rectangle settings_visual = ui::is_pressed(config.settings_chip_rect)
        ? ui::inset(config.settings_chip_rect, 1.5f)
        : config.settings_chip_rect;
    draw_settings_icon(settings_visual,
                       ui::is_hovered(config.settings_chip_rect) ? t.text : t.text_secondary,
                       account_alpha);

    draw_chip_background(config.refresh_chip_rect, t.row_hover, t.border, account_alpha);
    const Rectangle refresh_visual = ui::is_pressed(config.refresh_chip_rect)
        ? ui::inset(config.refresh_chip_rect, 1.5f)
        : config.refresh_chip_rect;
    draw_refresh_icon(refresh_visual, ui::is_hovered(config.refresh_chip_rect) ? t.text : t.text_secondary, account_alpha);

    ui::draw_rect_f(config.account_chip_rect, with_alpha(t.panel, account_alpha));
    ui::draw_rect_lines(config.account_chip_rect, kAccountBorderWidth, with_alpha(t.border, account_alpha));
    const Rectangle avatar_rect = {config.account_chip_rect.x + kAvatarOffsetX,
                                   config.account_chip_rect.y + kAvatarOffsetY,
                                   kAvatarSize, kAvatarSize};
    const Vector2 avatar_center = {avatar_rect.x + avatar_rect.width * 0.5f, avatar_rect.y + avatar_rect.height * 0.5f};
    DrawCircleV(avatar_center, kAvatarRadius, with_alpha(config.logged_in ? t.accent : t.row_selected, account_alpha));
    ui::draw_text_in_rect(std::string(config.avatar_label).c_str(), 18, avatar_rect,
                          with_alpha(config.logged_in ? t.panel : t.text, account_alpha), ui::text_align::center);
    const Rectangle account_name_rect = {
        config.account_chip_rect.x + kAccountTextOffsetX, config.account_chip_rect.y + kAccountNameOffsetY,
        config.account_chip_rect.width - kAccountTextRightReserved, kAccountNameHeight
    };
    draw_marquee_text(std::string(config.account_name).c_str(), account_name_rect, 18,
                      with_alpha(t.text, account_alpha), config.now);
    ui::draw_text_in_rect(std::string(config.account_status).c_str(),
                          13,
                          {config.account_chip_rect.x + kAccountTextOffsetX, config.account_chip_rect.y + kAccountStatusOffsetY,
                           config.account_chip_rect.width - kAccountTextRightReserved, kAccountStatusHeight},
                          with_alpha(config.logged_in && !config.email_verified ? t.error : t.text_muted, account_alpha),
                          ui::text_align::left);
    ui::draw_text_in_rect(">", 18,
                          {config.account_chip_rect.x + config.account_chip_rect.width - kAccountArrowRightInset,
                           config.account_chip_rect.y + kAccountArrowOffsetY, kAccountArrowWidth, kAccountArrowHeight},
                          with_alpha(t.text_muted, account_alpha), ui::text_align::center);
}

}  // namespace title_header_view
