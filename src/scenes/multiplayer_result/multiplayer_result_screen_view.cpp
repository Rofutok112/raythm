#include "multiplayer_result/multiplayer_result_screen_view.h"

#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
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

}  // namespace

action_result handle_input(bool returning) {
    action_result result;
    if (returning) {
        return result;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        ui::contains_point(kBackButtonRect, virtual_screen::get_virtual_mouse())) {
        result.back_requested = true;
    }
    return result;
}

void draw_frame() {
    draw_background();
    draw_song_select_top_bar();
    draw_song_select_column(kLeftPanelRect);
    draw_song_select_column(kMainPanelRect);
    draw_song_select_column(kRankingPanelRect);
}

void draw_ranking_header(int player_count) {
    ui::draw_text_in_rect("Ranking", 38, kRankingTitleRect, g_theme->text, ui::text_align::left);
    ui::divider(kRankingDividerRect, g_theme->border);
    ui::draw_text_in_rect(TextFormat("%d players", player_count), 22,
                          kRankingCountRect, g_theme->text_muted, ui::text_align::right);
}

action_result draw_back_button(bool returning) {
    action_result result;
    const unsigned char normal_row_alpha = g_theme->row_soft_alpha;
    const unsigned char hover_row_alpha = g_theme->row_soft_hover_alpha;
    const Color back_bg = returning ? g_theme->row_soft_selected : g_theme->row_soft;
    if (ui::button(kBackButtonRect, returning ? "Returning..." : "Back to Room", {
            .font_size = 24,
            .border_width = 1.5f,
            .bg = with_alpha(back_bg, normal_row_alpha),
            .bg_hover = with_alpha(g_theme->row_soft_hover, hover_row_alpha),
            .text_color = g_theme->text,
            .custom_colors = true,
        }).clicked &&
        !returning) {
        result.back_requested = true;
    }
    return result;
}

}  // namespace multiplayer_result::screen_view
