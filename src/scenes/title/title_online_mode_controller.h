#pragma once

#include <functional>

#include "raylib.h"
#include "title/online_download_view.h"

class title_online_mode_controller {
public:
    struct callbacks {
        std::function<void()> enter_home;
        std::function<void()> select_preview_song;
        std::function<void()> resume_preview;
        std::function<void()> pause_preview;
        std::function<void()> open_local_selection;
    };

    static void update(title_online_view::state& state,
                       float play_view_anim,
                       Rectangle play_entry_origin_rect,
                       float dt,
                       const callbacks& callbacks);
};
