#pragma once

#include "raylib.h"
#include "title/online_download_view.h"

namespace title_online_view::preview_controller {

bool update_scrub(state& state,
                  const song_entry_state* song,
                  Rectangle bar_rect,
                  Vector2 mouse,
                  bool left_pressed);

requested_action toggle_playback_action();

}  // namespace title_online_view::preview_controller
