#pragma once

#include "raylib.h"
#include "song_select/song_select_ranking_loader.h"
#include "song_select/song_select_state.h"
#include "title/seamless_song_select_layout.h"
#include "title/title_selection_media_coordinator.h"

namespace title_create_tools_model {
struct view_model;
}

namespace title_play_view {

struct update_result {
    bool back_requested = false;
    bool play_requested = false;
    bool multiplayer_select_requested = false;
    bool preview_toggle_requested = false;
    bool preview_pause_requested = false;
    bool preview_seek_requested = false;
    bool preview_resume_requested = false;
    double preview_seek_seconds = 0.0;
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
    bool update_song_requested = false;
    bool update_chart_requested = false;
    std::string requested_profile_user_id;
};

void draw(song_select::state& state,
          const title_selection_media_snapshot& media,
          mode view_mode,
          float anim_t,
          Rectangle origin_rect,
          const title_create_tools_model::view_model* create_tools_model = nullptr);

}  // namespace title_play_view
