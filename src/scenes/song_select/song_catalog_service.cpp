#include "song_select/song_catalog_service.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>
#include <unordered_map>

#include "app_paths.h"
#include "chart_fingerprint.h"
#include "chart_identity_store.h"
#include "chart_level_cache.h"
#include "mv/mv_storage.h"
#include "network/auth_client.h"
#include "network/ranking_client.h"
#include "path_utils.h"
#include "player_note_offsets.h"
#include "ranking_service.h"
#include "song_loader.h"
#include "song_fingerprint.h"
#include "title/upload_mapping_store.h"
#include "updater/update_verify.h"

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

std::string trim(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return std::string(value.substr(start, end - start));
}

std::string status_label(content_status status) {
    switch (status) {
        case content_status::official: return "official";
        case content_status::community: return "community";
        case content_status::update: return "update";
        case content_status::modified: return "modified";
        case content_status::checking: return "checking";
        case content_status::local: return "local";
    }
    return "local";
}

content_status parse_status(std::string_view value) {
    const std::string normalized = trim(value);
    if (normalized == "official") return content_status::official;
    if (normalized == "community") return content_status::community;
    if (normalized == "update") return content_status::update;
    if (normalized == "modified") return content_status::modified;
    if (normalized == "checking") return content_status::checking;
    return content_status::local;
}

content_status status_for_content_source(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "official") {
        return content_status::official;
    }
    if (value == "community") {
        return content_status::community;
    }
    return content_status::local;
}

std::string current_manifest_server_url() {
    const auth::session_summary summary = auth::load_session_summary();
    if (!summary.server_url.empty()) {
        return auth::normalize_server_url(summary.server_url);
    }
    return auth::normalize_server_url(auth::kDefaultServerUrl);
}

std::string expected_remote_song_id(const std::string& server_url,
                                    const std::string& local_song_id) {
    const title_upload_mapping::store mappings = title_upload_mapping::load();
    return title_upload_mapping::find_remote_song_id(mappings, server_url, local_song_id)
        .value_or(local_song_id);
}

std::string expected_remote_chart_id(const std::string& server_url,
                                     const std::string& local_chart_id) {
    const title_upload_mapping::store mappings = title_upload_mapping::load();
    return title_upload_mapping::find_remote_chart_id(mappings, server_url, local_chart_id)
        .value_or(local_chart_id);
}

std::string file_signature(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        return {};
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return {};
    }
    const auto write_time = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return {};
    }
    return std::to_string(size) + ":" + std::to_string(write_time.time_since_epoch().count());
}

struct local_content_files {
    std::filesystem::path song_json_path;
    std::filesystem::path audio_path;
    std::filesystem::path jacket_path;
    std::filesystem::path chart_path;
};

struct content_hashes {
    std::string song_json_sha256;
    std::string song_json_fingerprint;
    std::string audio_sha256;
    std::string jacket_sha256;
    std::string chart_sha256;
    std::string chart_fingerprint;
};

content_hashes manifest_hashes(const ranking_client::chart_manifest& manifest) {
    return {
        manifest.song_json_sha256,
        manifest.song_json_fingerprint,
        manifest.audio_sha256,
        manifest.jacket_sha256,
        manifest.chart_sha256,
        manifest.chart_fingerprint,
    };
}

content_hashes manifest_song_hashes(const ranking_client::song_manifest& manifest) {
    return {
        manifest.song_json_sha256,
        manifest.song_json_fingerprint,
        manifest.audio_sha256,
        manifest.jacket_sha256,
        {},
        {},
    };
}

bool hashes_present(const content_hashes& hashes) {
    return (!hashes.song_json_fingerprint.empty() || !hashes.song_json_sha256.empty()) &&
           !hashes.audio_sha256.empty() &&
           !hashes.jacket_sha256.empty() &&
           (!hashes.chart_fingerprint.empty() || !hashes.chart_sha256.empty());
}

bool song_hashes_present(const content_hashes& hashes) {
    return (!hashes.song_json_fingerprint.empty() || !hashes.song_json_sha256.empty()) &&
           !hashes.audio_sha256.empty() &&
           !hashes.jacket_sha256.empty();
}

bool song_json_hash_equal(const content_hashes& left, const content_hashes& right) {
    if (!left.song_json_fingerprint.empty() && !right.song_json_fingerprint.empty()) {
        return left.song_json_fingerprint == right.song_json_fingerprint;
    }
    return !left.song_json_sha256.empty() && !right.song_json_sha256.empty() &&
           left.song_json_sha256 == right.song_json_sha256;
}

bool hashes_equal(const content_hashes& left, const content_hashes& right) {
    if (!song_json_hash_equal(left, right) ||
        left.audio_sha256 != right.audio_sha256 ||
        left.jacket_sha256 != right.jacket_sha256) {
        return false;
    }

    if (!left.chart_fingerprint.empty() && !right.chart_fingerprint.empty()) {
        return left.chart_fingerprint == right.chart_fingerprint;
    }
    return !left.chart_sha256.empty() && !right.chart_sha256.empty() &&
           left.chart_sha256 == right.chart_sha256;
}

std::string content_signature(const local_content_files& files) {
    const std::string song_json = file_signature(files.song_json_path);
    const std::string audio = file_signature(files.audio_path);
    const std::string jacket = file_signature(files.jacket_path);
    const std::string chart = file_signature(files.chart_path);
    if (song_json.empty() || audio.empty() || jacket.empty() || chart.empty()) {
        return {};
    }
    return song_json + ";" + audio + ";" + jacket + ";" + chart;
}

std::string song_content_signature(const song_data& song) {
    const local_content_files files{
        path_utils::from_utf8(song.directory) / "song.json",
        path_utils::from_utf8(song.directory) / path_utils::from_utf8(song.meta.audio_file),
        path_utils::from_utf8(song.directory) / path_utils::from_utf8(song.meta.jacket_file),
        {},
    };
    const std::string song_json = file_signature(files.song_json_path);
    const std::string audio = file_signature(files.audio_path);
    const std::string jacket = file_signature(files.jacket_path);
    if (song_json.empty() || audio.empty() || jacket.empty()) {
        return {};
    }
    return song_json + ";" + audio + ";" + jacket;
}

std::optional<content_hashes> compute_hashes(const local_content_files& files) {
    const std::optional<std::string> song_json = updater::compute_sha256_hex(files.song_json_path);
    const std::optional<std::string> song_json_fingerprint =
        song_fingerprint::compute_sha256_hex(files.song_json_path);
    const std::optional<std::string> audio = updater::compute_sha256_hex(files.audio_path);
    const std::optional<std::string> jacket = updater::compute_sha256_hex(files.jacket_path);
    const std::optional<std::string> chart = updater::compute_sha256_hex(files.chart_path);
    const std::optional<std::string> fingerprint = chart_fingerprint::compute_sha256_hex(files.chart_path);
    if (!song_json.has_value() || !song_json_fingerprint.has_value() ||
        !audio.has_value() || !jacket.has_value() ||
        !chart.has_value() || !fingerprint.has_value()) {
        return std::nullopt;
    }
    return content_hashes{*song_json, *song_json_fingerprint, *audio, *jacket, *chart, *fingerprint};
}

std::optional<content_hashes> compute_song_hashes(const song_data& song) {
    const std::filesystem::path song_json_path = path_utils::from_utf8(song.directory) / "song.json";
    const std::optional<std::string> song_json =
        updater::compute_sha256_hex(song_json_path);
    const std::optional<std::string> song_json_fingerprint =
        song_fingerprint::compute_sha256_hex(song_json_path);
    const std::optional<std::string> audio =
        updater::compute_sha256_hex(path_utils::from_utf8(song.directory) / path_utils::from_utf8(song.meta.audio_file));
    const std::optional<std::string> jacket =
        updater::compute_sha256_hex(path_utils::from_utf8(song.directory) / path_utils::from_utf8(song.meta.jacket_file));
    if (!song_json.has_value() || !song_json_fingerprint.has_value() ||
        !audio.has_value() || !jacket.has_value()) {
        return std::nullopt;
    }
    return content_hashes{*song_json, *song_json_fingerprint, *audio, *jacket, {}, {}};
}

struct verification_cache_record {
    std::string server_url;
    std::string chart_id;
    std::string song_id;
    content_status status = content_status::local;
    std::string content_source;
    std::string file_signature;
    content_hashes local_hashes;
    content_hashes server_hashes;
};

std::string cache_key(std::string_view server_url, std::string_view chart_id) {
    return std::string(server_url) + "\n" + std::string(chart_id);
}

using verification_cache = std::unordered_map<std::string, verification_cache_record>;

verification_cache load_verification_cache() {
    verification_cache cache;
    std::ifstream input(app_paths::source_verification_cache_path(), std::ios::binary);
    if (!input.is_open()) {
        return cache;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream row(line);
        std::string kind;
        verification_cache_record record;
        if (!std::getline(row, kind, '\t') || trim(kind) != "record" ||
            !std::getline(row, record.server_url, '\t') ||
            !std::getline(row, record.chart_id, '\t') ||
            !std::getline(row, record.song_id, '\t')) {
            continue;
        }

        std::string status_token;
        if (!std::getline(row, status_token, '\t') ||
            !std::getline(row, record.content_source, '\t') ||
            !std::getline(row, record.file_signature, '\t') ||
            !std::getline(row, record.local_hashes.song_json_sha256, '\t') ||
            !std::getline(row, record.local_hashes.audio_sha256, '\t') ||
            !std::getline(row, record.local_hashes.jacket_sha256, '\t') ||
            !std::getline(row, record.local_hashes.chart_sha256, '\t') ||
            !std::getline(row, record.server_hashes.song_json_sha256, '\t') ||
            !std::getline(row, record.server_hashes.audio_sha256, '\t') ||
            !std::getline(row, record.server_hashes.jacket_sha256, '\t') ||
            !std::getline(row, record.server_hashes.chart_sha256, '\t')) {
            continue;
        }
        std::getline(row, record.local_hashes.chart_fingerprint, '\t');
        std::getline(row, record.server_hashes.chart_fingerprint, '\t');
        std::getline(row, record.local_hashes.song_json_fingerprint, '\t');
        std::getline(row, record.server_hashes.song_json_fingerprint);
        record.status = parse_status(status_token);
        if (!record.server_url.empty() && !record.chart_id.empty()) {
            cache[cache_key(record.server_url, record.chart_id)] = std::move(record);
        }
    }
    return cache;
}

void save_verification_cache(const verification_cache& cache) {
    app_paths::ensure_directories();
    std::ofstream output(app_paths::source_verification_cache_path(), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return;
    }
    output << "# raythm source verification cache v1\n";
    for (const auto& [_, record] : cache) {
        output << "record\t"
               << record.server_url << '\t'
               << record.chart_id << '\t'
               << record.song_id << '\t'
               << status_label(record.status) << '\t'
               << record.content_source << '\t'
               << record.file_signature << '\t'
               << record.local_hashes.song_json_sha256 << '\t'
               << record.local_hashes.audio_sha256 << '\t'
               << record.local_hashes.jacket_sha256 << '\t'
               << record.local_hashes.chart_sha256 << '\t'
               << record.server_hashes.song_json_sha256 << '\t'
               << record.server_hashes.audio_sha256 << '\t'
               << record.server_hashes.jacket_sha256 << '\t'
               << record.server_hashes.chart_sha256 << '\t'
               << record.local_hashes.chart_fingerprint << '\t'
               << record.server_hashes.chart_fingerprint << '\t'
               << record.local_hashes.song_json_fingerprint << '\t'
               << record.server_hashes.song_json_fingerprint << '\n';
    }
}

content_status cached_status_for(const verification_cache_record* cached,
                                 const std::string& signature) {
    if (cached != nullptr && cached->file_signature == signature) {
        return cached->status;
    }
    return content_status::local;
}

bool is_verified_status(content_status status) {
    return status == content_status::official ||
           status == content_status::community ||
           status == content_status::update;
}

content_status mismatched_verified_status(const verification_cache_record* cached,
                                          bool local_unchanged) {
    if (cached == nullptr) {
        return content_status::local;
    }
    if (cached->status == content_status::modified) {
        return content_status::modified;
    }
    if (is_verified_status(cached->status)) {
        return local_unchanged ? content_status::update : content_status::modified;
    }
    return content_status::local;
}

content_status verify_song_content_source(const song_data& song,
                                          const std::string& server_url,
                                          verification_cache& cache,
                                          bool& server_reachable) {
    if (server_url.empty() || song.meta.song_id.empty()) {
        return content_status::local;
    }

    const std::string signature = song_content_signature(song);
    if (signature.empty()) {
        return content_status::local;
    }

    const std::string song_cache_id = "song:" + song.meta.song_id;
    const std::string key = cache_key(server_url, song_cache_id);
    const auto cached_it = cache.find(key);
    const verification_cache_record* cached = cached_it == cache.end() ? nullptr : &cached_it->second;
    if (!server_reachable) {
        return cached_status_for(cached, signature);
    }

    const std::string remote_song_id = expected_remote_song_id(server_url, song.meta.song_id);
    const ranking_client::song_manifest_operation_result request =
        ranking_client::fetch_song_manifest(server_url, remote_song_id);
    if (!request.success) {
        server_reachable = false;
        return cached_status_for(cached, signature);
    }
    if (!request.manifest.has_value() || !request.manifest->available) {
        return cached_status_for(cached, signature);
    }

    const ranking_client::song_manifest& manifest = *request.manifest;
    if (manifest.song_id != remote_song_id) {
        return content_status::local;
    }

    std::optional<content_hashes> local_hashes;
    if (cached != nullptr && cached->file_signature == signature && song_hashes_present(cached->local_hashes)) {
        local_hashes = cached->local_hashes;
    } else {
        local_hashes = compute_song_hashes(song);
    }
    if (!local_hashes.has_value()) {
        return content_status::local;
    }

    const content_hashes server_hashes = manifest_song_hashes(manifest);
    if (!song_hashes_present(server_hashes)) {
        return content_status::local;
    }

    const content_status source_status = status_for_content_source(manifest.content_source);
    const bool matched = song_hashes_present(*local_hashes) &&
                         song_json_hash_equal(*local_hashes, server_hashes) &&
                         local_hashes->audio_sha256 == server_hashes.audio_sha256 &&
                         local_hashes->jacket_sha256 == server_hashes.jacket_sha256;
    const bool local_unchanged =
        cached != nullptr &&
        cached->file_signature == signature &&
        song_json_hash_equal(*local_hashes, cached->local_hashes) &&
        local_hashes->audio_sha256 == cached->local_hashes.audio_sha256 &&
        local_hashes->jacket_sha256 == cached->local_hashes.jacket_sha256;
    const content_status status = matched ? source_status : mismatched_verified_status(cached, local_unchanged);

    cache[key] = verification_cache_record{
        .server_url = server_url,
        .chart_id = song_cache_id,
        .song_id = manifest.song_id,
        .status = status,
        .content_source = manifest.content_source,
        .file_signature = signature,
        .local_hashes = *local_hashes,
        .server_hashes = server_hashes,
    };
    return status;
}

content_status verify_chart_content_source(const song_data& song,
                                           const song_select::chart_option& chart,
                                           const std::string& server_url,
                                           verification_cache& cache,
                                           bool& server_reachable) {
    if (server_url.empty() || chart.meta.chart_id.empty()) {
        return content_status::local;
    }

    const local_content_files files{
        path_utils::from_utf8(song.directory) / "song.json",
        path_utils::from_utf8(song.directory) / path_utils::from_utf8(song.meta.audio_file),
        path_utils::from_utf8(song.directory) / path_utils::from_utf8(song.meta.jacket_file),
        path_utils::from_utf8(chart.path),
    };
    const std::string signature = content_signature(files);
    if (signature.empty()) {
        return content_status::local;
    }

    const std::string key = cache_key(server_url, chart.meta.chart_id);
    const auto cached_it = cache.find(key);
    const verification_cache_record* cached = cached_it == cache.end() ? nullptr : &cached_it->second;
    if (!server_reachable) {
        return cached_status_for(cached, signature);
    }

    const std::string remote_song_id = expected_remote_song_id(server_url, song.meta.song_id);
    const std::string remote_chart_id = expected_remote_chart_id(server_url, chart.meta.chart_id);
    const ranking_client::manifest_operation_result request =
        ranking_client::fetch_chart_manifest(server_url, remote_chart_id);
    if (!request.success) {
        server_reachable = false;
        return cached_status_for(cached, signature);
    }
    if (!request.manifest.has_value() || !request.manifest->available) {
        return cached_status_for(cached, signature);
    }

    const ranking_client::chart_manifest& manifest = *request.manifest;
    if (manifest.chart_id != remote_chart_id ||
        manifest.song_id != remote_song_id) {
        return content_status::local;
    }

    std::optional<content_hashes> local_hashes;
    if (cached != nullptr && cached->file_signature == signature && hashes_present(cached->local_hashes)) {
        local_hashes = cached->local_hashes;
    } else {
        local_hashes = compute_hashes(files);
    }
    if (!local_hashes.has_value()) {
        return content_status::local;
    }

    const content_hashes server_hashes = manifest_hashes(manifest);
    if (!hashes_present(server_hashes)) {
        return content_status::local;
    }

    const content_status source_status = status_for_content_source(manifest.content_source);
    const bool matched = hashes_equal(*local_hashes, server_hashes);
    const bool local_unchanged =
        cached != nullptr &&
        cached->file_signature == signature &&
        hashes_equal(*local_hashes, cached->local_hashes);
    const content_status status = matched ? source_status : mismatched_verified_status(cached, local_unchanged);

    cache[key] = verification_cache_record{
        .server_url = server_url,
        .chart_id = chart.meta.chart_id,
        .song_id = manifest.song_id,
        .status = status,
        .content_source = manifest.content_source,
        .file_signature = signature,
        .local_hashes = *local_hashes,
        .server_hashes = server_hashes,
    };
    return status;
}

content_status aggregate_song_status(const std::vector<song_select::chart_option>& charts) {
    if (charts.empty()) {
        return content_status::local;
    }

    bool has_modified = false;
    bool has_update = false;
    bool has_local = false;
    std::optional<content_status> remote_status;
    for (const song_select::chart_option& chart : charts) {
        if (chart.status == content_status::modified) {
            has_modified = true;
        } else if (chart.status == content_status::update) {
            has_update = true;
        } else if (chart.status == content_status::official || chart.status == content_status::community) {
            if (!remote_status.has_value()) {
                remote_status = chart.status;
            } else if (*remote_status != chart.status) {
                has_local = true;
            }
        } else {
            has_local = true;
        }
    }

    if (has_modified) {
        return content_status::modified;
    }
    if (has_update) {
        return content_status::update;
    }
    if (!has_local && remote_status.has_value()) {
        return *remote_status;
    }
    return content_status::local;
}

content_status combine_song_and_chart_status(content_status song_status, content_status chart_status) {
    if (song_status == content_status::modified || chart_status == content_status::modified) {
        return content_status::modified;
    }
    if (song_status == content_status::update || chart_status == content_status::update) {
        return content_status::update;
    }
    if (song_status == content_status::official || song_status == content_status::community) {
        return song_status;
    }
    return chart_status;
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
    catalog_data catalog;
    const player_chart_offset_map chart_offsets = load_player_chart_offsets();
    const std::string manifest_server_url = current_manifest_server_url();
    verification_cache verification_cache = load_verification_cache();
    bool manifest_server_reachable = true;

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
            if (meta.song_id.empty()) {
                meta.song_id = song.meta.song_id;
            }
            if (calculate_missing_levels) {
                meta.level = chart_level_cache::get_or_calculate(chart_path, *parse_result.data);
            } else if (const std::optional<float> cached_level = chart_level_cache::find_level(chart_path);
                       cached_level.has_value()) {
                meta.level = *cached_level;
            }
            const auto [min_bpm, max_bpm] = collect_bpm_range(*parse_result.data);

            chart_option option{
                chart_path,
                meta,
                content_status::local,
                chart_offsets.contains(meta.chart_id) ? chart_offsets.at(meta.chart_id) : 0,
                load_best_local_rank(meta.chart_id),
                static_cast<int>(parse_result.data->notes.size()),
                min_bpm,
                max_bpm,
            };
            option.status = verify_chart_content_source(
                song, option, manifest_server_url, verification_cache, manifest_server_reachable);
            entry.charts.push_back(std::move(option));
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

        entry.status = combine_song_and_chart_status(
            verify_song_content_source(song, manifest_server_url, verification_cache, manifest_server_reachable),
            aggregate_song_status(entry.charts));
        catalog.songs.push_back(std::move(entry));
    }

    save_verification_cache(verification_cache);
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

    struct chart_delete_target {
        std::filesystem::path path;
        std::string chart_id;
    };
    std::vector<chart_delete_target> chart_paths_to_delete;
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

            std::string linked_song_id = parse_result.data->meta.song_id;
            if (linked_song_id.empty()) {
                linked_song_id = chart_identity::find_song_id(parse_result.data->meta.chart_id).value_or("");
            }

            if (linked_song_id == entry.song.meta.song_id) {
                chart_paths_to_delete.push_back({
                    .path = chart_entry.path(),
                    .chart_id = parse_result.data->meta.chart_id,
                });
            }
        }
    }

    std::error_code ec;
    for (const auto& chart_target : chart_paths_to_delete) {
        std::filesystem::remove(chart_target.path, ec);
        if (ec) {
            result.message = "Failed to delete a linked chart file.";
            return result;
        }
        chart_identity::remove(chart_target.chart_id);
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
    chart_identity::remove_for_song(entry.song.meta.song_id);

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
    chart_identity::remove(charts[static_cast<size_t>(chart_index)].meta.chart_id);

    result.success = true;
    result.message = "Chart deleted.";
    result.preferred_song_id = state.songs[static_cast<size_t>(song_index)].song.meta.song_id;
    result.preferred_chart_id = fallback_chart_id_after_chart_delete(state, song_index, chart_index);
    return result;
}

}  // namespace song_select
