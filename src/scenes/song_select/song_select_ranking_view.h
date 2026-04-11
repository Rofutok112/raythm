#pragma once

#include "song_select/song_select_state.h"

namespace song_select {

struct ranking_panel_result {
    bool source_dropdown_toggled = false;
    int source_clicked_index = -1;
    bool source_dropdown_close_requested = false;
};

ranking_panel_result draw_ranking_panel(const state& state, bool source_dropdown_interactive = true);
float ranking_content_height(const state& state);

}  // namespace song_select
