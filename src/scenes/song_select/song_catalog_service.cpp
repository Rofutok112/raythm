#include "song_select/song_catalog_service.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

#include "app_paths.h"
#include "chart_level_cache.h"
#include "mv/mv_storage.h"
#include "path_utils.h"
#include "player_note_offsets.h"
#include "ranking_service.h"
#include "song_loader.h"
#include "song_select/local_catalog_database.h"
#include "title/local_content_index.h"

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

std::optional<ranking_service::entry> load_best_local_entry(const std::string& chart_id) {
    if (chart_id.empty()) {
        return std::nullopt;
    }

    const ranking_service::listing listing =
        ranking_service::load_chart_ranking(chart_id, ranking_service::source::local, 1);
    if (listing.entries.empty()) {
        return std::nullopt;
    }
    return listing.entries.front();
}

std::vector<online_content::chart_identity> remote_links_for_chart(
    const local_content_index::snapshot& index,
    const std::string& local_chart_id) {
    std::vector<online_content::chart_identity> links;
    if (local_chart_id.empty()) {
        return links;
    }
    for (const local_content_index::online_chart_binding& binding : index.charts) {
        if (binding.local_chart_id != local_chart_id ||
            binding.server_url.empty() ||
            binding.remote_song_id.empty() ||
            binding.remote_chart_id.empty()) {
            continue;
        }
        links.push_back({
            .server_url = binding.server_url,
            .remote_song_id = binding.remote_song_id,
            .remote_chart_id = binding.remote_chart_id,
            .content_source = online_content::source::community,
            .remote_chart_version = binding.remote_chart_version,
        });
    }
    return links;
}

int source_sort_bucket(content_status status) {
    switch (status) {
    case content_status::official:
        return 0;
    case content_status::community:
        return 1;
    case content_status::update:
    case content_status::modified:
    case content_status::checking:
    case content_status::local:
        return 2;
    }
    return 2;
}

bool song_source_less(const song_select::song_entry& left, const song_select::song_entry& right) {
    const int left_bucket = source_sort_bucket(left.source_status);
    const int right_bucket = source_sort_bucket(right.source_status);
    if (left_bucket != right_bucket) {
        return left_bucket < right_bucket;
    }
    return left.song.meta.title < right.song.meta.title;
}

bool chart_source_less(const song_select::chart_option& left, const song_select::chart_option& right) {
    const int left_bucket = source_sort_bucket(left.source_status);
    const int right_bucket = source_sort_bucket(right.source_status);
    if (left_bucket != right_bucket) {
        return left_bucket < right_bucket;
    }
    if (left.meta.key_count != right.meta.key_count) {
        return left.meta.key_count < right.meta.key_count;
    }
    if (left.meta.level != right.meta.level) {
        return left.meta.level < right.meta.level;
    }
    return left.meta.difficulty < right.meta.difficulty;
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

catalog_data load_catalog(bool calculate_missing_levels) {
    if (!calculate_missing_levels) {
        catalog_data cached_catalog = local_catalog_database::load_cached_catalog();
        if (!cached_catalog.songs.empty()) {
            const player_chart_offset_map chart_offsets = load_player_chart_offsets();
            const local_content_index::snapshot local_index = local_content_index::load_snapshot();
            for (song_entry& song : cached_catalog.songs) {
                for (chart_option& chart : song.charts) {
                    chart.remote_links = remote_links_for_chart(local_index, chart.meta.chart_id);
                    chart.local_note_offset_ms = chart_offsets.contains(chart.meta.chart_id)
                        ? chart_offsets.at(chart.meta.chart_id)
                        : 0;
                    if (const auto best = load_best_local_entry(chart.meta.chart_id)) {
                        chart.best_local_rank = best->clear_rank();
                        chart.best_local_score = best->score;
                    }
                }
                std::sort(song.charts.begin(), song.charts.end(), chart_source_less);
            }
            std::sort(cached_catalog.songs.begin(), cached_catalog.songs.end(), song_source_less);
            return cached_catalog;
        }
    }

    catalog_data catalog;
    const player_chart_offset_map chart_offsets = load_player_chart_offsets();
    const local_content_index::snapshot local_index = local_content_index::load_snapshot();

    const song_load_result load_result = song_loader::load_all(path_utils::to_utf8(app_paths::songs_root()));
    catalog.load_errors = load_result.errors;

    const std::vector<song_data> all_songs = load_result.songs;

    catalog.songs.reserve(all_songs.size());
    for (const song_data& song : all_songs) {
        song_entry entry;
        entry.song = song;
        for (const std::string& chart_path : song.chart_paths) {
            const chart_parse_result parse_result = song_loader::load_chart(chart_path);
            if (!parse_result.success || !parse_result.data.has_value()) {
                continue;
            }

            chart_data effective_chart = *parse_result.data;
            chart_meta meta = effective_chart.meta;
            meta.song_id = song.meta.song_id;
            if (calculate_missing_levels) {
                meta.level = chart_level_cache::get_or_calculate(chart_path, effective_chart);
            } else if (const std::optional<float> cached_level = chart_level_cache::find_level(chart_path);
                       cached_level.has_value()) {
                meta.level = *cached_level;
            }
            const auto [min_bpm, max_bpm] = collect_bpm_range(effective_chart);

            const auto best_local = load_best_local_entry(meta.chart_id);
            chart_option option;
            option.path = chart_path;
            option.meta = meta;
            option.kind = content_kind::local;
            option.storage = storage_policy::plain_workspace;
            option.verification = verification_state::unchecked;
            option.status = content_status::local;
            option.source_status = content_status::local;
            option.remote_links = remote_links_for_chart(local_index, meta.chart_id);
            option.local_note_offset_ms = chart_offsets.contains(meta.chart_id) ? chart_offsets.at(meta.chart_id) : 0;
            option.best_local_rank = best_local.has_value()
                ? std::optional<rank>(best_local->clear_rank())
                : std::nullopt;
            option.best_local_score = best_local.has_value()
                ? std::optional<int>(best_local->score)
                : std::nullopt;
            option.note_count = static_cast<int>(parse_result.data->notes.size());
            option.min_bpm = min_bpm;
            option.max_bpm = max_bpm;
            entry.charts.push_back(std::move(option));
        }

        entry.kind = content_kind::local;
        entry.storage = storage_policy::plain_workspace;
        entry.verification = verification_state::unchecked;
        entry.status = content_status::local;
        entry.source_status = content_status::local;
        entry.online_identity.reset();
        std::sort(entry.charts.begin(), entry.charts.end(), chart_source_less);
        catalog.songs.push_back(std::move(entry));
    }

    std::sort(catalog.songs.begin(), catalog.songs.end(), song_source_less);

    local_catalog_database::replace_catalog(catalog.songs);
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

    std::error_code ec;
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
    local_content_index::remove_song_bindings(entry.song.meta.song_id);
    local_catalog_database::remove_song(entry.song.meta.song_id);

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
    const std::string deleted_chart_id = charts[static_cast<size_t>(chart_index)].meta.chart_id;
    local_content_index::remove_chart_bindings(deleted_chart_id);
    local_catalog_database::remove_chart(deleted_chart_id);

    result.success = true;
    result.message = "Chart deleted.";
    result.preferred_song_id = state.songs[static_cast<size_t>(song_index)].song.meta.song_id;
    result.preferred_chart_id = fallback_chart_id_after_chart_delete(state, song_index, chart_index);
    return result;
}

}  // namespace song_select
