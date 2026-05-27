#include "song_select/song_catalog_service.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

#include "app_paths.h"
#include "chart_level_cache.h"
#include "managed_content_storage.h"
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
            .can_edit = binding.can_edit,
            .lifecycle_status = binding.lifecycle_status,
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

content_kind kind_for_source(online_content::source source) {
    return source == online_content::source::official ? content_kind::official : content_kind::community;
}

content_status status_for_source(online_content::source source) {
    return source == online_content::source::official ? content_status::official : content_status::community;
}

const managed_content_storage::chart_manifest_entry* find_manifest_chart(
    const managed_content_storage::package_manifest& manifest,
    const std::string& local_chart_id) {
    for (const managed_content_storage::chart_manifest_entry& chart : manifest.charts) {
        if (chart.local_chart_id == local_chart_id) {
            return &chart;
        }
    }
    return nullptr;
}

std::optional<online_content::song_identity> managed_song_identity(
    const managed_content_storage::package_manifest& manifest,
    const local_content_index::snapshot& index) {
    if (manifest.song.server_url.empty() || manifest.song.remote_song_id.empty()) {
        return std::nullopt;
    }

    std::optional<local_content_index::online_song_binding> binding =
        local_content_index::find_song_by_local(index, manifest.song.server_url, manifest.local_song_id);
    return online_content::song_identity{
        .server_url = manifest.song.server_url,
        .remote_song_id = manifest.song.remote_song_id,
        .content_source = manifest.song.source,
        .can_edit = binding.has_value() ? binding->can_edit : std::nullopt,
        .lifecycle_status = binding.has_value() ? binding->lifecycle_status : "",
    };
}

std::optional<online_content::chart_identity> managed_chart_identity(
    const managed_content_storage::package_manifest& manifest,
    const managed_content_storage::chart_manifest_entry& chart,
    const local_content_index::snapshot& index) {
    if (manifest.song.server_url.empty() || manifest.song.remote_song_id.empty() ||
        chart.remote_chart_id.empty() || chart.local_chart_id.empty()) {
        return std::nullopt;
    }

    std::optional<local_content_index::online_chart_binding> binding =
        local_content_index::find_chart_by_local(index, manifest.song.server_url, chart.local_chart_id);
    return online_content::chart_identity{
        .server_url = manifest.song.server_url,
        .remote_song_id = manifest.song.remote_song_id,
        .remote_chart_id = chart.remote_chart_id,
        .content_source = manifest.song.source,
        .remote_chart_version = chart.chart_version,
        .can_edit = binding.has_value() ? binding->can_edit : std::nullopt,
        .lifecycle_status = binding.has_value() ? binding->lifecycle_status : "",
    };
}

song_select::managed_song_manifest_metadata managed_song_metadata(
    const managed_content_storage::package_manifest& manifest) {
    return {
        .package_id = manifest.song.package_id,
        .song_json_hash = manifest.song_json_hash,
        .song_json_fingerprint = manifest.song_json_fingerprint,
        .audio_hash = manifest.audio_hash,
        .jacket_hash = manifest.jacket_hash,
        .remote_song_json_hash = manifest.remote_song_json_hash,
        .remote_song_json_fingerprint = manifest.remote_song_json_fingerprint,
        .remote_audio_hash = manifest.remote_audio_hash,
        .remote_jacket_hash = manifest.remote_jacket_hash,
        .created_at = manifest.created_at,
        .updated_at = manifest.updated_at,
    };
}

song_select::managed_chart_manifest_metadata managed_chart_metadata(
    const managed_content_storage::chart_manifest_entry& chart) {
    return {
        .chart_hash = chart.chart_hash,
        .chart_fingerprint = chart.chart_fingerprint,
        .remote_chart_hash = chart.remote_chart_hash,
        .remote_chart_fingerprint = chart.remote_chart_fingerprint,
        .revision_id = chart.revision_id,
    };
}

void append_loaded_song(song_select::catalog_data& catalog,
                        const song_data& song,
                        const player_chart_offset_map& chart_offsets,
                        const local_content_index::snapshot& local_index,
                        bool calculate_missing_levels,
                        std::optional<managed_content_storage::package_manifest> managed_manifest) {
    song_select::song_entry entry;
    entry.song = song;
    const bool managed = managed_manifest.has_value();
    if (managed) {
        entry.kind = kind_for_source(managed_manifest->song.source);
        entry.storage = storage_policy::managed_package;
        entry.verification = verification_state::unchecked;
        entry.status = status_for_source(managed_manifest->song.source);
        entry.source_status = entry.status;
        entry.online_identity = managed_song_identity(*managed_manifest, local_index);
        entry.managed_manifest = managed_song_metadata(*managed_manifest);
    } else {
        entry.kind = content_kind::local;
        entry.storage = storage_policy::plain_workspace;
        entry.verification = verification_state::unchecked;
        entry.status = content_status::local;
        entry.source_status = content_status::local;
        entry.online_identity.reset();
        entry.managed_manifest.reset();
    }

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
        song_select::chart_option option;
        option.path = chart_path;
        option.meta = meta;
        option.kind = managed ? kind_for_source(managed_manifest->song.source) : content_kind::local;
        option.storage = managed ? storage_policy::managed_package : storage_policy::plain_workspace;
        option.verification = verification_state::unchecked;
        option.status = managed ? status_for_source(managed_manifest->song.source) : content_status::local;
        option.source_status = option.status;
        if (managed) {
            if (const managed_content_storage::chart_manifest_entry* chart =
                    find_manifest_chart(*managed_manifest, meta.chart_id)) {
                option.online_identity = managed_chart_identity(*managed_manifest, *chart, local_index);
                option.managed_manifest = managed_chart_metadata(*chart);
                if (option.online_identity.has_value()) {
                    option.remote_links.push_back(*option.online_identity);
                }
                option.meta.chart_version = chart->chart_version;
            }
        } else {
            option.remote_links = remote_links_for_chart(local_index, meta.chart_id);
        }
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

    std::sort(entry.charts.begin(), entry.charts.end(), chart_source_less);
    catalog.songs.push_back(std::move(entry));
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
                    if (chart.online_identity.has_value()) {
                        chart.remote_links = {*chart.online_identity};
                    } else {
                        chart.remote_links = remote_links_for_chart(local_index, chart.meta.chart_id);
                    }
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

    catalog.songs.reserve(load_result.songs.size());
    for (const song_data& song : load_result.songs) {
        append_loaded_song(catalog, song, chart_offsets, local_index, calculate_missing_levels, std::nullopt);
    }

    for (const online_content::source source : {online_content::source::community,
                                                online_content::source::official}) {
        for (const std::filesystem::path& package_dir :
             managed_content_storage::list_package_directories(source)) {
            const std::optional<managed_content_storage::package_manifest> manifest =
                managed_content_storage::read_manifest(package_dir);
            if (!manifest.has_value() || manifest->song.source != source) {
                continue;
            }

            const song_load_result managed_load =
                song_loader::load_directory(path_utils::to_utf8(package_dir));
            catalog.load_errors.insert(catalog.load_errors.end(),
                                       managed_load.errors.begin(),
                                       managed_load.errors.end());
            if (managed_load.songs.empty()) {
                continue;
            }
            append_loaded_song(catalog,
                               managed_load.songs.front(),
                               chart_offsets,
                               local_index,
                               calculate_missing_levels,
                               manifest);
        }
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
    const bool managed = entry.storage == storage_policy::managed_package;
    const std::filesystem::path allowed_root = managed ? app_paths::content_cache_root() : app_paths::songs_root();
    if (!is_within_root(song_dir, allowed_root)) {
        result.message = managed
            ? "Refused to delete a managed song outside the content cache."
            : "Refused to delete a song outside the user songs directory.";
        return result;
    }

    std::error_code ec;
    if (!managed) {
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
    }

    std::filesystem::remove_all(song_dir, ec);
    if (ec) {
        result.message = "Failed to delete the song directory.";
        return result;
    }
    local_content_index::remove_song_bindings(entry.song.meta.song_id);
    for (const chart_option& chart : entry.charts) {
        local_content_index::remove_chart_bindings(chart.meta.chart_id);
    }
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

    const chart_option& chart = charts[static_cast<size_t>(chart_index)];
    const std::filesystem::path chart_path = path_utils::from_utf8(chart.path);
    const bool managed = chart.storage == storage_policy::managed_package;
    const std::filesystem::path allowed_root = managed ? app_paths::content_cache_root() : app_paths::songs_root();
    if (!is_within_root(chart_path, allowed_root)) {
        result.message = managed
            ? "Refused to delete a managed chart outside the content cache."
            : "Refused to delete a chart outside the user songs directory.";
        return result;
    }

    std::error_code ec;
    const bool removed = std::filesystem::remove(chart_path, ec);
    if (ec || !removed) {
        result.message = "Failed to delete the chart file.";
        return result;
    }
    const std::string deleted_chart_id = chart.meta.chart_id;
    local_content_index::remove_chart_bindings(deleted_chart_id);
    local_catalog_database::remove_chart(deleted_chart_id);

    result.success = true;
    result.message = "Chart deleted.";
    result.preferred_song_id = state.songs[static_cast<size_t>(song_index)].song.meta.song_id;
    result.preferred_chart_id = fallback_chart_id_after_chart_delete(state, song_index, chart_index);
    return result;
}

}  // namespace song_select
