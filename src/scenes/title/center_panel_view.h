#pragma once

#include <span>

#include "raylib.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_state.h"

namespace title_center_view {

struct draw_config {
    Rectangle main_column_rect;
    Rectangle jacket_rect;
    Rectangle chart_detail_rect;
    Rectangle chart_buttons_rect;
    float play_t = 1.0f;
    unsigned char alpha = 255;
    Color button_base{};
    Color button_selected{};
    unsigned char normal_row_alpha = 255;
    unsigned char hover_row_alpha = 255;
    unsigned char selected_row_alpha = 255;
    double now = 0.0;
};

float chart_list_content_height(int count);
float max_chart_scroll(Rectangle area, int count);
Rectangle chart_button_rect(Rectangle area, int index, float scroll_y);
int hit_test_chart(Rectangle area, float scroll_y, Vector2 point, int count);
void draw(const song_select::state& state,
          const song_select::preview_controller& preview_controller,
          const song_select::song_entry* song,
          const song_select::chart_option* chart,
          std::span<const song_select::chart_option* const> filtered,
          const draw_config& config);

}  // namespace title_center_view
