#pragma once

#include <optional>

#include "song_select/song_select_state.h"

namespace song_select {

struct list_hit {
    int song_index = -1;
    std::optional<int> chart_index;
};

std::optional<list_hit> hit_test_song_list(const state& state, Vector2 mouse);
void draw_song_list(const state& state);

}  // namespace song_select
