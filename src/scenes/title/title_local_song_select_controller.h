#pragma once

#include "title/create_tools_model.h"
#include "title/seamless_song_select_view.h"
#include "title/title_audio_controller.h"

namespace title_local_song_select_controller {

title_play_view::update_result update(song_select::state& state,
                                      title_play_view::mode view_mode,
                                      float anim_t,
                                      Rectangle origin_rect,
                                      float dt,
                                      title_audio_controller* audio_controller = nullptr,
                                      const title_create_tools_model::view_model* create_tools_model = nullptr,
                                      song_select::preview_audio_loader::load_status preview_audio_status =
                                          song_select::preview_audio_loader::load_status::idle);

}  // namespace title_local_song_select_controller
