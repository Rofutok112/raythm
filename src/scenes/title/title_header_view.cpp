#include "title/title_header_view.h"

#include <algorithm>

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
    const bool settings_hovered = ui::is_hovered(config.settings_chip_rect);
    const bool settings_pressed = ui::is_pressed(config.settings_chip_rect);
    const Rectangle settings_visual = settings_pressed ? ui::inset(config.settings_chip_rect, 1.5f)
                                                       : config.settings_chip_rect;
    const Color settings_bg = settings_hovered ? t.row_hover : t.panel;
    ui::draw_rect_f(settings_visual, with_alpha(settings_bg, account_alpha));
    ui::draw_rect_lines(settings_visual, kSettingsBorderWidth, with_alpha(t.border, account_alpha));
    ui::draw_text_in_rect("SET", 18, settings_visual,
                          with_alpha(t.text, account_alpha), ui::text_align::center);

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
