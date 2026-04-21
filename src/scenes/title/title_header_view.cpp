#include "title/title_header_view.h"

#include <algorithm>

#include "title/title_layout.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"

namespace title_header_view {

void draw(const draw_config& config) {
    const auto& t = *g_theme;

    const Vector2 closed_title_pos = ui::text_position("raythm", 124,
                                                       {config.closed_header_rect.x, config.closed_header_rect.y,
                                                        config.closed_header_rect.width, 124.0f},
                                                       ui::text_align::center);
    const Rectangle title_play_header_rect = title_layout::play_header_rect();
    const Vector2 home_title_pos = ui::text_position("raythm", 124,
                                                     {config.open_header_rect.x, config.open_header_rect.y,
                                                      config.open_header_rect.width, 124.0f},
                                                     ui::text_align::left);
    const Vector2 play_title_pos = ui::text_position("raythm", 108,
                                                     {title_play_header_rect.x, title_play_header_rect.y,
                                                      title_play_header_rect.width, 108.0f},
                                                     ui::text_align::left);
    const Vector2 title_pos =
        tween::lerp(tween::lerp(closed_title_pos, home_title_pos, config.menu_t), play_title_pos, config.play_t);

    const Vector2 closed_subtitle_pos = ui::text_position("trace the line before the beat disappears", 30,
                                                          {config.closed_header_rect.x + 10.0f, config.closed_header_rect.y + 128.0f,
                                                           config.closed_header_rect.width - 10.0f, 30.0f},
                                                          ui::text_align::center);
    const Vector2 home_subtitle_pos = ui::text_position("trace the line before the beat disappears", 30,
                                                        {config.open_header_rect.x + 10.0f, config.open_header_rect.y + 128.0f,
                                                         config.open_header_rect.width - 10.0f, 30.0f},
                                                        ui::text_align::left);
    const Vector2 play_subtitle_pos = ui::text_position("trace the line before the beat disappears", 24,
                                                        {title_play_header_rect.x + 8.0f, title_play_header_rect.y + 102.0f,
                                                         title_play_header_rect.width - 8.0f, 24.0f},
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
    DrawRectangleRec(config.account_chip_rect, with_alpha(t.panel, account_alpha));
    DrawRectangleLinesEx(config.account_chip_rect, 2.0f, with_alpha(t.border, account_alpha));
    const Rectangle avatar_rect = {config.account_chip_rect.x + 12.0f, config.account_chip_rect.y + 9.0f, 40.0f, 40.0f};
    const Vector2 avatar_center = {avatar_rect.x + avatar_rect.width * 0.5f, avatar_rect.y + avatar_rect.height * 0.5f};
    DrawCircleV(avatar_center, 20.0f, with_alpha(config.logged_in ? t.accent : t.row_selected, account_alpha));
    ui::draw_text_in_rect(std::string(config.avatar_label).c_str(), 18, avatar_rect,
                          with_alpha(config.logged_in ? t.panel : t.text, account_alpha), ui::text_align::center);
    const Rectangle account_name_rect = {
        config.account_chip_rect.x + 64.0f, config.account_chip_rect.y + 8.0f, config.account_chip_rect.width - 88.0f, 22.0f
    };
    draw_marquee_text(std::string(config.account_name).c_str(), account_name_rect, 18,
                      with_alpha(t.text, account_alpha), config.now);
    ui::draw_text_in_rect(std::string(config.account_status).c_str(),
                          13,
                          {config.account_chip_rect.x + 64.0f, config.account_chip_rect.y + 30.0f,
                           config.account_chip_rect.width - 88.0f, 16.0f},
                          with_alpha(config.logged_in && !config.email_verified ? t.error : t.text_muted, account_alpha),
                          ui::text_align::left);
    ui::draw_text_in_rect(">", 18,
                          {config.account_chip_rect.x + config.account_chip_rect.width - 24.0f, config.account_chip_rect.y + 12.0f, 12.0f, 24.0f},
                          with_alpha(t.text_muted, account_alpha), ui::text_align::center);
}

}  // namespace title_header_view
