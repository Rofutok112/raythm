#pragma once

#include <string>

#include "data_models.h"
#include "play/play_session_types.h"
#include "raylib.h"

namespace multiplayer_result::summary_view {

void draw(const song_data& song,
          const chart_meta& chart,
          int key_count,
          const play_multiplayer_score_row& self_score,
          int self_place,
          Texture2D jacket_texture,
          bool jacket_texture_loaded,
          const std::string& avatar_base_url);

}  // namespace multiplayer_result::summary_view
