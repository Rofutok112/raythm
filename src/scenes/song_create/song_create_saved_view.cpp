#include "song_create/song_create_saved_view.h"

#include <array>

#include "theme.h"
#include "ui_draw.h"
#include "ui_layout.h"

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

struct saved_action_definition {
    action value;
    const char* label;
};

struct saved_text_line {
    Rectangle rect;
    const char* text;
    int font_size;
    Color color;
};

constexpr std::array<saved_action_definition, 2> kSavedActions = {{
    {action::add_chart, "ADD CHART"},
    {action::add_later, "ADD LATER"},
}};

saved_view_layout saved_view_layout_for(Rectangle card_rect, int screen_width) {
    std::array<Rectangle, 2> buttons{};
    ui::centered_hstack({0.0f, kCenterY + 67.5f, static_cast<float>(screen_width), kSavedButtonHeight},
                        kSavedButtonWidth,
                        kSavedButtonHeight,
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

std::array<ui::action_button_definition<action>, kSavedActions.size()> saved_action_buttons_for(
    const saved_view_layout& layout) {
    std::array<ui::action_button_definition<action>, kSavedActions.size()> buttons{};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        buttons[i] = {
            .rect = layout.buttons[i],
            .label = kSavedActions[i].label,
            .action = kSavedActions[i].value,
        };
    }
    return buttons;
}

std::array<saved_text_line, 3> saved_text_lines_for(const saved_view_layout& layout,
                                                    const song_data& created_song) {
    return {{
        {layout.title, "Song has been created.", 24, g_theme->text},
        {layout.song, created_song.meta.title.c_str(), 22, g_theme->text_secondary},
        {layout.message, "What would you like to do next?", 20, g_theme->text_muted},
    }};
}

}  // namespace

action draw(const song_data& created_song, Rectangle card_rect, int screen_width) {
    const saved_view_layout layout = saved_view_layout_for(card_rect, screen_width);
    for (const saved_text_line& line : saved_text_lines_for(layout, created_song)) {
        ui::draw_text_in_rect(line.text, line.font_size, line.rect, line.color, ui::text_align::center);
    }

    const std::array<ui::action_button_definition<action>, kSavedActions.size()> buttons =
        saved_action_buttons_for(layout);
    const auto clicked_action = ui::draw_action_buttons<action>(buttons, {
        .font_size = 16,
        .border_width = 2.0f,
    });
    if (clicked_action.has_value()) {
        return *clicked_action;
    }
    return action::none;
}

}  // namespace song_create::saved_view
