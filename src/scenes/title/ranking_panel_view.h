#pragma once

#include <optional>

#include "raylib.h"
#include "ranking_service.h"
#include "song_select/song_select_state.h"

namespace title_ranking_view {

struct draw_config {
    Rectangle header_rect;
    Rectangle source_local_rect;
    Rectangle source_online_rect;
    Rectangle list_rect;
    float play_t = 1.0f;
    unsigned char alpha = 255;
    Color button_base{};
    Color button_hover{};
    Color button_selected{};
    Color button_selected_hover{};
    unsigned char normal_row_alpha = 255;
    unsigned char hover_row_alpha = 255;
    unsigned char selected_row_alpha = 255;
    unsigned char selected_hover_row_alpha = 255;
};

float content_height(const ranking_service::listing& listing);
float max_scroll(Rectangle list_rect, const ranking_service::listing& listing);
std::optional<ranking_service::source> hit_test_source(const draw_config& config, Vector2 point);
void draw(const song_select::ranking_panel_state& panel, const draw_config& config);

}  // namespace title_ranking_view
