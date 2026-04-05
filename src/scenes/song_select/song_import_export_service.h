#pragma once

#include <string>

#include "song_select/song_select_state.h"

namespace song_select {

struct transfer_result {
    bool success = false;
    bool cancelled = false;
    bool reload_catalog = false;
    std::string message;
    std::string preferred_song_id;
    std::string preferred_chart_id;
};

transfer_result export_chart_package(const state& state, int song_index, int chart_index);
transfer_result import_chart_package(const state& state, int song_index);
transfer_result export_song_package(const state& state, int song_index);
transfer_result import_song_package(const state& state);

}  // namespace song_select
