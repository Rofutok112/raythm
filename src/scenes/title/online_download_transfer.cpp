#include "title/online_download_internal.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "app_paths.h"
#include "chart_file_storage.h"
#include "network/json_helpers.h"
#include "path_utils.h"
#include "song_writer.h"
#include "title/local_content_index.h"
#include "title/online_catalog_data_controller.h"
#include "title/online_download_remote_client.h"
#include "ui_notice.h"

namespace title_online_view {
namespace {
namespace json = network::json;

content_status source_status_from_remote_download(const std::string& content_source) {
    return content_source == "official" ? content_status::official : content_status::community;
}

online_content::source online_source_from_remote_download(const std::string& content_source) {
    return content_source == "official" ? online_content::source::official : online_content::source::community;
}

content_kind content_kind_from_remote_download(const std::string& content_source) {
    return content_source == "official" ? content_kind::official : content_kind::community;
}

const song_select::song_entry* find_local_song_by_remote(const std::vector<song_select::song_entry>& local_songs,
                                                         const local_content_index::snapshot& index,
                                                         const std::string& server_url,
                                                         const std::string& remote_song_id,
                                                         std::string& local_song_id) {
    for (const song_select::song_entry& song : local_songs) {
        const std::string& candidate_id = song.song.meta.song_id;
        if (candidate_id == remote_song_id) {
            local_song_id = candidate_id;
            return &song;
        }
        const std::optional<local_content_index::online_song_binding> binding =
            local_content_index::find_song_by_local(index, server_url, candidate_id);
        if (binding.has_value() && binding->remote_song_id == remote_song_id) {
            local_song_id = candidate_id;
            return &song;
        }
    }
    local_song_id.clear();
    return nullptr;
}

song_entry_state make_download_song_state(const remote_song_payload& remote_song,
                                          const std::string& server_url,
                                          const std::vector<song_select::song_entry>& local_songs,
                                          const local_content_index::snapshot& index) {
    song_entry_state song;
    song.song.song.meta.song_id = remote_song.id;
    song.song.song.meta.title = remote_song.title;
    song.song.song.meta.artist = remote_song.artist;
    song.song.song.meta.genre = remote_song.genre;
    song.song.song.meta.genres = remote_song.genres;
    song.song.song.meta.keywords = remote_song.keywords;
    song.song.song.meta.base_bpm = remote_song.base_bpm;
    song.song.song.meta.offset = remote_song.offset;
    song.song.song.meta.has_offset = remote_song.has_offset;
    song.song.song.meta.timing_events = remote_song.timing_events;
    song.song.song.meta.duration_seconds = remote_song.duration_seconds;
    song.song.song.meta.audio_url = make_absolute_remote_url(server_url, remote_song.audio_url);
    song.song.song.meta.jacket_url = make_absolute_remote_url(server_url, remote_song.jacket_url);
    song.song.song.meta.preview_start_ms = remote_song.preview_start_ms;
    song.song.song.meta.preview_start_seconds = static_cast<float>(remote_song.preview_start_ms) / 1000.0f;
    song.song.song.meta.song_version = remote_song.song_version;
    song.song.song.meta.chart_count = remote_song.chart_count;
    song.song.song.meta.play_count = remote_song.play_count;
    song.song.song.meta.has_play_count = remote_song.has_play_count;
    song.song.kind = content_kind_from_remote_download(remote_song.content_source);
    song.song.storage = storage_policy::managed_package;
    song.song.verification = verification_state::unchecked;
    song.song.source_status = source_status_from_remote_download(remote_song.content_source);
    song.song.online_identity = online_content::song_identity{
        .server_url = server_url,
        .remote_song_id = remote_song.id,
        .content_source = online_source_from_remote_download(remote_song.content_source),
        .can_edit = remote_song.has_can_edit ? std::optional<bool>(remote_song.can_edit) : std::nullopt,
        .lifecycle_status = remote_song.lifecycle_status,
    };
    std::string local_song_id;
    const song_select::song_entry* local_song =
        find_local_song_by_remote(local_songs, index, server_url, remote_song.id, local_song_id);
    song.installed = local_song != nullptr;
    song.installed_local_song_id = local_song_id;
    if (local_song != nullptr) {
        song.song.status = local_song->status == content_status::modified
            ? content_status::modified
            : song.song.source_status;
        song.update_available = local_song->song.meta.song_version < remote_song.song_version;
    }
    return song;
}

chart_entry_state make_download_chart_state(const remote_chart_payload& remote_chart,
                                            const remote_song_payload& remote_song,
                                            const std::string& server_url,
                                            const song_entry_state& song,
                                            const local_content_index::snapshot& index) {
    chart_entry_state chart;
    chart.chart.meta.chart_id = remote_chart.id;
    chart.chart.meta.song_id = remote_chart.song_id;
    chart.chart.meta.chart_version = remote_chart.chart_version;
    chart.chart.meta.key_count = remote_chart.key_count;
    chart.chart.meta.difficulty = remote_chart.difficulty_name;
    chart.chart.meta.level = remote_chart.level;
    chart.chart.meta.chart_author = remote_chart.chart_author;
    chart.chart.meta.format_version = remote_chart.format_version;
    chart.chart.meta.resolution = remote_chart.resolution;
    chart.chart.meta.offset = remote_chart.offset;
    chart.chart.kind = content_kind_from_remote_download(remote_chart.content_source);
    chart.chart.storage = storage_policy::managed_package;
    chart.chart.verification = verification_state::unchecked;
    chart.chart.source_status = source_status_from_remote_download(remote_chart.content_source);
    chart.chart.status = chart.chart.source_status;
    chart.chart.online_identity = online_content::chart_identity{
        .server_url = server_url,
        .remote_song_id = remote_chart.song_id,
        .remote_chart_id = remote_chart.id,
        .content_source = online_source_from_remote_download(remote_chart.content_source),
        .remote_chart_version = remote_chart.chart_version,
        .can_edit = remote_chart.has_can_edit ? std::optional<bool>(remote_chart.can_edit) : std::nullopt,
        .lifecycle_status = remote_chart.lifecycle_status,
    };
    chart.chart.note_count = remote_chart.note_count;
    chart.chart.min_bpm = remote_chart.min_bpm > 0.0f ? remote_chart.min_bpm : remote_song.base_bpm;
    chart.chart.max_bpm = remote_chart.max_bpm > 0.0f ? remote_chart.max_bpm : remote_song.base_bpm;
    chart.uploader_id = remote_chart.uploader_id;

    const std::optional<local_content_index::online_chart_binding> binding =
        local_content_index::find_chart_by_remote(index, server_url, remote_chart.id);
    chart.installed_local_chart_id = binding.has_value() ? binding->local_chart_id : "";
    chart.installed = binding.has_value() && !binding->local_chart_id.empty();
    chart.update_available = chart.installed &&
                             remote_chart.chart_version > std::max(1, binding->remote_chart_version);
    if (chart.installed && song.installed_local_song_id.empty()) {
        chart.installed = false;
        chart.installed_local_chart_id.clear();
    }
    return chart;
}

std::string trim_ascii(std::string_view value) {
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

bool write_binary_file(const std::filesystem::path& path,
                       const std::vector<unsigned char>& bytes,
                       std::string& error_message) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error_message = "Failed to open a local file for writing.";
        return false;
    }

    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }

    if (!output.good()) {
        error_message = "Failed to write downloaded data to disk.";
        return false;
    }

    return true;
}

bool restore_staged_charts(const std::filesystem::path& staged_charts_dir,
                           const std::filesystem::path& charts_dir) {
    std::error_code ec;
    if (!std::filesystem::exists(staged_charts_dir, ec)) {
        return true;
    }
    std::filesystem::create_directories(charts_dir.parent_path(), ec);
    if (ec) {
        return false;
    }
    std::filesystem::rename(staged_charts_dir, charts_dir, ec);
    return !ec;
}

std::vector<timing_event> parse_timing_events(const std::string& metadata_json) {
    std::vector<timing_event> events;
    const std::optional<std::string> timing_array = json::extract_array(metadata_json, "timingEvents");
    if (!timing_array.has_value()) {
        return events;
    }

    for (const std::string& object : json::extract_objects_from_array(*timing_array)) {
        const std::string type = json::extract_string(object, "type").value_or("");
        const std::optional<int> tick = json::extract_int(object, "tick");
        if (!tick.has_value() || *tick < 0) {
            continue;
        }

        timing_event event;
        event.tick = *tick;
        if (type == "bpm") {
            const std::optional<float> bpm = json::extract_float(object, "bpm");
            if (!bpm.has_value() || *bpm <= 0.0f) {
                continue;
            }
            event.type = timing_event_type::bpm;
            event.bpm = *bpm;
        } else if (type == "meter") {
            const std::optional<int> numerator = json::extract_int(object, "numerator");
            const std::optional<int> denominator = json::extract_int(object, "denominator");
            if (!numerator.has_value() || !denominator.has_value() || *numerator <= 0 || *denominator <= 0) {
                continue;
            }
            event.type = timing_event_type::meter;
            event.numerator = *numerator;
            event.denominator = *denominator;
        } else {
            continue;
        }
        events.push_back(event);
    }
    return events;
}

std::optional<song_meta> parse_downloaded_song_metadata(const std::string& metadata_json,
                                                        const std::string& local_song_id,
                                                        int fallback_song_version,
                                                        std::string& error_message) {
    song_meta meta;
    meta.song_id = local_song_id;
    meta.title = trim_ascii(json::extract_string(metadata_json, "title").value_or(""));
    meta.artist = trim_ascii(json::extract_string(metadata_json, "artist").value_or(""));
    meta.genre = trim_ascii(json::extract_string(metadata_json, "genre").value_or(""));
    if (const std::optional<std::string> genres = json::extract_array(metadata_json, "genres")) {
        meta.genres = json::extract_strings_from_array(*genres);
    }
    if (meta.genre.empty() && !meta.genres.empty()) {
        meta.genre = meta.genres.front();
    }
    if (meta.genres.empty() && !meta.genre.empty()) {
        meta.genres.push_back(meta.genre);
    }
    if (const std::optional<std::string> keywords = json::extract_array(metadata_json, "keywords")) {
        meta.keywords = json::extract_strings_from_array(*keywords);
    }
    meta.audio_file = trim_ascii(json::extract_string(metadata_json, "audioFile").value_or(""));
    meta.jacket_file = trim_ascii(json::extract_string(metadata_json, "jacketFile").value_or(""));
    meta.base_bpm = json::extract_float(metadata_json, "baseBpm").value_or(0.0f);
    if (const std::optional<int> offset = json::extract_int(metadata_json, "offset")) {
        meta.offset = *offset;
        meta.has_offset = true;
    }
    meta.timing_events = parse_timing_events(metadata_json);
    meta.duration_seconds = json::extract_float(metadata_json, "durationSec").value_or(0.0f);
    meta.chart_count = std::max(0, json::extract_int(metadata_json, "chartCount").value_or(0));
    meta.preview_start_ms = json::extract_int(metadata_json, "previewStartMs").value_or(0);
    meta.preview_start_seconds = static_cast<float>(meta.preview_start_ms) / 1000.0f;
    meta.song_version = json::extract_int(metadata_json, "songVersion").value_or(
        fallback_song_version > 0 ? fallback_song_version : 1);

    if (meta.song_id.empty()) {
        error_message = "Downloaded song metadata was missing a local song ID.";
        return std::nullopt;
    }
    if (meta.title.empty()) {
        error_message = "Downloaded song metadata did not include title.";
        return std::nullopt;
    }
    if (meta.artist.empty()) {
        error_message = "Downloaded song metadata did not include artist.";
        return std::nullopt;
    }
    if (meta.audio_file.empty()) {
        error_message = "Downloaded song metadata did not include audioFile.";
        return std::nullopt;
    }
    if (meta.jacket_file.empty()) {
        error_message = "Downloaded song metadata did not include jacketFile.";
        return std::nullopt;
    }
    if (meta.base_bpm <= 0.0f) {
        error_message = "Downloaded song metadata did not include a valid baseBpm.";
        return std::nullopt;
    }

    return meta;
}

void mark_song_downloaded(std::vector<song_entry_state>& songs, const std::string& song_id) {
    for (song_entry_state& song : songs) {
        if (song.song.song.meta.song_id != song_id) {
            continue;
        }

        song.installed = true;
        song.update_available = false;
        song.song.status = song.song.source_status;
        song.charts_loaded = true;
        song.charts_loading = false;
        song.charts_has_more = false;
    }
}

void mark_chart_downloaded(std::vector<song_entry_state>& songs,
                           const std::string& song_id,
                           const std::string& chart_id) {
    for (song_entry_state& song : songs) {
        if (song.song.song.meta.song_id != song_id) {
            continue;
        }

        song.installed = true;
        for (chart_entry_state& chart : song.charts) {
            if (chart.chart.meta.chart_id == chart_id) {
                chart.installed = true;
                chart.update_available = false;
                chart.chart.status = chart.chart.source_status;
            }
        }
    }
}

void mark_song_not_installed(std::vector<song_entry_state>& songs, const std::string& song_id) {
    for (song_entry_state& song : songs) {
        if (song.song.song.meta.song_id != song_id) {
            continue;
        }

        song.installed = false;
        for (chart_entry_state& chart : song.charts) {
            chart.installed = false;
        }
    }
}

download_song_result download_song_package(const song_entry_state song,
                                           const std::string& server_url,
                                           const std::shared_ptr<download_progress_state>& progress) {
    download_song_result result;
    result.song_id = song.song.song.meta.song_id;
    const std::string local_song_id = song.installed_local_song_id.empty()
        ? result.song_id
        : song.installed_local_song_id;

    if (server_url.empty() || result.song_id.empty() || local_song_id.empty()) {
        result.message = "Missing song download information.";
        return result;
    }

    auto begin_step = [&](const std::string& url) {
        return fetch_remote_binary(url, [progress](size_t bytes_received, size_t total_bytes) {
            if (!progress) {
                return;
            }
            progress->current_bytes.store(bytes_received);
            progress->current_total_bytes.store(total_bytes);
        });
    };

    auto finish_step = [&]() {
        if (!progress) {
            return;
        }
        progress->completed_steps.fetch_add(1);
        progress->current_bytes.store(0);
        progress->current_total_bytes.store(0);
    };

    const std::string metadata_url =
        make_absolute_remote_url(server_url, "/songs/" + result.song_id + "/metadata");
    const remote_binary_fetch_result metadata_fetch = begin_step(metadata_url);
    if (!metadata_fetch.success || metadata_fetch.bytes.empty()) {
        result.message = metadata_fetch.error_message.empty()
            ? "Failed to download song metadata."
            : metadata_fetch.error_message;
        return result;
    }
    finish_step();

    std::string error_message;
    const std::string metadata_json(metadata_fetch.bytes.begin(), metadata_fetch.bytes.end());
    const std::optional<song_meta> local_meta =
        parse_downloaded_song_metadata(metadata_json,
                                       local_song_id,
                                       song.song.song.meta.song_version,
                                       error_message);
    if (!local_meta.has_value()) {
        result.message = error_message;
        return result;
    }

    const int total_steps = 2 + (local_meta->jacket_file.empty() ? 0 : 1);
    if (progress) {
        progress->total_steps.store(total_steps);
        progress->completed_steps.store(1);
        progress->current_bytes.store(0);
        progress->current_total_bytes.store(0);
    }

    const remote_binary_fetch_result audio_fetch = begin_step(song.song.song.meta.audio_url);
    if (!audio_fetch.success || audio_fetch.bytes.empty()) {
        result.message = audio_fetch.error_message.empty()
            ? "Failed to download the song audio."
            : audio_fetch.error_message;
        return result;
    }
    finish_step();

    std::vector<unsigned char> jacket_bytes;
    if (!local_meta->jacket_file.empty()) {
        const remote_binary_fetch_result jacket_fetch = begin_step(song.song.song.meta.jacket_url);
        if (!jacket_fetch.success || jacket_fetch.bytes.empty()) {
            result.message = jacket_fetch.error_message.empty()
                ? "Failed to download the song jacket."
                : jacket_fetch.error_message;
            return result;
        }
        jacket_bytes = jacket_fetch.bytes;
        finish_step();
    }

    app_paths::ensure_directories();
    const std::filesystem::path song_dir = app_paths::song_dir(local_song_id);
    const std::filesystem::path charts_dir = song_dir / "charts";
    const std::filesystem::path audio_path = song_dir / path_utils::from_utf8(local_meta->audio_file);
    const std::filesystem::path jacket_path = song_dir / path_utils::from_utf8(local_meta->jacket_file);

    std::error_code ec;
    const std::filesystem::path staged_charts_dir =
        app_paths::app_data_root() / "download-staging" / (local_song_id + "-charts");
    std::filesystem::remove_all(staged_charts_dir, ec);
    ec.clear();
    if (std::filesystem::exists(charts_dir, ec)) {
        std::filesystem::create_directories(staged_charts_dir.parent_path(), ec);
        if (ec) {
            result.message = "Failed to prepare existing chart files for song update.";
            return result;
        }
        std::filesystem::rename(charts_dir, staged_charts_dir, ec);
        if (ec) {
            result.message = "Failed to preserve existing chart files for song update.";
            return result;
        }
    }
    std::filesystem::remove_all(song_dir, ec);
    if (ec) {
        restore_staged_charts(staged_charts_dir, charts_dir);
        result.message = "Failed to replace the local song files.";
        return result;
    }

    if (!song_writer::write_song_json(*local_meta, path_utils::to_utf8(song_dir))) {
        restore_staged_charts(staged_charts_dir, charts_dir);
        result.message = "Failed to write downloaded song metadata to disk.";
        return result;
    }

    if (!write_binary_file(audio_path, audio_fetch.bytes, error_message)) {
        restore_staged_charts(staged_charts_dir, charts_dir);
        result.message = error_message;
        return result;
    }

    if (!local_meta->jacket_file.empty() && !write_binary_file(jacket_path, jacket_bytes, error_message)) {
        restore_staged_charts(staged_charts_dir, charts_dir);
        result.message = error_message;
        return result;
    }
    if (!restore_staged_charts(staged_charts_dir, charts_dir)) {
        result.message = "Song updated, but existing chart files could not be restored.";
        return result;
    }

    const local_content_index::online_origin origin =
        [&]() {
            const std::optional<local_content_index::online_song_binding> binding =
                local_content_index::find_song_by_local(server_url, local_song_id);
            return binding.has_value() ? binding->origin : local_content_index::online_origin::downloaded;
        }();
    local_content_index::put_song_binding({
        .server_url = server_url,
        .local_song_id = local_song_id,
        .remote_song_id = result.song_id,
        .origin = origin,
        .can_edit = song.song.online_identity.has_value() ? song.song.online_identity->can_edit : std::nullopt,
        .lifecycle_status = song.song.online_identity.has_value() ? song.song.online_identity->lifecycle_status : "",
    });

    result.success = true;
    result.message = song.installed ? "Song updated." : "Song downloaded.";
    return result;
}

download_song_result download_chart_file(const song_entry_state song,
                                         const chart_entry_state chart,
                                         const std::string& server_url,
                                         const std::shared_ptr<download_progress_state>& progress) {
    download_song_result result;
    result.song_id = song.song.song.meta.song_id;
    result.chart_id = chart.chart.meta.chart_id;
    result.chart_only = true;
    const std::string local_song_id = !song.installed_local_song_id.empty()
        ? song.installed_local_song_id
        : song.song.song.meta.song_id;

    if (server_url.empty() || result.song_id.empty() || result.chart_id.empty() || local_song_id.empty()) {
        result.message = "Missing chart download information.";
        return result;
    }
    if (!song.installed) {
        result.message = "Download the song first.";
        return result;
    }

    if (progress) {
        progress->total_steps.store(1);
        progress->completed_steps.store(0);
        progress->current_bytes.store(0);
        progress->current_total_bytes.store(0);
    }

    const std::string chart_url =
        make_absolute_remote_url(server_url, "/charts/" + result.chart_id + "/file");
    const remote_binary_fetch_result chart_fetch =
        fetch_remote_binary(chart_url, [progress](size_t bytes_received, size_t total_bytes) {
            if (!progress) {
                return;
            }
            progress->current_bytes.store(bytes_received);
            progress->current_total_bytes.store(total_bytes);
        });
    if (!chart_fetch.success || chart_fetch.bytes.empty()) {
        result.message = chart_fetch.error_message.empty()
            ? "Failed to download the chart file."
            : chart_fetch.error_message;
        return result;
    }
    if (progress) {
        progress->completed_steps.store(1);
        progress->current_bytes.store(0);
        progress->current_total_bytes.store(0);
    }

    const std::string local_chart_id = chart.installed_local_chart_id.empty()
        ? result.chart_id
        : chart.installed_local_chart_id;

    std::string error_message;
    app_paths::ensure_directories();
    if (!chart_file_storage::write_validated_raw_chart_file(
            app_paths::song_chart_path(local_song_id, local_chart_id),
            chart_fetch.bytes,
            error_message)) {
        result.message = error_message;
        return result;
    }
    if (!local_content_index::find_song_by_local(server_url, local_song_id).has_value()) {
        local_content_index::put_song_binding({
            .server_url = server_url,
            .local_song_id = local_song_id,
            .remote_song_id = result.song_id,
            .origin = local_content_index::online_origin::downloaded,
            .can_edit = song.song.online_identity.has_value() ? song.song.online_identity->can_edit : std::nullopt,
            .lifecycle_status = song.song.online_identity.has_value() ? song.song.online_identity->lifecycle_status : "",
        });
    }
    local_content_index::put_chart_binding({
        .server_url = server_url,
        .local_chart_id = local_chart_id,
        .remote_chart_id = result.chart_id,
        .remote_song_id = result.song_id,
        .remote_chart_version = chart.chart.meta.chart_version,
        .origin = local_content_index::online_origin::downloaded,
        .can_edit = chart.chart.online_identity.has_value() ? chart.chart.online_identity->can_edit : std::nullopt,
        .lifecycle_status = chart.chart.online_identity.has_value() ? chart.chart.online_identity->lifecycle_status : "",
    });

    result.success = true;
    result.message = "Chart downloaded.";
    return result;
}

}  // namespace

bool needs_download(const song_entry_state& song) {
    return !song.installed ||
           song.update_available ||
           song.song.status == content_status::modified;
}

void start_download(state& state, online_catalog::data_controller& data_controller) {
    if (state.download_in_progress) {
        return;
    }

    const song_entry_state* song = selected_song(state);
    if (song == nullptr || !needs_download(*song)) {
        return;
    }

    state.download_in_progress = true;
    const song_entry_state selected = *song;
    const std::string server_url = state.catalog_server_url;
    state.download_progress = std::make_shared<download_progress_state>();
    ui::notify("Downloading song...", ui::notice_tone::info, 1.8f);
    const std::shared_ptr<download_progress_state> progress = state.download_progress;
    std::promise<download_song_result> promise;
    data_controller.download_future() = promise.get_future();
    std::thread([promise = std::move(promise), selected, server_url, progress]() mutable {
        try {
            promise.set_value(download_song_package(selected, server_url, progress));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void start_chart_download(state& state, online_catalog::data_controller& data_controller) {
    if (state.download_in_progress) {
        return;
    }

    const song_entry_state* song = selected_song(state);
    const chart_entry_state* chart = selected_chart(state);
    if (song == nullptr || chart == nullptr) {
        return;
    }
    if (!song->installed) {
        ui::notify("Download the song first.", ui::notice_tone::error, 2.6f);
        return;
    }
    if (chart->installed &&
        !chart->update_available &&
        chart->chart.status != content_status::modified) {
        return;
    }

    state.download_in_progress = true;
    const song_entry_state selected_song = *song;
    const chart_entry_state selected_chart = *chart;
    const std::string server_url = state.catalog_server_url;
    state.download_progress = std::make_shared<download_progress_state>();
    ui::notify("Downloading chart...", ui::notice_tone::info, 1.8f);
    const std::shared_ptr<download_progress_state> progress = state.download_progress;
    std::promise<download_song_result> promise;
    data_controller.download_future() = promise.get_future();
    std::thread([promise = std::move(promise), selected_song, selected_chart, server_url, progress]() mutable {
        try {
            promise.set_value(download_chart_file(selected_song, selected_chart, server_url, progress));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void start_chart_download_by_remote_id(state& state,
                                       online_catalog::data_controller& data_controller,
                                       const std::string& remote_song_id,
                                       const std::string& remote_chart_id) {
    if (state.download_in_progress) {
        return;
    }
    if (remote_song_id.empty() || remote_chart_id.empty()) {
        ui::notify("Missing online chart identity.", ui::notice_tone::error, 2.6f);
        return;
    }

    state.download_in_progress = true;
    state.download_progress = std::make_shared<download_progress_state>();
    ui::notify("Downloading queued chart...", ui::notice_tone::info, 1.8f);
    const std::shared_ptr<download_progress_state> progress = state.download_progress;
    const std::string preferred_server_url = state.catalog_server_url;
    const std::vector<song_select::song_entry> local_songs = state.local_songs;
    std::promise<download_song_result> promise;
    data_controller.download_future() = promise.get_future();
    std::thread([promise = std::move(promise),
                 remote_song_id,
                 remote_chart_id,
                 preferred_server_url,
                 local_songs,
                 progress]() mutable {
        try {
            download_song_result result;
            const remote_song_lookup_result song_lookup =
                fetch_remote_song_by_id(remote_song_id, preferred_server_url);
            if (!song_lookup.success) {
                result.message = song_lookup.not_found ? "Queued song was not found." :
                    (song_lookup.error_message.empty() ? "Failed to load queued song." : song_lookup.error_message);
                promise.set_value(std::move(result));
                return;
            }

            remote_chart_payload remote_chart;
            bool found_chart = false;
            std::string cursor;
            do {
                const remote_chart_page_fetch_result chart_page =
                    fetch_remote_chart_page(song_lookup.server_url, remote_song_id, cursor, 50);
                if (!chart_page.success) {
                    result.message = chart_page.error_message.empty()
                        ? "Failed to load queued chart."
                        : chart_page.error_message;
                    promise.set_value(std::move(result));
                    return;
                }
                for (const remote_chart_payload& chart : chart_page.charts) {
                    if (chart.id == remote_chart_id) {
                        remote_chart = chart;
                        found_chart = true;
                        break;
                    }
                }
                cursor = chart_page.next_cursor;
                if (found_chart || !chart_page.has_more) {
                    break;
                }
            } while (!cursor.empty());

            if (!found_chart) {
                result.message = "Queued chart was not found.";
                promise.set_value(std::move(result));
                return;
            }

            const local_content_index::snapshot index = local_content_index::load_snapshot();
            song_entry_state song =
                make_download_song_state(song_lookup.song, song_lookup.server_url, local_songs, index);
            if (!song.installed || song.update_available || song.song.status == content_status::modified) {
                result = download_song_package(song, song_lookup.server_url, progress);
                if (!result.success) {
                    promise.set_value(std::move(result));
                    return;
                }
                song.installed = true;
                song.installed_local_song_id = result.song_id;
            }

            const local_content_index::snapshot chart_index = local_content_index::load_snapshot();
            const chart_entry_state chart =
                make_download_chart_state(remote_chart, song_lookup.song, song_lookup.server_url, song, chart_index);
            result = download_chart_file(song, chart, song_lookup.server_url, progress);
            promise.set_value(std::move(result));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

bool poll_download(state& state, online_catalog::data_controller& data_controller) {
    if (!state.download_in_progress) {
        return false;
    }
    if (data_controller.download_future().wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    download_song_result result;
    try {
        result = data_controller.download_future().get();
    } catch (const std::exception& ex) {
        result.success = false;
        result.message = ex.what();
    } catch (...) {
        result.success = false;
        result.message = "Download failed.";
    }
    state.download_in_progress = false;
    state.download_progress.reset();

    if (result.success) {
        if (result.chart_only) {
            mark_chart_downloaded(state.official_songs, result.song_id, result.chart_id);
            mark_chart_downloaded(state.community_songs, result.song_id, result.chart_id);
            mark_chart_downloaded(state.owned_songs, result.song_id, result.chart_id);
        } else {
            mark_song_downloaded(state.official_songs, result.song_id);
            mark_song_downloaded(state.community_songs, result.song_id);
            mark_song_downloaded(state.owned_songs, result.song_id);
        }
        ui::notify(result.message, ui::notice_tone::success, 2.4f);
        reload_catalog(state, data_controller, true);
    } else {
        ui::notify(result.message.empty() ? "Download failed." : result.message,
                   ui::notice_tone::error, 3.2f);
    }
    return true;
}

void mark_song_removed(state& state, const std::string& song_id) {
    if (song_id.empty()) {
        return;
    }

    mark_song_not_installed(state.official_songs, song_id);
    mark_song_not_installed(state.community_songs, song_id);
    std::erase_if(state.owned_songs, [&](const song_entry_state& song) {
        return song.song.song.meta.song_id == song_id;
    });
    detail::ensure_selection_valid(state);
}

}  // namespace title_online_view
