#pragma once

namespace multiplayer_result::screen_view {

struct action_result {
    bool back_requested = false;
};

[[nodiscard]] action_result handle_input(bool returning);

void draw_frame();
void draw_ranking_header(int player_count);
[[nodiscard]] action_result draw_back_button(bool returning);

}  // namespace multiplayer_result::screen_view
