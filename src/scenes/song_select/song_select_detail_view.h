#pragma once

#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"

namespace song_select {

void draw_frame();
void draw_empty_state(const state& state);
void draw_song_details(const state& state, const preview_controller& preview_controller);
void draw_status_message(const state& state);
void draw_busy_overlay(const std::string& message);

}  // namespace song_select
