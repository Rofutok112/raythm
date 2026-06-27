#pragma once

#include "raylib.h"
#include "song_select/song_select_level_filter.h"

namespace title_play_view {

enum class mode {
    play,
    create,
};

namespace mod_layout {

inline constexpr float kButtonLeftInset = 24.0f;
inline constexpr float kButtonBottomInset = 78.0f;
inline constexpr float kButtonWidth = 276.0f;
inline constexpr float kButtonHeight = 58.0f;
inline constexpr float kModalGapFromButton = 16.0f;
inline constexpr float kModalSidePadding = 18.0f;
inline constexpr float kModalTopPadding = 18.0f;
inline constexpr float kModalBottomPadding = 18.0f;
inline constexpr float kHeaderHeight = 18.0f;
inline constexpr float kHeaderToDescriptionGap = 6.0f;
inline constexpr float kDescriptionHeight = 26.0f;
inline constexpr float kDescriptionToRowsGap = 10.0f;
inline constexpr float kRowHeight = 54.0f;
inline constexpr float kRowGap = 10.0f;
inline constexpr int kRowCount = 2;
inline constexpr float kModalWidth = kButtonWidth;
inline constexpr float kModalHeight = kModalTopPadding + kHeaderHeight + kHeaderToDescriptionGap +
                                      kDescriptionHeight + kDescriptionToRowsGap +
                                      kRowHeight * kRowCount + kRowGap * (kRowCount - 1) +
                                      kModalBottomPadding;

}  // namespace mod_layout

struct layout {
    Rectangle back_rect;
    Rectangle song_column;
    Rectangle main_column;
    Rectangle ranking_column;
    Rectangle jacket_rect;
    Rectangle meta_rect;
    Rectangle chart_detail_rect;
    Rectangle chart_buttons_rect;
    Rectangle ranking_header_rect;
    Rectangle ranking_source_friends_rect;
    Rectangle ranking_source_local_rect;
    Rectangle ranking_source_online_rect;
    Rectangle ranking_list_rect;
};

inline constexpr float kChartFilterMinLevel = song_select::level_filter::kMinLevel;
inline constexpr float kChartFilterUsefulMaxLevel = song_select::level_filter::kUsefulMaxLevel;
inline constexpr float kChartFilterMaxLevel = song_select::level_filter::kMaxLevel;
inline constexpr float kChartFilterUsefulTrack = song_select::level_filter::kUsefulTrack;

layout make_layout(float anim_t, Rectangle origin_rect);
layout make_mode_layout(float anim_t, Rectangle origin_rect, mode view_mode);

Rectangle song_list_rect(const layout& current);
Rectangle center_jacket_rect(const layout& current);
Rectangle center_detail_rect(const layout& current);
Rectangle right_top_pane_rect(Rectangle ranking_column);
Rectangle right_bottom_pane_rect(Rectangle ranking_column);

Rectangle start_button_rect(Rectangle ranking_column);
Rectangle mod_button_rect(Rectangle ranking_column);
Rectangle mod_modal_rect(Rectangle ranking_column);
Rectangle auto_mod_toggle_rect(Rectangle modal);
Rectangle no_fail_mod_toggle_rect(Rectangle modal);

Rectangle preview_prev_button_rect(const layout& current);
Rectangle preview_play_button_rect(const layout& current);
Rectangle preview_next_button_rect(const layout& current);

Rectangle play_filter_button_rect(Rectangle panel);
Rectangle play_filter_modal_rect(const layout& current);
Rectangle play_filter_source_button_rect(Rectangle panel, int index);
Rectangle play_filter_key_button_rect(Rectangle panel, int index);
Rectangle play_filter_clear_button_rect(Rectangle panel);
Rectangle play_filter_level_slider_rect(Rectangle panel);

float level_filter_t(float level);
float level_from_filter_t(float t);
Rectangle level_filter_chip_rect(Rectangle range, float level);

}  // namespace title_play_view
