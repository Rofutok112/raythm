#pragma once

#include <optional>
#include <string>
#include <vector>

#include "play/play_session_types.h"

namespace multiplayer_result::score_list {

struct interaction_result {
    bool scroll_changed = false;
    float scroll_y = 0.0f;
    bool scrollbar_drag_state_changed = false;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
    std::optional<std::string> selected_score_key;
};

std::string score_key(const play_multiplayer_score_row& score);

[[nodiscard]] interaction_result handle_input(const std::vector<play_multiplayer_score_row>& scores,
                                              float current_scroll_y,
                                              bool current_scrollbar_dragging,
                                              float current_scrollbar_drag_offset);

void draw(const std::vector<play_multiplayer_score_row>& scores,
          const std::string& self_user_id,
          const std::string& selected_score_key,
          const std::string& avatar_base_url,
          float scroll_y);

}  // namespace multiplayer_result::score_list
