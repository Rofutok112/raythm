#pragma once

#include "raylib.h"
#include "song_select/song_select_state.h"

namespace title_song_list_view {

struct chart_hit {
    int song_index = -1;
    int chart_index = -1;
};

struct draw_config {
    Rectangle column_rect;
    float play_t = 1.0f;
    unsigned char alpha = 255;
    Color button_base{};
    Color button_selected{};
    unsigned char normal_row_alpha = 255;
    unsigned char hover_row_alpha = 255;
    unsigned char selected_row_alpha = 255;
    bool expanded_cards = false;
    float embedded_chart_scroll_y = 0.0f;
    double now = 0.0;
};

float content_height(int count);
float max_scroll(Rectangle area, int count);
float content_height(const song_select::state& state);
float max_scroll(Rectangle area, const song_select::state& state);
Rectangle row_rect(Rectangle area, int index, float scroll_y);
int hit_test(Rectangle area, float scroll_y, Vector2 point, int count);
Rectangle row_rect(const song_select::state& state, Rectangle area, int index, float scroll_y);
int hit_test(const song_select::state& state, Rectangle area, float scroll_y, Vector2 point);
Rectangle selected_chart_list_rect(const song_select::state& state, Rectangle area, float scroll_y);
float max_embedded_chart_scroll(const song_select::state& state, Rectangle area, float scroll_y);
chart_hit hit_test_chart(const song_select::state& state, Rectangle area, float scroll_y, float chart_scroll_y, Vector2 point);
void draw(const song_select::state& state, const draw_config& config);

}  // namespace title_song_list_view
