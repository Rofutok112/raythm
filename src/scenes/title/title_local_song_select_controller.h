#pragma once

#include "title/create_tools_model.h"
#include "title/seamless_song_select_view.h"

namespace title_local_song_select_controller {

title_play_view::update_result update(song_select::state& state,
                                      title_play_view::mode view_mode,
                                      float anim_t,
                                      Rectangle origin_rect,
                                      float dt,
                                      const title_create_tools_model::view_model* create_tools_model = nullptr,
                                      title_preview_snapshot preview = {});

}  // namespace title_local_song_select_controller
