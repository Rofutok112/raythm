#pragma once

#include "raylib.h"
#include "song_select/song_select_state.h"

namespace title_song_list_view {

struct draw_config {
    Rectangle column_rect;
    float play_t = 1.0f;
    unsigned char alpha = 255;
    Color button_base{};
    Color button_selected{};
    unsigned char normal_row_alpha = 255;
    unsigned char hover_row_alpha = 255;
    unsigned char selected_row_alpha = 255;
    double now = 0.0;
};

float content_height(int count);
float max_scroll(Rectangle area, int count);
Rectangle row_rect(Rectangle area, int index, float scroll_y);
int hit_test(Rectangle area, float scroll_y, Vector2 point, int count);
void draw(const song_select::state& state, const draw_config& config);

}  // namespace title_song_list_view
