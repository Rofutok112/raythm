#pragma once

#include <string>
#include <vector>

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

struct chart_import_request {
    std::string source_path;
    std::string target_song_id;
    chart_data chart;
    bool overwrite_existing = false;
};

struct song_import_request {
    state catalog_state;
    std::string source_path;
    std::string extracted_root;
    song_data imported_song;
    bool overwrite_existing = false;
};

struct song_import_prepare_result {
    transfer_result transfer;
    std::optional<song_import_request> request;
};

struct chart_import_batch_request {
    std::vector<chart_import_request> requests;
    int overwrite_count = 0;
};

struct song_import_prepare_batch_result {
    transfer_result transfer;
    std::vector<song_import_request> requests;
    int overwrite_count = 0;
};

transfer_result export_chart_package(const state& state, int song_index, int chart_index);
transfer_result import_chart_package(const state& state, int song_index);
transfer_result export_song_package(const state& state, int song_index);
transfer_result import_song_package(const state& state);
std::optional<chart_import_request> prepare_chart_import(const state& state, transfer_result& result);
std::optional<chart_import_batch_request> prepare_chart_imports(const state& state, transfer_result& result);
std::optional<song_export_request> prepare_song_export(const state& state, int song_index);
std::optional<song_import_request> prepare_song_import(const state& state, transfer_result& result);
song_import_prepare_result prepare_song_import_from_path(const state& state, const std::string& source_path);
song_import_prepare_batch_result prepare_song_imports_from_paths(const state& state,
                                                                 const std::vector<std::string>& source_paths);
transfer_result import_chart_package(const chart_import_request& request);
transfer_result import_chart_packages(const std::vector<chart_import_request>& requests);
transfer_result export_song_package(const song_export_request& request);
transfer_result import_song_package(const song_import_request& request);
transfer_result import_song_packages(const std::vector<song_import_request>& requests);
void cleanup_song_import_request(song_import_request& request);
void cleanup_song_import_requests(std::vector<song_import_request>& requests);

}  // namespace song_select
