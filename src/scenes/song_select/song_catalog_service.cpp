#include "song_select/song_catalog_service.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <set>
#include <system_error>
#include <utility>
#include <vector>

#include "app_paths.h"
#include "content_cache_paths.h"
#include "services/content_sync_service.h"
#include "managed_content_storage.h"
#include "mv/mv_storage.h"
#include "path_utils.h"
#include "player_note_offsets.h"
#include "song_loader.h"
#include "song_select/local_catalog_cache_service.h"
#include "song_select/local_catalog_database.h"
#include "title/local_content_database.h"
#include "title/local_content_index.h"

namespace {

void set_catalog_progress(const song_select::catalog_progress_callback& progress,
                          std::string message,
                          float value,
                          bool active = true) {
    if (progress) {
        progress(std::move(message), value, active);
    }
}

class catalog_progress_scope {
public:
    explicit catalog_progress_scope(const song_select::catalog_progress_callback& progress)
        : progress_(progress) {
        set_catalog_progress(progress_, "Preparing local catalog...", 0.02f);
    }

    ~catalog_progress_scope() {
        set_catalog_progress(progress_, "Local catalog ready.", 1.0f, false);
    }

private:
    const song_select::catalog_progress_callback& progress_;
};

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

content_source content_source_for_online_source(online_content::source source) {
    return source == online_content::source::official ? content_source::official : content_source::community;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool hash_changed(const std::string& local_hash, const std::string& remote_hash) {
    return !local_hash.empty() &&
           !remote_hash.empty() &&
           lowercase(local_hash) != lowercase(remote_hash);
}

bool managed_package_exists_for_local_song_id(const std::string& local_song_id) {
    for (const online_content::source source : {online_content::source::community,
                                                online_content::source::official}) {
        const std::filesystem::path package_dir =
            content_cache_paths::source_root(source) / "songs" / local_song_id;
        if (managed_content_storage::read_manifest(package_dir).has_value()) {
            return true;
        }
    }
    return false;
}

void remove_orphaned_managed_workspace_shadow_dirs() {
    const std::filesystem::path songs_root = app_paths::songs_root();
    if (!std::filesystem::exists(songs_root) || !std::filesystem::is_directory(songs_root)) {
        return;
    }

    std::error_code ec;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(songs_root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_directory(ec)) {
            continue;
        }
        const std::filesystem::path song_dir = entry.path();
        if (std::filesystem::exists(song_dir / "song.json", ec)) {
            continue;
        }
        ec.clear();

        const std::string local_song_id = path_utils::to_utf8(song_dir.filename());
        if (local_song_id.find("song_") != 0 || !managed_package_exists_for_local_song_id(local_song_id)) {
            continue;
        }

        std::filesystem::remove_all(song_dir, ec);
        ec.clear();
    }
}

bool managed_chart_modified(const managed_content_storage::chart_manifest_entry& chart) {
    if (!chart.remote_chart_fingerprint.empty()) {
        return hash_changed(chart.chart_fingerprint, chart.remote_chart_fingerprint);
    }
    return hash_changed(chart.chart_hash, chart.remote_chart_hash);
}

bool managed_song_modified(const managed_content_storage::package_manifest& manifest) {
    if (!manifest.remote_song_json_fingerprint.empty() &&
        hash_changed(manifest.song_json_fingerprint, manifest.remote_song_json_fingerprint)) {
        return true;
    }
    if (manifest.remote_song_json_fingerprint.empty() &&
        hash_changed(manifest.song_json_hash, manifest.remote_song_json_hash)) {
        return true;
    }
    return hash_changed(manifest.audio_hash, manifest.remote_audio_hash) ||
           hash_changed(manifest.jacket_hash, manifest.remote_jacket_hash);
}

content_status status_for_managed_song(online_content::source source,
                                       const managed_content_storage::package_manifest& manifest) {
    if (managed_song_modified(manifest)) {
        return content_status::modified;
    }
    return status_for_source(source);
}

content_status status_for_managed_chart(online_content::source source,
                                        const managed_content_storage::chart_manifest_entry* chart) {
    if (chart != nullptr && managed_chart_modified(*chart)) {
        return content_status::modified;
    }
    return status_for_source(source);
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

    (void)index;
    const std::optional<local_content_database::remote_metadata> metadata =
        local_content_database::find_remote_metadata(local_content_database::remote_content_type::song,
                                                     manifest.song.server_url,
                                                     manifest.song.remote_song_id);
    return online_content::song_identity{
        .server_url = manifest.song.server_url,
        .remote_song_id = manifest.song.remote_song_id,
        .content_source = manifest.song.source,
        .lifecycle_status = metadata.has_value() ? metadata->lifecycle_status : "",
        .review_status = metadata.has_value() ? metadata->review_status : "",
    };
}

std::optional<online_content::source> remote_song_source_override(
    const managed_content_storage::package_manifest& manifest) {
    const std::optional<local_content_database::remote_metadata> metadata =
        local_content_database::find_remote_metadata(local_content_database::remote_content_type::song,
                                                     manifest.song.server_url,
                                                     manifest.song.remote_song_id);
    if (!metadata.has_value() || metadata->content_source.empty()) {
        return std::nullopt;
    }
    return online_content::source_from_string(metadata->content_source);
}

std::optional<online_content::chart_identity> managed_chart_identity(
    const managed_content_storage::package_manifest& manifest,
    const managed_content_storage::chart_manifest_entry& chart,
    const local_content_index::snapshot& index) {
    if (manifest.song.server_url.empty() || manifest.song.remote_song_id.empty() ||
        chart.remote_chart_id.empty() || chart.local_chart_id.empty()) {
        return std::nullopt;
    }

    (void)index;
    const std::optional<local_content_database::remote_metadata> metadata =
        local_content_database::find_remote_metadata(local_content_database::remote_content_type::chart,
                                                     manifest.song.server_url,
                                                     chart.remote_chart_id);
    return online_content::chart_identity{
        .server_url = manifest.song.server_url,
        .remote_song_id = manifest.song.remote_song_id,
        .remote_chart_id = chart.remote_chart_id,
        .content_source = manifest.song.source,
        .remote_chart_version = chart.chart_version,
        .lifecycle_status = metadata.has_value() ? metadata->lifecycle_status : "",
        .review_status = metadata.has_value() ? metadata->review_status : "",
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

std::string managed_level_signature(const song_select::chart_option& chart) {
    if (chart.storage != storage_policy::managed_package || !chart.managed_manifest.has_value()) {
        return {};
    }
    return !chart.managed_manifest->chart_hash.empty()
        ? chart.managed_manifest->chart_hash
        : chart.managed_manifest->chart_fingerprint;
}

std::string managed_level_signature(const managed_content_storage::chart_manifest_entry* chart) {
    if (chart == nullptr) {
        return {};
    }
    return !chart->chart_hash.empty() ? chart->chart_hash : chart->chart_fingerprint;
}

float calculate_level_for_chart(const std::string& chart_path,
                                const chart_data& chart,
                                const std::string& content_signature) {
    return content_signature.empty()
        ? song_select::local_catalog_cache_service::get_or_calculate_chart_level(chart_path, chart)
        : song_select::local_catalog_cache_service::get_or_calculate_chart_level(
              chart_path, content_signature, chart);
}

std::optional<float> cached_level_for_chart(const std::string& chart_path,
                                            const std::string& content_signature) {
    return content_signature.empty()
        ? song_select::local_catalog_cache_service::find_chart_level(chart_path)
        : song_select::local_catalog_cache_service::find_chart_level(chart_path, content_signature);
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
        entry.status = status_for_managed_song(managed_manifest->song.source, *managed_manifest);
        entry.source_status = status_for_source(managed_manifest->song.source);
        entry.source = content_source_for_online_source(managed_manifest->song.source);
        entry.sync_state = content_sync_service::sync_from_status(entry.status);
        entry.online_identity = managed_song_identity(*managed_manifest, local_index);
        entry.managed_manifest = managed_song_metadata(*managed_manifest);
    } else {
        entry.kind = content_kind::local;
        entry.storage = storage_policy::plain_workspace;
        entry.verification = verification_state::unchecked;
        entry.status = content_status::local;
        entry.source_status = content_status::local;
        entry.source = content_source::local;
        entry.sync_state = content_sync_state::clean;
        entry.online_identity.reset();
        entry.managed_manifest.reset();
    }

    for (const std::string& chart_path : song.chart_paths) {
        const chart_parse_result parse_result = song_loader::load_chart(chart_path);
        if (!parse_result.success || !parse_result.data.has_value()) {
            if (parse_result.errors.empty()) {
                catalog.load_errors.push_back("Failed to load chart: " + chart_path);
            } else {
                for (const std::string& error : parse_result.errors) {
                    catalog.load_errors.push_back(chart_path + ": " + error);
                }
            }
            continue;
        }

        chart_data effective_chart = *parse_result.data;
        chart_meta meta = effective_chart.meta;
        meta.song_id = song.meta.song_id;
        const managed_content_storage::chart_manifest_entry* managed_chart = managed
            ? find_manifest_chart(*managed_manifest, meta.chart_id)
            : nullptr;
        const bool managed_remote_chart = managed && managed_chart != nullptr;
        const std::string level_signature = managed_level_signature(managed_chart);
        if (calculate_missing_levels) {
            meta.level = calculate_level_for_chart(chart_path, effective_chart, level_signature);
        } else if (const std::optional<float> cached_level = cached_level_for_chart(chart_path, level_signature);
                   cached_level.has_value()) {
            meta.level = *cached_level;
        }
        const auto [min_bpm, max_bpm] = collect_bpm_range(effective_chart);

        song_select::chart_option option;
        option.path = chart_path;
        option.meta = meta;
        option.kind = managed_remote_chart ? kind_for_source(managed_manifest->song.source) : content_kind::local;
        option.storage = managed_remote_chart ? storage_policy::managed_package : storage_policy::plain_workspace;
        option.verification = verification_state::unchecked;
        option.status = managed_remote_chart ? status_for_managed_chart(managed_manifest->song.source, managed_chart)
                                             : content_status::local;
        option.source_status = managed_remote_chart ? status_for_source(managed_manifest->song.source) : content_status::local;
        const content_sync_service::state option_state =
            content_sync_service::state_from_legacy_status(option.source_status, option.status);
        option.source = option_state.source;
        option.sync_state = option_state.sync;
        if (managed_remote_chart) {
            option.online_identity = managed_chart_identity(*managed_manifest, *managed_chart, local_index);
            option.managed_manifest = managed_chart_metadata(*managed_chart);
            if (option.online_identity.has_value()) {
                option.remote_links.push_back(*option.online_identity);
            }
            option.meta.chart_version = managed_chart->chart_version;
        }
        option.local_note_offset_ms = chart_offsets.contains(meta.chart_id) ? chart_offsets.at(meta.chart_id) : 0;
        option.best_local_rank.reset();
        option.best_local_score.reset();
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

catalog_data load_catalog(bool calculate_missing_levels, catalog_progress_callback progress) {
    catalog_progress_scope progress_scope(progress);
    if (!calculate_missing_levels) {
        set_catalog_progress(progress, "Checking local catalog cache...", 0.05f);
        if (std::optional<catalog_data> cached_catalog =
                local_catalog_cache_service::load_ready_catalog(
                    [&progress](std::string message, float cache_progress) {
                        set_catalog_progress(
                            progress,
                            std::move(message),
                            0.05f + 0.91f * std::clamp(cache_progress, 0.0f, 1.0f));
                    })) {
            set_catalog_progress(progress, "Using cached catalog...", 0.96f);
            return *cached_catalog;
        }
    }

    set_catalog_progress(progress, "Reading saved chart settings...", 0.08f);
    const local_catalog_cache_service::refresh_guard cache_guard =
        local_catalog_cache_service::capture_refresh_guard();
    catalog_data catalog;
    const player_chart_offset_map chart_offsets = load_player_chart_offsets();

    set_catalog_progress(progress, "Reading online content links...", 0.12f);
    const local_content_index::snapshot local_index = local_content_index::load_snapshot();

    if (calculate_missing_levels) {
        set_catalog_progress(progress, "Cleaning old content cache...", 0.16f);
        remove_orphaned_managed_workspace_shadow_dirs();
    }

    set_catalog_progress(progress, "Scanning local songs...", 0.22f);
    const song_load_result load_result = song_loader::load_all(path_utils::to_utf8(app_paths::songs_root()));
    catalog.load_errors = load_result.errors;

    catalog.songs.reserve(load_result.songs.size());
    for (size_t i = 0; i < load_result.songs.size(); ++i) {
        set_catalog_progress(
            progress,
            "Loading local charts " + std::to_string(i + 1) + "/" + std::to_string(load_result.songs.size()) + "...",
            0.28f + 0.24f * (static_cast<float>(i) / std::max<size_t>(1, load_result.songs.size())));
        const song_data& song = load_result.songs[i];
        append_loaded_song(catalog, song, chart_offsets, local_index, calculate_missing_levels, std::nullopt);
    }

    set_catalog_progress(progress, "Scanning downloaded songs...", 0.54f);
    std::set<std::string> processed_package_dirs;
    std::vector<std::pair<online_content::source, std::filesystem::path>> package_dirs;
    for (const online_content::source source : {online_content::source::community,
                                                online_content::source::official}) {
        for (const std::filesystem::path& package_dir : managed_content_storage::list_package_directories(source)) {
            package_dirs.emplace_back(source, package_dir);
        }
    }
    for (size_t package_index = 0; package_index < package_dirs.size(); ++package_index) {
        const online_content::source source = package_dirs[package_index].first;
        const std::filesystem::path& package_dir = package_dirs[package_index].second;
        set_catalog_progress(
            progress,
            "Loading downloaded charts " + std::to_string(package_index + 1) + "/" +
                std::to_string(package_dirs.size()) + "...",
            0.58f + 0.30f * (static_cast<float>(package_index) / std::max<size_t>(1, package_dirs.size())));
        std::filesystem::path effective_package_dir = package_dir;
        std::optional<managed_content_storage::package_manifest> manifest =
            managed_content_storage::read_manifest(package_dir);
        if (!manifest.has_value()) {
            continue;
        }
        if (const std::optional<online_content::source> remote_source = remote_song_source_override(*manifest);
            remote_source.has_value() && *remote_source != manifest->song.source) {
            std::string relocate_error;
            const managed_content_storage::package_relocation_result relocation =
                managed_content_storage::relocate_package_source(package_dir, *remote_source, relocate_error);
            if (!relocation.success) {
                continue;
            }
            effective_package_dir = relocation.song_directory;
            manifest = managed_content_storage::read_manifest(effective_package_dir);
            if (!manifest.has_value()) {
                continue;
            }
        }
        if (manifest->song.source != source && effective_package_dir == package_dir) {
            continue;
        }
        std::error_code canonical_ec;
        const std::filesystem::path canonical_dir =
            std::filesystem::weakly_canonical(effective_package_dir, canonical_ec);
        const std::string processed_key =
            canonical_ec ? path_utils::to_utf8(effective_package_dir) : path_utils::to_utf8(canonical_dir);
        if (!processed_package_dirs.insert(processed_key).second) {
            continue;
        }
        local_catalog_cache_service::sync_managed_manifest_identity(*manifest);

        const song_load_result managed_load =
            song_loader::load_directory(path_utils::to_utf8(effective_package_dir));
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

    set_catalog_progress(progress, "Sorting catalog...", 0.90f);
    std::sort(catalog.songs.begin(), catalog.songs.end(), song_source_less);

    set_catalog_progress(progress, "Saving catalog cache...", 0.94f);
    local_catalog_cache_service::replace_if_unchanged(cache_guard, catalog.songs);
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
    const std::filesystem::path song_dir =
        path_utils::from_utf8(state.songs[static_cast<size_t>(song_index)].song.directory);
    const std::filesystem::path containment_path = managed ? song_dir : chart_path;
    const std::filesystem::path allowed_root = managed ? app_paths::content_cache_root() : app_paths::songs_root();
    if (!is_within_root(containment_path, allowed_root)) {
        result.message = managed
            ? "Refused to delete a managed chart outside the content cache."
            : "Refused to delete a chart outside the user songs directory.";
        return result;
    }

    std::error_code ec;
    if (managed) {
        std::optional<managed_content_storage::package_manifest> manifest =
            managed_content_storage::read_manifest(song_dir);
        if (!manifest.has_value()) {
            result.message = "Failed to read managed chart manifest.";
            return result;
        }

        const auto chart_it = std::find_if(
            manifest->charts.begin(),
            manifest->charts.end(),
            [&](const managed_content_storage::chart_manifest_entry& entry) {
                return entry.local_chart_id == chart.meta.chart_id;
            });
        if (chart_it == manifest->charts.end()) {
            result.message = "Managed chart was not found in the package manifest.";
            return result;
        }

        const std::filesystem::path encrypted_chart_path =
            managed_content_storage::encrypted_asset_path(song_dir, chart_it->encrypted_chart);
        if (!managed_content_storage::is_within_content_cache(encrypted_chart_path)) {
            result.message = "Refused to delete a managed chart asset outside the content cache.";
            return result;
        }

        std::filesystem::remove(encrypted_chart_path, ec);
        if (ec) {
            result.message = "Failed to delete the managed chart asset.";
            return result;
        }

        manifest->charts.erase(chart_it);
        std::string error_message;
        if (!managed_content_storage::write_manifest(*manifest, error_message)) {
            result.message = error_message.empty()
                ? "Failed to update managed chart manifest."
                : error_message;
            return result;
        }
    } else {
        const bool removed = std::filesystem::remove(chart_path, ec);
        if (ec || !removed) {
            result.message = "Failed to delete the chart file.";
            return result;
        }
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
