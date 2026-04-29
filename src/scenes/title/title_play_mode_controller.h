#pragma once

#include <functional>

#include "raylib.h"
#include "scene_manager.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"
#include "title/title_play_transfer_controller.h"

class title_play_mode_controller {
public:
    struct callbacks {
        std::function<void()> enter_home;
        std::function<void()> sync_media;
        std::function<void()> request_ranking_reload;
    };

    static void update(scene_manager& manager,
                       song_select::state& state,
                       song_select::preview_controller& preview_controller,
                       const title_play_transfer_controller& transfer_controller,
                       float play_view_anim,
                       Rectangle play_entry_origin_rect,
                       float dt,
                       const callbacks& callbacks);
};
