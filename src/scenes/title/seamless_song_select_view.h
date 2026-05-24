#pragma once

#include "raylib.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"

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
    Rectangle ranking_source_local_rect;
    Rectangle ranking_source_online_rect;
    Rectangle ranking_list_rect;
};

struct update_result {
    bool back_requested = false;
    bool play_requested = false;
    bool multiplayer_select_requested = false;
    bool preview_toggle_requested = false;
    bool song_selection_changed = false;
    bool chart_selection_changed = false;
    bool ranking_source_changed = false;
    bool delete_song_requested = false;
    bool delete_chart_requested = false;
    bool create_song_requested = false;
    bool edit_song_requested = false;
    bool upload_song_requested = false;
    bool import_song_requested = false;
    bool export_song_requested = false;
    bool create_chart_requested = false;
    bool edit_chart_requested = false;
    bool upload_chart_requested = false;
    bool import_chart_requested = false;
    bool export_chart_requested = false;
    bool edit_mv_requested = false;
    bool manage_library_requested = false;
    bool update_song_requested = false;
    bool update_chart_requested = false;
};

layout make_layout(float anim_t, Rectangle origin_rect);
layout make_mode_layout(float anim_t, Rectangle origin_rect, mode view_mode);
void draw(song_select::state& state,
          const song_select::preview_controller& preview_controller,
          mode view_mode,
          float anim_t,
          Rectangle origin_rect);

}  // namespace title_play_view
