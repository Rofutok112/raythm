#include "song_select/local_catalog_cache_service.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "app_paths.h"
#include "local_catalog_signature.h"
#include "path_utils.h"
#include "player_note_offsets.h"
#include "ranking_service.h"
#include "song_select/local_catalog_database.h"
#include "title/local_content_index.h"

namespace song_select::local_catalog_cache_service {
namespace {

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

bool song_source_less(const song_entry& left, const song_entry& right) {
    const int left_bucket = source_sort_bucket(left.source_status);
    const int right_bucket = source_sort_bucket(right.source_status);
    if (left_bucket != right_bucket) {
        return left_bucket < right_bucket;
    }
    return left.song.meta.title < right.song.meta.title;
}

bool chart_source_less(const chart_option& left, const chart_option& right) {
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

bool encrypted_asset_exists(const std::filesystem::path& song_dir,
                            const managed_content_storage::encrypted_asset_metadata& asset) {
    if (asset.encrypted_path.empty()) {
        return true;
    }

    std::error_code ec;
    const std::filesystem::path encrypted_path =
        managed_content_storage::encrypted_asset_path(song_dir, asset);
    return std::filesystem::exists(encrypted_path, ec) &&
           std::filesystem::is_regular_file(encrypted_path, ec);
}

bool cached_managed_chart_available(
    const song_entry& song,
    const chart_option& chart,
    const std::optional<managed_content_storage::package_manifest>& manifest) {
    if (chart.storage != storage_policy::managed_package) {
        return true;
    }
    if (!manifest.has_value()) {
        return false;
    }

    const managed_content_storage::chart_manifest_entry* manifest_chart =
        find_manifest_chart(*manifest, chart.meta.chart_id);
    if (manifest_chart == nullptr) {
        return false;
    }
    const std::filesystem::path song_dir = path_utils::from_utf8(song.song.directory);
    if (!encrypted_asset_exists(song_dir, manifest_chart->encrypted_chart)) {
        return false;
    }
    if (!manifest_chart->encrypted_chart.encrypted_path.empty()) {
        return true;
    }

    std::error_code ec;
    const std::filesystem::path chart_path = path_utils::from_utf8(chart.path);
    return std::filesystem::exists(chart_path, ec) &&
           std::filesystem::is_regular_file(chart_path, ec);
}

bool cached_managed_song_available(const song_entry& song) {
    const managed_content_storage::managed_file_read_result metadata =
        managed_content_storage::read_managed_file(
            path_utils::join_utf8(song.song.directory, "song.json"));
    std::error_code ec;
    const std::filesystem::path song_json_path =
        path_utils::join_utf8(song.song.directory, "song.json");
    return (!metadata.managed || metadata.success) &&
           (metadata.managed || std::filesystem::is_regular_file(song_json_path, ec));
}

struct cache_repair_result {
    bool changed = false;
    bool remove_song = false;
};

bool sanitize_plain_workspace_song(song_entry& song) {
    if (song.storage != storage_policy::plain_workspace) {
        return false;
    }

    bool changed = song.kind != content_kind::local ||
                   song.status != content_status::local ||
                   song.source_status != content_status::local ||
                   song.online_identity.has_value() ||
                   song.managed_manifest.has_value();
    song.kind = content_kind::local;
    song.status = content_status::local;
    song.source_status = content_status::local;
    song.online_identity.reset();
    song.managed_manifest.reset();
    return changed;
}

bool sanitize_plain_workspace_chart(chart_option& chart) {
    if (chart.storage != storage_policy::plain_workspace) {
        return false;
    }

    bool changed = chart.kind != content_kind::local ||
                   chart.status != content_status::local ||
                   chart.source_status != content_status::local ||
                   chart.online_identity.has_value() ||
                   chart.managed_manifest.has_value() ||
                   !chart.remote_links.empty();
    chart.kind = content_kind::local;
    chart.status = content_status::local;
    chart.source_status = content_status::local;
    chart.online_identity.reset();
    chart.managed_manifest.reset();
    chart.remote_links.clear();
    return changed;
}

cache_repair_result repair_cached_song(song_entry& song, const player_chart_offset_map& chart_offsets) {
    cache_repair_result result;
    std::optional<managed_content_storage::package_manifest> manifest;
    if (song.storage == storage_policy::managed_package) {
        manifest = managed_content_storage::read_manifest(path_utils::from_utf8(song.song.directory));
        if (!manifest.has_value() || !cached_managed_song_available(song)) {
            result.changed = true;
            result.remove_song = true;
            return result;
        }
        sync_managed_manifest_identity(*manifest);
    }

    result.changed = sanitize_plain_workspace_song(song) || result.changed;
    for (auto chart_it = song.charts.begin(); chart_it != song.charts.end();) {
        chart_option& chart = *chart_it;
        if (!cached_managed_chart_available(song, chart, manifest)) {
            chart_it = song.charts.erase(chart_it);
            result.changed = true;
            continue;
        }

        result.changed = sanitize_plain_workspace_chart(chart) || result.changed;
        if (chart.online_identity.has_value()) {
            chart.remote_links = {*chart.online_identity};
        } else {
            chart.remote_links.clear();
        }
        chart.local_note_offset_ms = chart_offsets.contains(chart.meta.chart_id)
            ? chart_offsets.at(chart.meta.chart_id)
            : 0;
        if (const auto best = load_best_local_entry(chart.meta.chart_id)) {
            chart.best_local_rank = best->clear_rank();
            chart.best_local_score = best->score;
        } else {
            chart.best_local_rank.reset();
            chart.best_local_score.reset();
        }
        ++chart_it;
    }
    std::sort(song.charts.begin(), song.charts.end(), chart_source_less);
    if (song.storage == storage_policy::managed_package && song.charts.empty()) {
        result.remove_song = true;
        result.changed = true;
    }
    return result;
}

}  // namespace

void sync_managed_manifest_identity(const managed_content_storage::package_manifest& manifest) {
    if (manifest.song.server_url.empty() ||
        manifest.local_song_id.empty() ||
        manifest.song.remote_song_id.empty()) {
        return;
    }

    local_content_index::online_song_binding song_binding{
        .server_url = manifest.song.server_url,
        .local_song_id = manifest.local_song_id,
        .remote_song_id = manifest.song.remote_song_id,
        .origin = local_content_index::online_origin::downloaded,
    };
    const std::optional<local_content_index::online_song_binding> existing_song =
        local_content_index::find_song_by_local(song_binding.server_url, song_binding.local_song_id);
    if (!existing_song.has_value() ||
        existing_song->remote_song_id != song_binding.remote_song_id) {
        local_content_index::put_song_binding(song_binding);
    }

    for (const managed_content_storage::chart_manifest_entry& chart : manifest.charts) {
        if (chart.local_chart_id.empty() || chart.remote_chart_id.empty()) {
            continue;
        }
        local_content_index::online_chart_binding chart_binding{
            .server_url = manifest.song.server_url,
            .local_chart_id = chart.local_chart_id,
            .remote_chart_id = chart.remote_chart_id,
            .remote_song_id = manifest.song.remote_song_id,
            .remote_chart_version = chart.chart_version,
            .origin = local_content_index::online_origin::downloaded,
        };
        const std::optional<local_content_index::online_chart_binding> existing_chart =
            local_content_index::find_chart_by_local(chart_binding.server_url, chart_binding.local_chart_id);
        if (!existing_chart.has_value() ||
            existing_chart->remote_chart_id != chart_binding.remote_chart_id ||
            existing_chart->remote_song_id != chart_binding.remote_song_id ||
            existing_chart->remote_chart_version != chart_binding.remote_chart_version) {
            local_content_index::put_chart_binding(chart_binding);
        }
    }
}

std::optional<catalog_data> load_ready_catalog() {
    const refresh_guard guard = capture_refresh_guard();
    catalog_data catalog = local_catalog_database::load_cached_catalog();
    if (catalog.songs.empty()) {
        return std::nullopt;
    }

    const player_chart_offset_map chart_offsets = load_player_chart_offsets();
    bool changed = false;
    for (auto song_it = catalog.songs.begin(); song_it != catalog.songs.end();) {
        const cache_repair_result repair = repair_cached_song(*song_it, chart_offsets);
        if (repair.remove_song) {
            song_it = catalog.songs.erase(song_it);
            changed = true;
            continue;
        }
        changed = repair.changed || changed;
        ++song_it;
    }

    std::sort(catalog.songs.begin(), catalog.songs.end(), song_source_less);
    if (catalog.songs.empty()) {
        return std::nullopt;
    }
    if (changed) {
        replace_if_unchanged(guard, catalog.songs);
    }
    return catalog;
}

refresh_guard capture_refresh_guard() {
    app_paths::ensure_directories();
    return {.signature = local_catalog_signature::current()};
}

void replace_if_unchanged(const refresh_guard& guard, const std::vector<song_entry>& songs) {
    if (guard.signature.empty()) {
        return;
    }

    const std::string current_signature = local_catalog_signature::current();
    if (current_signature == guard.signature) {
        local_catalog_database::replace_catalog(songs, current_signature);
    }
}

}  // namespace song_select::local_catalog_cache_service
