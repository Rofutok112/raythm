#pragma once

#include <functional>
#include <string>

#include "raylib.h"
#include "scene_manager.h"
#include "song_select/song_select_state.h"
#include "title/title_audio_controller.h"
#include "title/title_command.h"
#include "title/title_play_transfer_controller.h"
#include "title/title_selection_media_coordinator.h"

class title_play_mode_controller {
public:
    struct callbacks {
        std::function<void()> sync_media;
        std::function<void()> request_ranking_reload;
    };

    struct update_result {
        title::command title_command;
        bool update_catalog_include_chart = false;
    };

    [[nodiscard]] static update_result update(scene_manager& manager,
                                              song_select::state& state,
                                              title_audio_controller& audio_controller,
                                              const title_play_transfer_controller& transfer_controller,
                                              const title_selection_media_snapshot& media,
                                              float play_view_anim,
                                              Rectangle play_entry_origin_rect,
                                              float dt,
                                              const callbacks& callbacks);
};
