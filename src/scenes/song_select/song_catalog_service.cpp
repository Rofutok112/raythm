#include "song_select/song_catalog_service.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "app_paths.h"
#include "path_utils.h"
#include "player_note_offsets.h"
#include "song_loader.h"

namespace {

bool is_within_root(const std::filesystem::path& path, const std::filesystem::path& root) {
    std::error_code ec;
    const std::filesystem::path normalized_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return false;
    }

    const std::filesystem::path normalized_root = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        return false;
    }

    auto path_it = normalized_path.begin();
    auto root_it = normalized_root.begin();
    for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }

    return true;
}

}  // namespace

namespace song_select {

catalog_data load_catalog() {
    catalog_data catalog;
    const player_song_offset_map song_offsets = load_player_song_offsets();

    const song_load_result legacy_result = song_loader::load_all(path_utils::to_utf8(app_paths::legacy_songs_root()),
                                                                 content_source::legacy_assets);
    const song_load_result appdata_result = song_loader::load_all(path_utils::to_utf8(app_paths::songs_root()),
                                                                  content_source::app_data);
    catalog.load_errors = legacy_result.errors;
    catalog.load_errors.insert(catalog.load_errors.end(), appdata_result.errors.begin(), appdata_result.errors.end());

    std::vector<song_data> all_songs = legacy_result.songs;
    all_songs.insert(all_songs.end(), appdata_result.songs.begin(), appdata_result.songs.end());
    song_loader::attach_external_charts(path_utils::to_utf8(app_paths::charts_root()), all_songs);

    std::sort(all_songs.begin(), all_songs.end(), [](const song_data& left, const song_data& right) {
        return left.meta.title < right.meta.title;
    });

    catalog.songs.reserve(all_songs.size());
    for (const song_data& song : all_songs) {
        song_entry entry;
        entry.song = song;
        if (const auto it = song_offsets.find(song.meta.song_id); it != song_offsets.end()) {
            entry.local_note_offset_ms = it->second;
        }

        for (const std::string& chart_path : song.chart_paths) {
            const chart_parse_result parse_result = song_loader::load_chart(chart_path);
            if (!parse_result.success || !parse_result.data.has_value()) {
                continue;
            }

            const content_source chart_source = song_loader::classify_chart_path(chart_path);
            entry.charts.push_back({
                chart_path,
                parse_result.data->meta,
                chart_source,
                chart_source == content_source::app_data,
            });
        }

        std::sort(entry.charts.begin(), entry.charts.end(), [](const chart_option& left, const chart_option& right) {
            if (left.meta.key_count != right.meta.key_count) {
                return left.meta.key_count < right.meta.key_count;
            }
            if (left.meta.level != right.meta.level) {
                return left.meta.level < right.meta.level;
            }
            return left.meta.difficulty < right.meta.difficulty;
        });

        catalog.songs.push_back(std::move(entry));
    }

    return catalog;
}

delete_result delete_song(const state& state, int song_index) {
    delete_result result;
    if (song_index < 0 || song_index >= static_cast<int>(state.songs.size())) {
        result.message = "Song delete target is invalid.";
        return result;
    }

    const song_entry& entry = state.songs[static_cast<size_t>(song_index)];
    if (!entry.song.can_delete) {
        result.message = "Only AppData songs can be deleted.";
        return result;
    }

    const std::filesystem::path song_dir = path_utils::from_utf8(entry.song.directory);
    if (!is_within_root(song_dir, app_paths::songs_root())) {
        result.message = "Refused to delete a song outside AppData.";
        return result;
    }

    std::vector<std::filesystem::path> chart_paths_to_delete;
    const std::filesystem::path charts_root = app_paths::charts_root();
    if (std::filesystem::exists(charts_root) && std::filesystem::is_directory(charts_root)) {
        for (const auto& chart_entry : std::filesystem::directory_iterator(charts_root)) {
            if (!chart_entry.is_regular_file() || chart_entry.path().extension() != ".chart") {
                continue;
            }

            const chart_parse_result parse_result = song_loader::load_chart(path_utils::to_utf8(chart_entry.path()));
            if (!parse_result.success || !parse_result.data.has_value()) {
                continue;
            }

            if (parse_result.data->meta.song_id == entry.song.meta.song_id) {
                chart_paths_to_delete.push_back(chart_entry.path());
            }
        }
    }

    std::error_code ec;
    for (const auto& chart_path : chart_paths_to_delete) {
        std::filesystem::remove(chart_path, ec);
        if (ec) {
            result.message = "Failed to delete a linked chart file.";
            return result;
        }
    }

    std::filesystem::remove_all(song_dir, ec);
    if (ec) {
        result.message = "Failed to delete the song directory.";
        return result;
    }

    result.success = true;
    result.message = "Song deleted.";
    result.preferred_song_id = fallback_song_id_after_song_delete(state, song_index);
    return result;
}

delete_result delete_chart(const state& state, int song_index, int chart_index) {
    delete_result result;
    if (song_index < 0 || song_index >= static_cast<int>(state.songs.size())) {
        result.message = "Chart delete target is invalid.";
        return result;
    }

    const auto& charts = state.songs[static_cast<size_t>(song_index)].charts;
    if (chart_index < 0 || chart_index >= static_cast<int>(charts.size())) {
        result.message = "Chart delete target is invalid.";
        return result;
    }

    const chart_option& chart = charts[static_cast<size_t>(chart_index)];
    if (!chart.can_delete) {
        result.message = "Only AppData charts can be deleted.";
        return result;
    }

    const std::filesystem::path chart_path = path_utils::from_utf8(chart.path);
    if (!is_within_root(chart_path, app_paths::app_data_root())) {
        result.message = "Refused to delete a chart outside AppData.";
        return result;
    }

    std::error_code ec;
    const bool removed = std::filesystem::remove(chart_path, ec);
    if (ec || !removed) {
        result.message = "Failed to delete the chart file.";
        return result;
    }

    result.success = true;
    result.message = "Chart deleted.";
    result.preferred_song_id = state.songs[static_cast<size_t>(song_index)].song.meta.song_id;
    result.preferred_chart_id = fallback_chart_id_after_chart_delete(state, song_index, chart_index);
    return result;
}

}  // namespace song_select
