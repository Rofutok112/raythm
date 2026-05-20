#pragma once

#include "title/seamless_song_select_view.h"

namespace title_local_song_select_controller {

title_play_view::update_result update(song_select::state& state,
                                      title_play_view::mode view_mode,
                                      float anim_t,
                                      Rectangle origin_rect,
                                      float dt);

}  // namespace title_local_song_select_controller
