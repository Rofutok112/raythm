#pragma once

#include <string>

#include "song_select/song_select_state.h"

namespace song_select {

struct delete_result {
    bool success = false;
    std::string message;
    std::string preferred_song_id;
    std::string preferred_chart_id;
};

catalog_data load_catalog(bool calculate_missing_levels = false);
delete_result delete_song(const state& state, int song_index);
delete_result delete_chart(const state& state, int song_index, int chart_index);

}  // namespace song_select
