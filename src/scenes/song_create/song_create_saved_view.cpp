#include "song_create/song_create_saved_view.h"

#include <array>

#include "theme.h"
#include "ui_draw.h"

namespace song_create::saved_view {

namespace {

constexpr float kCenterY = 465.0f;
constexpr float kSavedButtonWidth = 330.0f;
constexpr float kSavedButtonHeight = 72.0f;
constexpr float kSavedButtonGap = 24.0f;
constexpr float kCardContentInsetX = 54.0f;

struct saved_view_layout {
    Rectangle title;
    Rectangle song;
    Rectangle message;
    std::array<Rectangle, 2> buttons;
};

float saved_button_row_width() {
    return kSavedButtonWidth * 2.0f + kSavedButtonGap;
}

saved_view_layout saved_view_layout_for(Rectangle card_rect, int screen_width) {
    const float total_width = saved_button_row_width();
    const float start_x = (static_cast<float>(screen_width) - total_width) * 0.5f;
    std::array<Rectangle, 2> buttons{};
    ui::hstack_fill({start_x, kCenterY + 67.5f, total_width, kSavedButtonHeight},
                    kSavedButtonGap,
                    buttons);
    return {
        {card_rect.x + kCardContentInsetX, card_rect.y + 42.0f,
         card_rect.width - kCardContentInsetX * 2.0f, 51.0f},
        {card_rect.x + kCardContentInsetX, card_rect.y + 108.0f,
         card_rect.width - kCardContentInsetX * 2.0f, 45.0f},
        {card_rect.x + kCardContentInsetX, card_rect.y + 177.0f,
         card_rect.width - kCardContentInsetX * 2.0f, 45.0f},
        buttons,
    };
}

}  // namespace

action draw(const song_data& created_song, Rectangle card_rect, int screen_width) {
    const saved_view_layout layout = saved_view_layout_for(card_rect, screen_width);
    ui::draw_text_in_rect("Song has been created.", 24, layout.title, g_theme->text, ui::text_align::center);
    ui::draw_text_in_rect(created_song.meta.title.c_str(), 22, layout.song,
                          g_theme->text_secondary, ui::text_align::center);
    ui::draw_text_in_rect("What would you like to do next?", 20, layout.message,
                          g_theme->text_muted, ui::text_align::center);

    if (ui::button(layout.buttons[0], "ADD CHART", {.font_size = 16}).clicked) {
        return action::add_chart;
    }
    if (ui::button(layout.buttons[1], "ADD LATER", {.font_size = 16}).clicked) {
        return action::add_later;
    }
    return action::none;
}

}  // namespace song_create::saved_view
