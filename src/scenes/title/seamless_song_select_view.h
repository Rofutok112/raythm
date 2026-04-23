#pragma once

#include "raylib.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"

namespace title_play_view {

enum class mode {
    play,
    create,
};

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
    bool song_selection_changed = false;
    bool chart_selection_changed = false;
    bool ranking_source_changed = false;
    bool delete_song_requested = false;
    bool create_song_requested = false;
    bool edit_song_requested = false;
    bool upload_song_requested = false;
    bool create_chart_requested = false;
    bool edit_chart_requested = false;
    bool upload_chart_requested = false;
    bool edit_mv_requested = false;
    bool manage_library_requested = false;
};

layout make_layout(float anim_t, Rectangle origin_rect);
update_result update(song_select::state& state, mode view_mode, float anim_t, Rectangle origin_rect, float dt);
void draw(const song_select::state& state,
          const song_select::preview_controller& preview_controller,
          mode view_mode,
          float anim_t,
          Rectangle origin_rect);

}  // namespace title_play_view
