#pragma once

#include <functional>

#include "raylib.h"
#include "scene_manager.h"
#include "song_select/song_select_state.h"
#include "title/title_audio_controller.h"
#include "title/title_play_transfer_controller.h"

class title_play_mode_controller {
public:
    struct callbacks {
        std::function<void()> enter_home;
        std::function<void()> sync_media;
        std::function<void()> request_ranking_reload;
        std::function<void(bool)> open_update_catalog;
        std::function<bool()> add_selected_to_multiplayer;
    };

    static void update(scene_manager& manager,
                       song_select::state& state,
                       title_audio_controller& audio_controller,
                       const title_play_transfer_controller& transfer_controller,
                       float play_view_anim,
                       Rectangle play_entry_origin_rect,
                       float dt,
                       const callbacks& callbacks);
};
