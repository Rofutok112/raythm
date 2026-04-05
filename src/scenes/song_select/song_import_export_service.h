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

struct song_export_request {
    song_entry song;
    std::string save_path;
};

struct song_import_request {
    state catalog_state;
    std::string source_path;
};

transfer_result export_chart_package(const state& state, int song_index, int chart_index);
transfer_result import_chart_package(const state& state, int song_index);
transfer_result export_song_package(const state& state, int song_index);
transfer_result import_song_package(const state& state);
std::optional<song_export_request> prepare_song_export(const state& state, int song_index);
std::optional<song_import_request> prepare_song_import(const state& state);
transfer_result export_song_package(const song_export_request& request);
transfer_result import_song_package(const song_import_request& request);

}  // namespace song_select
