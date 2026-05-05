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
#include "chart_parser.h"
#include "chart_serializer.h"
#include "network/json_helpers.h"
#include "path_utils.h"
#include "song_writer.h"
#include "title/local_content_index.h"
#include "title/online_download_remote_client.h"
#include "ui_notice.h"

namespace title_online_view {
namespace {
namespace json = network::json;

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

bool write_chart_file(const std::filesystem::path& path,
                      const std::vector<unsigned char>& bytes,
                      const std::string& local_song_id,
                      std::string& error_message) {
    std::filesystem::create_directories(path.parent_path());
    const std::filesystem::path temp_path = path.parent_path() / (path.filename().string() + ".download.tmp");
    if (!write_binary_file(temp_path, bytes, error_message)) {
        return false;
    }

    const chart_parse_result parsed = chart_parser::parse(path_utils::to_utf8(temp_path));
    if (!parsed.success || !parsed.data.has_value()) {
        std::filesystem::remove(temp_path);
        error_message = parsed.errors.empty() ? "Downloaded chart file was invalid." : parsed.errors.front();
        return false;
    }

    chart_data data = *parsed.data;
    data.meta.song_id = local_song_id;
    if (!chart_serializer::serialize(data, path_utils::to_utf8(path))) {
        std::filesystem::remove(temp_path);
        error_message = "Failed to write downloaded chart data to disk.";
        return false;
    }

    std::filesystem::remove(temp_path);
    return true;
}

std::optional<song_meta> parse_downloaded_song_metadata(const std::string& metadata_json,
                                                        const std::string& local_song_id,
                                                        std::string& error_message) {
    song_meta meta;
    meta.song_id = local_song_id;
    meta.title = trim_ascii(json::extract_string(metadata_json, "title").value_or(""));
    meta.artist = trim_ascii(json::extract_string(metadata_json, "artist").value_or(""));
    meta.genre = trim_ascii(json::extract_string(metadata_json, "genre").value_or(""));
    meta.audio_file = trim_ascii(json::extract_string(metadata_json, "audioFile").value_or(""));
    meta.jacket_file = trim_ascii(json::extract_string(metadata_json, "jacketFile").value_or(""));
    meta.base_bpm = json::extract_float(metadata_json, "baseBpm").value_or(0.0f);
    meta.preview_start_ms = json::extract_int(metadata_json, "previewStartMs").value_or(0);
    meta.preview_start_seconds = static_cast<float>(meta.preview_start_ms) / 1000.0f;
    meta.song_version = json::extract_int(metadata_json, "songVersion").value_or(1);

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
        parse_downloaded_song_metadata(metadata_json, local_song_id, error_message);
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
    const std::filesystem::path audio_path = song_dir / path_utils::from_utf8(local_meta->audio_file);
    const std::filesystem::path jacket_path = song_dir / path_utils::from_utf8(local_meta->jacket_file);

    std::error_code ec;
    std::filesystem::remove_all(song_dir, ec);
    ec.clear();

    if (!song_writer::write_song_json(*local_meta, path_utils::to_utf8(song_dir))) {
        result.message = "Failed to write downloaded song metadata to disk.";
        return result;
    }

    if (!write_binary_file(audio_path, audio_fetch.bytes, error_message)) {
        result.message = error_message;
        return result;
    }

    if (!local_meta->jacket_file.empty() && !write_binary_file(jacket_path, jacket_bytes, error_message)) {
        result.message = error_message;
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
    });

    result.success = true;
    result.message = "Song downloaded.";
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
    if (!write_chart_file(app_paths::song_chart_path(local_song_id, local_chart_id),
                          chart_fetch.bytes,
                          local_song_id,
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
        });
    }
    local_content_index::put_chart_binding({
        .server_url = server_url,
        .local_chart_id = local_chart_id,
        .remote_chart_id = result.chart_id,
        .remote_song_id = result.song_id,
        .remote_chart_version = chart.chart.meta.chart_version,
        .origin = local_content_index::online_origin::downloaded,
    });

    result.success = true;
    result.message = "Chart downloaded.";
    return result;
}

}  // namespace

bool needs_download(const song_entry_state& song) {
    return !song.installed || song.update_available;
}

void start_download(state& state) {
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
    state.download_future = promise.get_future();
    std::thread([promise = std::move(promise), selected, server_url, progress]() mutable {
        try {
            promise.set_value(download_song_package(selected, server_url, progress));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void start_chart_download(state& state) {
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
    if (chart->installed && !chart->update_available) {
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
    state.download_future = promise.get_future();
    std::thread([promise = std::move(promise), selected_song, selected_chart, server_url, progress]() mutable {
        try {
            promise.set_value(download_chart_file(selected_song, selected_chart, server_url, progress));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

bool poll_download(state& state) {
    if (!state.download_in_progress) {
        return false;
    }
    if (state.download_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    download_song_result result;
    try {
        result = state.download_future.get();
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
        reload_catalog(state, true);
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
