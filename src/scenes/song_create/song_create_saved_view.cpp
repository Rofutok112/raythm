#include "song_create/song_create_saved_view.h"

#include "theme.h"
#include "ui_draw.h"

namespace song_create::saved_view {

action draw(const song_data& created_song, Rectangle card_rect, int screen_width) {
    constexpr float kCenterY = 465.0f;
    constexpr float kSavedButtonWidth = 330.0f;
    constexpr float kSavedButtonHeight = 72.0f;
    constexpr float kSavedButtonGap = 24.0f;
    const float total_width = kSavedButtonWidth * 2.0f + kSavedButtonGap;
    const float start_x = (static_cast<float>(screen_width) - total_width) * 0.5f;

    const Rectangle title_rect = {card_rect.x + 54.0f, card_rect.y + 42.0f, card_rect.width - 108.0f, 51.0f};
    const Rectangle song_rect = {card_rect.x + 54.0f, card_rect.y + 108.0f, card_rect.width - 108.0f, 45.0f};
    const Rectangle msg_rect = {card_rect.x + 54.0f, card_rect.y + 177.0f, card_rect.width - 108.0f, 45.0f};
    ui::draw_text_in_rect("Song has been created.", 24, title_rect, g_theme->text, ui::text_align::center);
    ui::draw_text_in_rect(created_song.meta.title.c_str(), 22, song_rect, g_theme->text_secondary, ui::text_align::center);
    ui::draw_text_in_rect("What would you like to do next?", 20, msg_rect, g_theme->text_muted, ui::text_align::center);

    const Rectangle add_chart_rect = {start_x, kCenterY + 67.5f, kSavedButtonWidth, kSavedButtonHeight};
    const Rectangle add_later_rect = {start_x + kSavedButtonWidth + kSavedButtonGap, kCenterY + 67.5f,
                                      kSavedButtonWidth, kSavedButtonHeight};

    if (ui::draw_button(add_chart_rect, "ADD CHART", 16).clicked) {
        return action::add_chart;
    }
    if (ui::draw_button(add_later_rect, "ADD LATER", 16).clicked) {
        return action::add_later;
    }
    return action::none;
}

}  // namespace song_create::saved_view
