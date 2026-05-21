#pragma once

#include "data_models.h"
#include "raylib.h"

namespace song_create::saved_view {

enum class action {
    none,
    add_chart,
    add_later,
};

action draw(const song_data& created_song, Rectangle card_rect, int screen_width);

}  // namespace song_create::saved_view
