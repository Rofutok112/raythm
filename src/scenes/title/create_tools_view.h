#pragma once

#include "raylib.h"
#include "song_select/song_select_state.h"
#include "title/create_tools_model.h"
#include "title/seamless_song_select_view.h"

namespace title_create_tools_view {

struct draw_config {
    title_play_view::layout current;
    unsigned char alpha = 255;
    Color button_base = WHITE;
    unsigned char normal_row_alpha = 255;
    unsigned char hover_row_alpha = 255;
};

title_play_view::update_result update(const title_create_tools_model::view_model& model,
                                      const title_play_view::layout& current,
                                      bool left_pressed,
                                      Vector2 mouse);

void draw(const title_create_tools_model::view_model& model, const draw_config& config);

}  // namespace title_create_tools_view
