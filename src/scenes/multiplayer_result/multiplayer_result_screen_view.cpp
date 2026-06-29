#include "multiplayer_result/multiplayer_result_screen_view.h"

#include <array>

#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "virtual_screen.h"

namespace multiplayer_result::screen_view {
namespace {

constexpr Rectangle kBackButtonRect{39.0f, 983.0f, 330.0f, 58.0f};
constexpr Rectangle kLeftPanelRect{39.0f, 109.0f, 330.0f, 854.0f};
constexpr Rectangle kMainPanelRect{390.0f, 109.0f, 820.0f, 932.0f};
constexpr Rectangle kRankingPanelRect{1228.0f, 109.0f, 650.0f, 932.0f};
constexpr Rectangle kRankingTitleRect{kRankingPanelRect.x + 30.0f, kRankingPanelRect.y + 18.0f, 260.0f, 54.0f};
constexpr Rectangle kRankingDividerRect{kRankingPanelRect.x + 30.0f, kRankingPanelRect.y + 80.0f, 590.0f, 3.0f};
constexpr Rectangle kRankingCountRect{kRankingPanelRect.x + 402.0f, kRankingPanelRect.y + 30.0f, 176.0f, 34.0f};
constexpr float kSongSelectTopBarHeight = 70.0f;
constexpr std::array<Rectangle, 3> kColumnRects{
    kLeftPanelRect,
    kMainPanelRect,
    kRankingPanelRect,
};

struct back_button_descriptor {
    Rectangle rect;
    const char* label;
    bool enabled;
    ui::button_options options;
};

void draw_background() {
    draw_scene_background(*g_theme);
    ui::vertical_gradient({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                          with_alpha(g_theme->panel, 120),
                          with_alpha(g_theme->bg_alt, 245));
    for (int x = 0; x < kScreenWidth; x += 32) {
        ui::draw_line_ex({static_cast<float>(x), 0.0f}, {static_cast<float>(x), static_cast<float>(kScreenHeight)},
                         1.0f, with_alpha(g_theme->border_light, 22));
    }
    for (int y = 0; y < kScreenHeight; y += 32) {
        ui::draw_line_ex({0.0f, static_cast<float>(y)}, {static_cast<float>(kScreenWidth), static_cast<float>(y)},
                         1.0f, with_alpha(g_theme->border_light, 16));
    }
}

void draw_song_select_top_bar() {
    const Rectangle visible = virtual_screen::visible_rect();
    const Rectangle top_bar{visible.x, visible.y, visible.width, kSongSelectTopBarHeight};
    const Color bar_color = lerp_color(g_theme->panel, BLACK, 0.58f);
    ui::bar_surface(top_bar, with_alpha(bar_color, 235), with_alpha(g_theme->border, 150), 2.0f);
}

void draw_song_select_column(Rectangle rect) {
    const unsigned char fill_alpha =
        static_cast<unsigned char>(g_theme->row_soft_alpha / 2);
    ui::surface(rect, with_alpha(g_theme->section, fill_alpha), with_alpha(g_theme->border_light, 255), 1.2f);
}

const char* back_button_label(bool returning) {
    return returning ? "Returning..." : "Back to Room";
}

ui::button_options back_button_options(bool returning) {
    const unsigned char normal_row_alpha = g_theme->row_soft_alpha;
    const unsigned char hover_row_alpha = g_theme->row_soft_hover_alpha;
    const Color back_bg = returning ? g_theme->row_soft_selected : g_theme->row_soft;
    return {
        .font_size = 24,
        .border_width = 1.5f,
        .bg = with_alpha(back_bg, normal_row_alpha),
        .bg_hover = with_alpha(g_theme->row_soft_hover, hover_row_alpha),
        .text_color = g_theme->text,
        .custom_colors = true,
    };
}

back_button_descriptor back_button_descriptor_for(bool returning) {
    return {
        kBackButtonRect,
        back_button_label(returning),
        !returning,
        back_button_options(returning),
    };
}

}  // namespace

action_result handle_input(bool returning) {
    action_result result;
    const back_button_descriptor back_button = back_button_descriptor_for(returning);
    if (!back_button.enabled) {
        return result;
    }

    if (ui::is_enter_pressed() || ui::is_escape_pressed()) {
        result.back_requested = true;
    }
    if (ui::is_mouse_button_released(back_button.rect, MOUSE_BUTTON_LEFT)) {
        result.back_requested = true;
    }
    return result;
}

void draw_frame() {
    draw_background();
    draw_song_select_top_bar();
    for (const Rectangle column : kColumnRects) {
        draw_song_select_column(column);
    }
}

void draw_ranking_header(int player_count) {
    ui::draw_text_in_rect("Ranking", 38, kRankingTitleRect, g_theme->text, ui::text_align::left);
    ui::divider(kRankingDividerRect, g_theme->border);
    ui::draw_text_in_rect(TextFormat("%d players", player_count), 22,
                          kRankingCountRect, g_theme->text_muted, ui::text_align::right);
}

action_result draw_back_button(bool returning) {
    action_result result;
    const back_button_descriptor back_button = back_button_descriptor_for(returning);
    if (ui::button(back_button.rect, back_button.label, back_button.options).clicked && back_button.enabled) {
        result.back_requested = true;
    }
    return result;
}

}  // namespace multiplayer_result::screen_view
