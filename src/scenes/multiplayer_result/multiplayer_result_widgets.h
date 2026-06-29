#pragma once

#include <string>

#include "raylib.h"

namespace multiplayer_result::widgets {

std::string avatar_label_for(const std::string& name);

void draw_profile_image_slot(Rectangle rect,
                             const std::string& avatar_url,
                             const std::string& display_name,
                             const std::string& base_url,
                             int font_size = 13);

std::string format_score(int score);

Color podium_rank_color(int rank_index);

}  // namespace multiplayer_result::widgets
