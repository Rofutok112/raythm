#include "title/seamless_song_select_view.h"

#include <algorithm>

#include "ranking_service.h"
#include "raylib.h"
#include "scene_common.h"
#include "song_select/song_select_login_dialog.h"
#include "title/center_panel_view.h"
#include "title/ranking_panel_view.h"
#include "title/song_list_view.h"
#include "theme.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace title_play_view {
namespace {

constexpr Rectangle kPlayBackButtonRect = {48.0f, 38.0f, 98.0f, 38.0f};
constexpr Rectangle kPlaySongColumnRect = {66.0f, 108.0f, 338.0f, 504.0f};
constexpr Rectangle kPlayMainColumnRect = {438.0f, 100.0f, 402.0f, 520.0f};
constexpr Rectangle kPlayRankingColumnRect = {878.0f, 102.0f, 338.0f, 516.0f};
constexpr Rectangle kPlayJacketRect = {456.0f, 182.0f, 188.0f, 188.0f};
constexpr Rectangle kPlayChartDetailRect = {668.0f, 182.0f, 172.0f, 90.0f};
constexpr Rectangle kPlayMetaRect = {456.0f, 388.0f, 384.0f, 60.0f};
constexpr Rectangle kPlayChartButtonsRect = {456.0f, 400.0f, 384.0f, 218.0f};
constexpr Rectangle kPlayRankingHeaderRect = {878.0f, 102.0f, 338.0f, 36.0f};
constexpr Rectangle kPlayRankingSourceLocalRect = {1034.0f, 98.0f, 86.0f, 34.0f};
constexpr Rectangle kPlayRankingSourceOnlineRect = {1126.0f, 98.0f, 90.0f, 34.0f};
constexpr Rectangle kPlayRankingListRect = {878.0f, 150.0f, 338.0f, 468.0f};
constexpr float kCreateToolButtonHeight = 54.0f;
constexpr float kCreateToolButtonGap = 12.0f;
float ease_out_cubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv * inv;
}

float lerp_value(float from, float to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return from + (to - from) * clamped;
}

Rectangle lerp_rect(Rectangle from, Rectangle to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return {
        lerp_value(from.x, to.x, clamped),
        lerp_value(from.y, to.y, clamped),
        lerp_value(from.width, to.width, clamped),
        lerp_value(from.height, to.height, clamped),
    };
}

Rectangle centered_scaled_rect(Rectangle anchor, Rectangle target, float scale, Vector2 offset = {0.0f, 0.0f}) {
    const Vector2 center = {
        anchor.x + anchor.width * 0.5f + offset.x,
        anchor.y + anchor.height * 0.5f + offset.y,
    };
    const float width = target.width * scale;
    const float height = target.height * scale;
    return {
        center.x - width * 0.5f,
        center.y - height * 0.5f,
        width,
        height,
    };
}

Rectangle fallback_origin_rect() {
    return {
        static_cast<float>(kScreenWidth) * 0.5f - 80.0f,
        376.0f,
        160.0f,
        60.0f,
    };
}

Rectangle resolve_origin_rect(Rectangle origin_rect) {
    return origin_rect.width > 0.0f && origin_rect.height > 0.0f ? origin_rect : fallback_origin_rect();
}
}  // namespace

layout make_layout(float anim_t, Rectangle origin_rect) {
    const float t = ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);

    const Rectangle seed_song = centered_scaled_rect(origin, kPlaySongColumnRect, 0.68f, {-330.0f, 22.0f});
    const Rectangle seed_main = centered_scaled_rect(origin, kPlayMainColumnRect, 0.74f, {0.0f, 6.0f});
    const Rectangle seed_ranking = centered_scaled_rect(origin, kPlayRankingColumnRect, 0.68f, {330.0f, 26.0f});
    const Rectangle seed_back = centered_scaled_rect(origin, kPlayBackButtonRect, 0.9f, {-428.0f, -194.0f});
    const Rectangle seed_jacket = centered_scaled_rect(origin, kPlayJacketRect, 0.82f, {-68.0f, -26.0f});
    const Rectangle seed_meta = centered_scaled_rect(origin, kPlayMetaRect, 0.8f, {10.0f, 92.0f});
    const Rectangle seed_chart_detail = centered_scaled_rect(origin, kPlayChartDetailRect, 0.76f, {152.0f, -10.0f});
    const Rectangle seed_chart_buttons = centered_scaled_rect(origin, kPlayChartButtonsRect, 0.88f, {54.0f, 160.0f});
    const Rectangle seed_ranking_header = centered_scaled_rect(origin, kPlayRankingHeaderRect, 0.7f, {348.0f, -184.0f});
    const Rectangle seed_ranking_source_local = centered_scaled_rect(origin, kPlayRankingSourceLocalRect, 0.8f, {418.0f, -190.0f});
    const Rectangle seed_ranking_source_online = centered_scaled_rect(origin, kPlayRankingSourceOnlineRect, 0.8f, {512.0f, -190.0f});
    const Rectangle seed_ranking_list = centered_scaled_rect(origin, kPlayRankingListRect, 0.7f, {348.0f, 20.0f});

    return {
        lerp_rect(seed_back, kPlayBackButtonRect, t),
        lerp_rect(seed_song, kPlaySongColumnRect, t),
        lerp_rect(seed_main, kPlayMainColumnRect, t),
        lerp_rect(seed_ranking, kPlayRankingColumnRect, t),
        lerp_rect(seed_jacket, kPlayJacketRect, t),
        lerp_rect(seed_meta, kPlayMetaRect, t),
        lerp_rect(seed_chart_detail, kPlayChartDetailRect, t),
        lerp_rect(seed_chart_buttons, kPlayChartButtonsRect, t),
        lerp_rect(seed_ranking_header, kPlayRankingHeaderRect, t),
        lerp_rect(seed_ranking_source_local, kPlayRankingSourceLocalRect, t),
        lerp_rect(seed_ranking_source_online, kPlayRankingSourceOnlineRect, t),
        lerp_rect(seed_ranking_list, kPlayRankingListRect, t),
    };
}

update_result update(song_select::state& state, mode view_mode, float anim_t, Rectangle origin_rect, float dt) {
    update_result result;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const float wheel = GetMouseWheelMove();
    const layout current = make_layout(anim_t, origin_rect);

    if (ui::is_clicked(current.back_rect) || IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
        return result;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        result.play_requested = true;
        return result;
    }

    if (view_mode == mode::play) {
        const title_ranking_view::draw_config ranking_config = {
            .header_rect = current.ranking_header_rect,
            .source_local_rect = current.ranking_source_local_rect,
            .source_online_rect = current.ranking_source_online_rect,
            .list_rect = current.ranking_list_rect,
        };
        if (left_pressed) {
            const auto source = title_ranking_view::hit_test_source(ranking_config, mouse);
            if (source.has_value() && source.value() != state.ranking_panel.selected_source) {
                state.ranking_panel.selected_source = *source;
                result.ranking_source_changed = true;
                return result;
            }
        }
    } else if (left_pressed) {
        const Rectangle list_rect = current.ranking_list_rect;
        const Rectangle tools[] = {
            {list_rect.x, list_rect.y, list_rect.width, kCreateToolButtonHeight},
            {list_rect.x, list_rect.y + (kCreateToolButtonHeight + kCreateToolButtonGap) * 1.0f, list_rect.width, kCreateToolButtonHeight},
            {list_rect.x, list_rect.y + (kCreateToolButtonHeight + kCreateToolButtonGap) * 2.0f, list_rect.width, kCreateToolButtonHeight},
            {list_rect.x, list_rect.y + (kCreateToolButtonHeight + kCreateToolButtonGap) * 3.0f, list_rect.width, kCreateToolButtonHeight},
            {list_rect.x, list_rect.y + (kCreateToolButtonHeight + kCreateToolButtonGap) * 4.0f, list_rect.width, kCreateToolButtonHeight},
            {list_rect.x, list_rect.y + (kCreateToolButtonHeight + kCreateToolButtonGap) * 5.0f, list_rect.width, kCreateToolButtonHeight},
        };
        if (CheckCollisionPointRec(mouse, tools[0])) { result.create_song_requested = true; return result; }
        if (CheckCollisionPointRec(mouse, tools[1])) { result.edit_song_requested = true; return result; }
        if (CheckCollisionPointRec(mouse, tools[2])) { result.create_chart_requested = true; return result; }
        if (CheckCollisionPointRec(mouse, tools[3])) { result.edit_chart_requested = true; return result; }
        if (CheckCollisionPointRec(mouse, tools[4])) { result.edit_mv_requested = true; return result; }
        if (CheckCollisionPointRec(mouse, tools[5])) { result.manage_library_requested = true; return result; }
    }

    if (left_pressed) {
        const auto filtered = song_select::filtered_charts_for_selected_song(state);
        const int clicked_chart =
            title_center_view::hit_test_chart(current.chart_buttons_rect, state.chart_scroll_y, mouse,
                                              static_cast<int>(filtered.size()));
        if (clicked_chart >= 0) {
            if (state.difficulty_index == clicked_chart) {
                result.play_requested = true;
            } else {
                state.difficulty_index = clicked_chart;
                state.chart_change_anim_t = 1.0f;
                result.chart_selection_changed = true;
            }
            return result;
        }
    }

    if (left_pressed && !state.songs.empty()) {
        const int clicked_song =
            title_song_list_view::hit_test(current.song_column, state.scroll_y, mouse,
                                           static_cast<int>(state.songs.size()));
        if (clicked_song >= 0) {
            if (song_select::apply_song_selection(state, clicked_song, 0)) {
                result.song_selection_changed = true;
            }
            return result;
        }
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        const int next_index = std::clamp(state.selected_song_index - 1, 0, static_cast<int>(state.songs.size()) - 1);
        if (next_index != state.selected_song_index && song_select::apply_song_selection(state, next_index, 0)) {
            result.song_selection_changed = true;
        }
    } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        const int next_index = std::clamp(state.selected_song_index + 1, 0, static_cast<int>(state.songs.size()) - 1);
        if (next_index != state.selected_song_index && song_select::apply_song_selection(state, next_index, 0)) {
            result.song_selection_changed = true;
        }
    }

    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        const int next_index = std::clamp(state.difficulty_index - 1, 0, static_cast<int>(filtered.size()) - 1);
        if (next_index != state.difficulty_index) {
            state.difficulty_index = next_index;
            state.chart_change_anim_t = 1.0f;
            result.chart_selection_changed = true;
        }
    } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        const int next_index = std::clamp(state.difficulty_index + 1, 0, static_cast<int>(filtered.size()) - 1);
        if (next_index != state.difficulty_index) {
            state.difficulty_index = next_index;
            state.chart_change_anim_t = 1.0f;
            result.chart_selection_changed = true;
        }
    }

    if (CheckCollisionPointRec(mouse, current.song_column) && wheel != 0.0f) {
        state.scroll_y_target -= wheel * 42.0f;
    } else if (CheckCollisionPointRec(mouse, current.chart_buttons_rect) && wheel != 0.0f) {
        state.chart_scroll_y_target -= wheel * 42.0f;
    } else if (view_mode == mode::play && CheckCollisionPointRec(mouse, current.ranking_list_rect) && wheel != 0.0f) {
        state.ranking_panel.scroll_y_target -= wheel * 42.0f;
    }

    state.scroll_y_target = std::clamp(
        state.scroll_y_target, 0.0f,
        title_song_list_view::max_scroll(current.song_column, static_cast<int>(state.songs.size())));
    state.scroll_y += (state.scroll_y_target - state.scroll_y) * std::min(1.0f, dt * 12.0f);
    if (std::fabs(state.scroll_y - state.scroll_y_target) < 0.5f) {
        state.scroll_y = state.scroll_y_target;
    }

    state.chart_scroll_y_target = std::clamp(
        state.chart_scroll_y_target, 0.0f,
        title_center_view::max_chart_scroll(current.chart_buttons_rect, static_cast<int>(filtered.size())));
    state.chart_scroll_y += (state.chart_scroll_y_target - state.chart_scroll_y) * std::min(1.0f, dt * 12.0f);
    if (std::fabs(state.chart_scroll_y - state.chart_scroll_y_target) < 0.5f) {
        state.chart_scroll_y = state.chart_scroll_y_target;
    }

    if (view_mode == mode::play) {
        state.ranking_panel.scroll_y_target = std::clamp(
            state.ranking_panel.scroll_y_target, 0.0f,
            title_ranking_view::max_scroll(current.ranking_list_rect, state.ranking_panel.listing));
        state.ranking_panel.scroll_y +=
            (state.ranking_panel.scroll_y_target - state.ranking_panel.scroll_y) *
            std::min(1.0f, dt * 12.0f);
        if (std::fabs(state.ranking_panel.scroll_y - state.ranking_panel.scroll_y_target) < 0.5f) {
            state.ranking_panel.scroll_y = state.ranking_panel.scroll_y_target;
        }
    }

    return result;
}

void draw(const song_select::state& state,
          const song_select::preview_controller& preview_controller,
          mode view_mode,
          float anim_t,
          Rectangle origin_rect) {
    const auto& t = *g_theme;
    const float play_t = ease_out_cubic(anim_t);
    if (play_t <= 0.01f) {
        return;
    }

    const layout current = make_layout(anim_t, origin_rect);
    const float content_fade_t = std::clamp((play_t - 0.18f) / 0.62f, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(255.0f * content_fade_t);
    const double now = GetTime();
    const Color button_base = t.row_soft;
    const Color button_hover = t.row_soft_hover;
    const Color button_selected = t.row_soft_selected;
    const Color button_selected_hover = t.row_soft_selected_hover;
    const unsigned char normal_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_alpha) / 255);
    const unsigned char hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_hover_alpha) / 255);
    const unsigned char selected_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_alpha) / 255);
    const unsigned char selected_hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_hover_alpha) / 255);
    ui::draw_button_colored(current.back_rect, "HOME", 16,
                            with_alpha(button_base, normal_row_alpha), with_alpha(button_hover, hover_row_alpha), with_alpha(t.text, alpha), 1.5f);

    DrawLineEx({current.song_column.x + current.song_column.width + 22.0f, current.song_column.y + 18.0f},
               {current.song_column.x + current.song_column.width + 22.0f, current.song_column.y + current.song_column.height - 18.0f},
               1.2f, with_alpha(t.border_light, static_cast<unsigned char>(170.0f * play_t)));
    DrawLineEx({current.ranking_column.x - 24.0f, current.ranking_column.y + 24.0f},
               {current.ranking_column.x - 24.0f, current.ranking_column.y + current.ranking_column.height - 20.0f},
               1.2f, with_alpha(t.border_light, static_cast<unsigned char>(170.0f * play_t)));

    title_song_list_view::draw(state, {
        .column_rect = current.song_column,
        .play_t = play_t,
        .alpha = alpha,
        .button_base = button_base,
        .button_selected = button_selected,
        .normal_row_alpha = normal_row_alpha,
        .hover_row_alpha = hover_row_alpha,
        .selected_row_alpha = selected_row_alpha,
        .now = now,
    });

    const song_select::song_entry* song = song_select::selected_song(state);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);

    title_center_view::draw(state, preview_controller, song, chart, filtered, {
        .main_column_rect = current.main_column,
        .jacket_rect = current.jacket_rect,
        .chart_detail_rect = current.chart_detail_rect,
        .chart_buttons_rect = current.chart_buttons_rect,
        .play_t = play_t,
        .alpha = alpha,
        .button_base = button_base,
        .button_selected = button_selected,
        .normal_row_alpha = normal_row_alpha,
        .hover_row_alpha = hover_row_alpha,
        .selected_row_alpha = selected_row_alpha,
        .now = now,
    });

    if (view_mode == mode::play) {
        title_ranking_view::draw(state.ranking_panel, {
            .header_rect = current.ranking_header_rect,
            .source_local_rect = current.ranking_source_local_rect,
            .source_online_rect = current.ranking_source_online_rect,
            .list_rect = current.ranking_list_rect,
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_hover = button_hover,
            .button_selected = button_selected,
            .button_selected_hover = button_selected_hover,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .selected_hover_row_alpha = selected_hover_row_alpha,
        });
    } else {
        ui::draw_text_in_rect("CREATE TOOLS", 18, current.ranking_header_rect, with_alpha(t.text, alpha), ui::text_align::left);
        const struct tool_entry { const char* title; const char* detail; } entries[] = {
            {"NEW SONG", "Start a new song package."},
            {"EDIT SONG", "Edit selected song metadata."},
            {"NEW CHART", "Open editor for a new chart."},
            {"EDIT CHART", "Open selected chart in editor."},
            {"MV EDITOR", "Open MV editor for song."},
            {"MANAGE", "Import / export with legacy tools."},
        };
        for (int i = 0; i < 6; ++i) {
            const Rectangle rect = {
                current.ranking_list_rect.x,
                current.ranking_list_rect.y + (kCreateToolButtonHeight + kCreateToolButtonGap) * static_cast<float>(i),
                current.ranking_list_rect.width,
                kCreateToolButtonHeight
            };
            const bool hovered = CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), rect);
            const unsigned char row_alpha = hovered ? hover_row_alpha : normal_row_alpha;
            DrawRectangleRec(rect, with_alpha(button_base, row_alpha));
            DrawRectangleLinesEx(rect, 1.2f, with_alpha(t.border, row_alpha));
            ui::draw_text_in_rect(entries[i].title, 18,
                                  {rect.x + 14.0f, rect.y + 8.0f, rect.width - 28.0f, 20.0f},
                                  with_alpha(t.text, alpha), ui::text_align::left);
            ui::draw_text_in_rect(entries[i].detail, 12,
                                  {rect.x + 14.0f, rect.y + 30.0f, rect.width - 28.0f, 16.0f},
                                  with_alpha(t.text_muted, alpha), ui::text_align::left);
        }
    }
}

}  // namespace title_play_view
