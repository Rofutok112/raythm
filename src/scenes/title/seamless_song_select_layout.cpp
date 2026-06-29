#include "title/seamless_song_select_layout.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "scene_common.h"
#include "tween.h"
#include "ui_layout.h"

namespace title_play_view {
namespace {

constexpr Rectangle kPlayBackButtonRect = {39.0f, 983.0f, 480.0f, 58.0f};
constexpr Rectangle kPlaySongColumnRect = {39.0f, 109.0f, 480.0f, 854.0f};
constexpr Rectangle kPlayMainColumnRect = {541.0f, 109.0f, 665.0f, 932.0f};
constexpr Rectangle kPlayRankingColumnRect = {1228.0f, 109.0f, 650.0f, 932.0f};
constexpr Rectangle kPlayJacketRect = {1258.0f, 141.0f, 212.0f, 212.0f};
constexpr Rectangle kPlayChartDetailRect = {1494.0f, 145.0f, 350.0f, 224.0f};
constexpr Rectangle kPlayMetaRect = {860.0f, 347.0f, 286.0f, 10.0f};
constexpr Rectangle kPlayChartButtonsRect = {565.0f, 542.0f, 617.0f, 444.0f};
constexpr Rectangle kPlayRankingHeaderRect = {1256.0f, 466.0f, 596.0f, 38.0f};
constexpr Rectangle kPlayRankingSourceFriendsRect = {1428.0f, 466.0f, 136.0f, 38.0f};
constexpr Rectangle kPlayRankingSourceOnlineRect = {1572.0f, 466.0f, 136.0f, 38.0f};
constexpr Rectangle kPlayRankingSourceLocalRect = {1716.0f, 466.0f, 136.0f, 38.0f};
constexpr Rectangle kPlayRankingListRect = {1256.0f, 512.0f, 596.0f, 431.0f};
constexpr Rectangle kFallbackOriginRect = {840.0f, 564.0f, 240.0f, 90.0f};
constexpr Vector2 kSeedSongOffset = {-495.0f, 33.0f};
constexpr Vector2 kSeedMainOffset = {0.0f, 9.0f};
constexpr Vector2 kSeedRankingOffset = {495.0f, 39.0f};
constexpr Vector2 kSeedBackOffset = {-642.0f, -291.0f};
constexpr Vector2 kSeedJacketOffset = {-102.0f, -39.0f};
constexpr Vector2 kSeedMetaOffset = {15.0f, 138.0f};
constexpr Vector2 kSeedChartDetailOffset = {228.0f, -15.0f};
constexpr Vector2 kSeedChartButtonsOffset = {81.0f, 240.0f};
constexpr Vector2 kSeedRankingHeaderOffset = {522.0f, -276.0f};
constexpr Vector2 kSeedRankingSourceLocalOffset = {627.0f, -285.0f};
constexpr Vector2 kSeedRankingSourceOnlineOffset = {768.0f, -285.0f};
constexpr Vector2 kSeedRankingListOffset = {522.0f, 30.0f};

Rectangle fallback_origin_rect() {
    return kFallbackOriginRect;
}

Rectangle resolve_origin_rect(Rectangle origin_rect) {
    return origin_rect.width > 0.0f && origin_rect.height > 0.0f ? origin_rect : fallback_origin_rect();
}

layout make_layout_for_targets(float anim_t,
                               Rectangle origin_rect,
                               Rectangle song_rect,
                               Rectangle main_rect,
                               Rectangle ranking_rect,
                               Rectangle jacket_rect,
                               Rectangle meta_rect,
                               Rectangle chart_detail_rect,
                               Rectangle chart_buttons_rect,
                               Rectangle ranking_header_rect,
                               Rectangle ranking_source_friends_rect,
                               Rectangle ranking_source_local_rect,
                               Rectangle ranking_source_online_rect,
                               Rectangle ranking_list_rect) {
    const float t = tween::ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);

    const Rectangle seed_song = ui::scaled_from_center(origin, song_rect, 0.68f, kSeedSongOffset);
    const Rectangle seed_main = ui::scaled_from_center(origin, main_rect, 0.74f, kSeedMainOffset);
    const Rectangle seed_ranking = ui::scaled_from_center(origin, ranking_rect, 0.68f, kSeedRankingOffset);
    const Rectangle seed_back = ui::scaled_from_center(origin, kPlayBackButtonRect, 0.9f, kSeedBackOffset);
    const Rectangle seed_jacket = ui::scaled_from_center(origin, jacket_rect, 0.82f, kSeedJacketOffset);
    const Rectangle seed_meta = ui::scaled_from_center(origin, meta_rect, 0.8f, kSeedMetaOffset);
    const Rectangle seed_chart_detail = ui::scaled_from_center(origin, chart_detail_rect, 0.76f, kSeedChartDetailOffset);
    const Rectangle seed_chart_buttons = ui::scaled_from_center(origin, chart_buttons_rect, 0.88f, kSeedChartButtonsOffset);
    const Rectangle seed_ranking_header = ui::scaled_from_center(origin, ranking_header_rect, 0.7f, kSeedRankingHeaderOffset);
    const Rectangle seed_ranking_source_friends =
        ui::scaled_from_center(origin, ranking_source_friends_rect, 0.8f, kSeedRankingSourceOnlineOffset);
    const Rectangle seed_ranking_source_local =
        ui::scaled_from_center(origin, ranking_source_local_rect, 0.8f, kSeedRankingSourceLocalOffset);
    const Rectangle seed_ranking_source_online =
        ui::scaled_from_center(origin, ranking_source_online_rect, 0.8f, kSeedRankingSourceOnlineOffset);
    const Rectangle seed_ranking_list = ui::scaled_from_center(origin, ranking_list_rect, 0.7f, kSeedRankingListOffset);

    return {
        tween::lerp(seed_back, kPlayBackButtonRect, t),
        tween::lerp(seed_song, song_rect, t),
        tween::lerp(seed_main, main_rect, t),
        tween::lerp(seed_ranking, ranking_rect, t),
        tween::lerp(seed_jacket, jacket_rect, t),
        tween::lerp(seed_meta, meta_rect, t),
        tween::lerp(seed_chart_detail, chart_detail_rect, t),
        tween::lerp(seed_chart_buttons, chart_buttons_rect, t),
        tween::lerp(seed_ranking_header, ranking_header_rect, t),
        tween::lerp(seed_ranking_source_friends, ranking_source_friends_rect, t),
        tween::lerp(seed_ranking_source_local, ranking_source_local_rect, t),
        tween::lerp(seed_ranking_source_online, ranking_source_online_rect, t),
        tween::lerp(seed_ranking_list, ranking_list_rect, t),
    };
}

}  // namespace

layout make_layout(float anim_t, Rectangle origin_rect) {
    return make_mode_layout(anim_t, origin_rect, mode::play);
}

layout make_mode_layout(float anim_t, Rectangle origin_rect, mode) {
    return make_layout_for_targets(
        anim_t,
        origin_rect,
        kPlaySongColumnRect,
        kPlayMainColumnRect,
        kPlayRankingColumnRect,
        kPlayJacketRect,
        kPlayMetaRect,
        kPlayChartDetailRect,
        kPlayChartButtonsRect,
        kPlayRankingHeaderRect,
        kPlayRankingSourceFriendsRect,
        kPlayRankingSourceLocalRect,
        kPlayRankingSourceOnlineRect,
        kPlayRankingListRect);
}

Rectangle song_list_rect(const layout& current) {
    return {
        current.song_column.x + 18.0f,
        current.song_column.y + 128.0f,
        current.song_column.width - 36.0f,
        current.song_column.height - 146.0f,
    };
}

Rectangle center_jacket_rect(const layout& current) {
    return {
        current.main_column.x + 28.0f,
        current.main_column.y + 58.0f,
        248.0f,
        248.0f,
    };
}

Rectangle center_detail_rect(const layout& current) {
    const Rectangle jacket = center_jacket_rect(current);
    return {
        jacket.x + jacket.width + 34.0f,
        current.main_column.y + 54.0f,
        current.main_column.x + current.main_column.width - jacket.x - jacket.width - 62.0f,
        150.0f,
    };
}

Rectangle right_top_pane_rect(Rectangle ranking_column) {
    return {ranking_column.x, ranking_column.y, ranking_column.width, 306.0f};
}

Rectangle right_bottom_pane_rect(Rectangle ranking_column) {
    return {
        ranking_column.x,
        ranking_column.y + 337.0f,
        ranking_column.width,
        ranking_column.height - 337.0f,
    };
}

Rectangle start_button_rect(Rectangle ranking_column) {
    return {
        ranking_column.x + 320.0f,
        ranking_column.y + ranking_column.height - 78.0f,
        ranking_column.width - 344.0f,
        58.0f,
    };
}

Rectangle mod_button_rect(Rectangle ranking_column) {
    using namespace mod_layout;
    return {
        ranking_column.x + kButtonLeftInset,
        ranking_column.y + ranking_column.height - kButtonBottomInset,
        kButtonWidth,
        kButtonHeight,
    };
}

Rectangle mod_modal_rect(Rectangle ranking_column) {
    using namespace mod_layout;
    const Rectangle button = mod_button_rect(ranking_column);
    return {
        button.x,
        button.y - kModalGapFromButton - kModalHeight,
        kModalWidth,
        kModalHeight,
    };
}

Rectangle auto_mod_toggle_rect(Rectangle modal) {
    using namespace mod_layout;
    const float row_y = modal.y + kModalTopPadding + kHeaderHeight + kHeaderToDescriptionGap +
                        kDescriptionHeight + kDescriptionToRowsGap;
    return {
        modal.x + kModalSidePadding,
        row_y,
        modal.width - kModalSidePadding * 2.0f,
        kRowHeight,
    };
}

Rectangle no_fail_mod_toggle_rect(Rectangle modal) {
    using namespace mod_layout;
    const Rectangle auto_row = auto_mod_toggle_rect(modal);
    return {
        auto_row.x,
        auto_row.y + kRowHeight + kRowGap,
        auto_row.width,
        kRowHeight,
    };
}

Rectangle preview_prev_button_rect(const layout& current) {
    return {current.meta_rect.x, current.meta_rect.y + 26.0f, 86.0f, 42.0f};
}

Rectangle preview_play_button_rect(const layout& current) {
    return {current.meta_rect.x + 100.0f, current.meta_rect.y + 26.0f, 86.0f, 42.0f};
}

Rectangle preview_next_button_rect(const layout& current) {
    return {current.meta_rect.x + 200.0f, current.meta_rect.y + 26.0f, 86.0f, 42.0f};
}

Rectangle play_filter_button_rect(Rectangle panel) {
    return {panel.x + panel.width - 58.0f, panel.y + 62.0f, 40.0f, 46.0f};
}

Rectangle play_filter_modal_rect(const layout& current) {
    return {current.song_column.x + 70.0f, current.song_column.y + 146.0f, 360.0f, 438.0f};
}

Rectangle play_filter_source_button_rect(Rectangle panel, int index) {
    const float source_y = panel.y + 90.0f;
    std::array<Rectangle, 3> buttons{};
    ui::hstack_fill({panel.x + 18.0f, source_y, panel.width - 36.0f, 36.0f}, 6.0f, buttons);
    return buttons[static_cast<std::size_t>(index)];
}

Rectangle play_filter_key_button_rect(Rectangle panel, int index) {
    constexpr float kKeyWidth = 46.0f;
    constexpr float kKeyGap = 7.0f;
    constexpr int kKeyCount = 5;
    const float group_width = kKeyWidth * static_cast<float>(kKeyCount) + kKeyGap * static_cast<float>(kKeyCount - 1);
    std::array<Rectangle, kKeyCount> buttons{};
    ui::hstack({panel.x + (panel.width - group_width) * 0.5f, panel.y + 318.0f, group_width, 30.0f},
               kKeyWidth,
               kKeyGap,
               buttons);
    return buttons[static_cast<std::size_t>(index)];
}

std::array<play_filter_source_option, 3> play_filter_source_options(Rectangle panel) {
    return {{
        {play_filter_source_button_rect(panel, 0), "ALL", song_select::chart_source_filter::all},
        {play_filter_source_button_rect(panel, 1), "OFFICIAL", song_select::chart_source_filter::official},
        {play_filter_source_button_rect(panel, 2), "COMMUNITY", song_select::chart_source_filter::community},
    }};
}

std::array<play_filter_key_option, 5> play_filter_key_options(Rectangle panel) {
    return {{
        {play_filter_key_button_rect(panel, 0), "ALL", 0},
        {play_filter_key_button_rect(panel, 1), "4K", 4},
        {play_filter_key_button_rect(panel, 2), "5K", 5},
        {play_filter_key_button_rect(panel, 3), "6K", 6},
        {play_filter_key_button_rect(panel, 4), "7K", 7},
    }};
}

Rectangle play_filter_clear_button_rect(Rectangle panel) {
    return {panel.x + 18.0f, panel.y + 378.0f, panel.width - 36.0f, 42.0f};
}

play_filter_clear_option play_filter_clear_option_for(Rectangle panel) {
    return {play_filter_clear_button_rect(panel), "CLEAR FILTERS"};
}

Rectangle play_filter_level_slider_rect(Rectangle panel) {
    return {panel.x + 52.0f, panel.y + 206.0f, panel.width - 104.0f, 24.0f};
}

float level_filter_t(float level) {
    return song_select::level_filter::t_for_level(level);
}

float level_from_filter_t(float t) {
    return song_select::level_filter::level_from_t(t);
}

Rectangle level_filter_chip_rect(Rectangle range, float level) {
    return song_select::level_filter::chip_rect(range, level);
}

}  // namespace title_play_view
