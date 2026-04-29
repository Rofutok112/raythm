#pragma once

#include <functional>

#include "raylib.h"
#include "scene_manager.h"
#include "song_select/song_select_state.h"
#include "title/title_play_transfer_controller.h"

class title_create_mode_controller {
public:
    struct callbacks {
        std::function<void()> enter_home;
        std::function<void()> sync_preview;
        std::function<void(const song_select::song_entry&)> start_song_upload;
        std::function<void(const song_select::song_entry&, const song_select::chart_option&)> start_chart_upload;
        std::function<title_play_transfer_controller::catalog_callbacks()> transfer_callbacks;
        std::function<bool()> sync_media_on_transfer;
        std::function<bool()> upload_in_progress;
    };

    static void update(scene_manager& manager,
                       song_select::state& state,
                       title_play_transfer_controller& transfer_controller,
                       float play_view_anim,
                       Rectangle play_entry_origin_rect,
                       float dt,
                       const callbacks& callbacks);
};
