#include "song_select/song_catalog_service.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <system_error>

#include "app_paths.h"
#include "chart_difficulty.h"
#include "mv/mv_storage.h"
#include "path_utils.h"
#include "player_note_offsets.h"
#include "ranking_service.h"
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

bool is_chart_file_path(const std::filesystem::path& path) {
    return path.extension() == ".rchart";
}

struct chart_level_cache_entry {
    std::filesystem::file_time_type write_time{};
    float level = 0.0f;
};

float cached_chart_level(const std::string& chart_path, const chart_data& chart) {
    static std::unordered_map<std::string, chart_level_cache_entry> cache;

    std::error_code ec;
    const std::filesystem::path path = path_utils::from_utf8(chart_path);
    const auto write_time = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return chart_difficulty::calculate_level(chart);
    }

    if (const auto it = cache.find(chart_path); it != cache.end() && it->second.write_time == write_time) {
        return it->second.level;
    }

    const float level = chart_difficulty::calculate_level(chart);
    cache[chart_path] = chart_level_cache_entry{
        .write_time = write_time,
        .level = level,
    };
    return level;
}

std::optional<rank> load_best_local_rank(const std::string& chart_id) {
    if (chart_id.empty()) {
        return std::nullopt;
    }

    const ranking_service::listing listing =
        ranking_service::load_chart_ranking(chart_id, ranking_service::source::local, 1);
    if (listing.entries.empty()) {
        return std::nullopt;
    }
    return listing.entries.front().clear_rank();
}

std::pair<float, float> collect_bpm_range(const chart_data& chart) {
    float min_bpm = 0.0f;
    float max_bpm = 0.0f;
    bool found = false;

    for (const timing_event& event : chart.timing_events) {
        if (event.type != timing_event_type::bpm || event.bpm <= 0.0f) {
            continue;
        }
        if (!found) {
            min_bpm = event.bpm;
            max_bpm = event.bpm;
            found = true;
            continue;
        }
        min_bpm = std::min(min_bpm, event.bpm);
        max_bpm = std::max(max_bpm, event.bpm);
    }

    return found ? std::pair<float, float>{min_bpm, max_bpm}
                 : std::pair<float, float>{0.0f, 0.0f};
}

}  // namespace

namespace song_select {

catalog_data load_catalog() {
    catalog_data catalog;
    const player_chart_offset_map chart_offsets = load_player_chart_offsets();

    const song_load_result load_result = song_loader::load_all(path_utils::to_utf8(app_paths::songs_root()));
    catalog.load_errors = load_result.errors;

    std::vector<song_data> all_songs = load_result.songs;
    song_loader::attach_external_charts(path_utils::to_utf8(app_paths::charts_root()), all_songs);

    std::sort(all_songs.begin(), all_songs.end(), [](const song_data& left, const song_data& right) {
        return left.meta.title < right.meta.title;
    });

    catalog.songs.reserve(all_songs.size());
    for (const song_data& song : all_songs) {
        song_entry entry;
        entry.song = song;
        for (const std::string& chart_path : song.chart_paths) {
            const chart_parse_result parse_result = song_loader::load_chart(chart_path);
            if (!parse_result.success || !parse_result.data.has_value()) {
                continue;
            }

            chart_meta meta = parse_result.data->meta;
            meta.level = cached_chart_level(chart_path, *parse_result.data);
            const auto [min_bpm, max_bpm] = collect_bpm_range(*parse_result.data);

            entry.charts.push_back({
                chart_path,
                meta,
                chart_offsets.contains(meta.chart_id) ? chart_offsets.at(meta.chart_id) : 0,
                load_best_local_rank(meta.chart_id),
                static_cast<int>(parse_result.data->notes.size()),
                min_bpm,
                max_bpm,
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
    const std::filesystem::path song_dir = path_utils::from_utf8(entry.song.directory);
    if (!is_within_root(song_dir, app_paths::songs_root())) {
        result.message = "Refused to delete a song outside the user songs directory.";
        return result;
    }

    std::vector<std::filesystem::path> chart_paths_to_delete;
    const std::filesystem::path charts_root = app_paths::charts_root();
    if (std::filesystem::exists(charts_root) && std::filesystem::is_directory(charts_root)) {
        for (const auto& chart_entry : std::filesystem::directory_iterator(charts_root)) {
            if (!chart_entry.is_regular_file() || !is_chart_file_path(chart_entry.path())) {
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

    for (const auto& package : mv::load_all_packages()) {
        if (package.meta.song_id != entry.song.meta.song_id) {
            continue;
        }

        std::filesystem::remove_all(path_utils::from_utf8(package.directory), ec);
        if (ec) {
            result.message = "Failed to delete a linked MV package.";
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

    const std::filesystem::path chart_path = path_utils::from_utf8(charts[static_cast<size_t>(chart_index)].path);
    if (!is_within_root(chart_path, app_paths::app_data_root())) {
        result.message = "Refused to delete a chart outside the user charts directory.";
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
