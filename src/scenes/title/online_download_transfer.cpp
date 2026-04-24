#include "title/online_download_internal.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "app_paths.h"
#include "network/json_helpers.h"
#include "path_utils.h"
#include "song_loader.h"
#include "title/online_download_remote_client.h"
#include "ui_notice.h"

namespace title_online_view {
namespace {
namespace json = network::json;

struct staged_chart_file {
    std::string chart_id;
    std::vector<unsigned char> bytes;
};

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

void remove_existing_local_charts_for_song(const std::string& song_id) {
    if (song_id.empty()) {
        return;
    }

    const std::filesystem::path charts_root = app_paths::charts_root();
    if (!std::filesystem::exists(charts_root) || !std::filesystem::is_directory(charts_root)) {
        return;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(charts_root, ec)) {
        if (ec) {
            return;
        }
        if (!entry.is_regular_file() || entry.path().extension() != ".rchart") {
            continue;
        }

        const chart_parse_result parse_result = song_loader::load_chart(path_utils::to_utf8(entry.path()));
        if (!parse_result.success || !parse_result.data.has_value()) {
            continue;
        }

        if (parse_result.data->meta.song_id != song_id) {
            continue;
        }

        std::filesystem::remove(entry.path(), ec);
        if (ec) {
            return;
        }
    }
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
        for (chart_entry_state& chart : song.charts) {
            chart.installed = true;
            chart.update_available = false;
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

    if (server_url.empty() || result.song_id.empty()) {
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

    const std::string metadata_json(metadata_fetch.bytes.begin(), metadata_fetch.bytes.end());
    const std::string audio_file = trim_ascii(json::extract_string(metadata_json, "audioFile").value_or(""));
    const std::string jacket_file = trim_ascii(json::extract_string(metadata_json, "jacketFile").value_or(""));
    if (audio_file.empty()) {
        result.message = "Downloaded song metadata did not include audioFile.";
        return result;
    }

    const int total_steps =
        2 + (jacket_file.empty() ? 0 : 1) + static_cast<int>(song.charts.size());
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
    if (!jacket_file.empty()) {
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

    std::vector<staged_chart_file> staged_charts;
    staged_charts.reserve(song.charts.size());
    for (const chart_entry_state& chart_entry : song.charts) {
        if (chart_entry.chart.meta.chart_id.empty()) {
            result.message = "A remote chart is missing chart_id.";
            return result;
        }

        const std::string chart_url = make_absolute_remote_url(
            server_url, "/charts/" + chart_entry.chart.meta.chart_id + "/file");
        const remote_binary_fetch_result chart_fetch = begin_step(chart_url);
        if (!chart_fetch.success || chart_fetch.bytes.empty()) {
            result.message = chart_fetch.error_message.empty()
                ? "Failed to download a chart file."
                : chart_fetch.error_message;
            return result;
        }
        finish_step();

        staged_chart_file staged_chart;
        staged_chart.chart_id = chart_entry.chart.meta.chart_id;
        staged_chart.bytes = chart_fetch.bytes;
        staged_charts.push_back(std::move(staged_chart));
    }

    app_paths::ensure_directories();
    const std::filesystem::path song_dir = app_paths::song_dir(result.song_id);
    const std::filesystem::path song_json_path = song_dir / "song.json";
    const std::filesystem::path audio_path = song_dir / path_utils::from_utf8(audio_file);
    const std::filesystem::path jacket_path = song_dir / path_utils::from_utf8(jacket_file);

    std::string error_message;
    std::error_code ec;
    std::filesystem::remove_all(song_dir, ec);
    ec.clear();

    remove_existing_local_charts_for_song(result.song_id);

    if (!write_binary_file(song_json_path, metadata_fetch.bytes, error_message) ||
        !write_binary_file(audio_path, audio_fetch.bytes, error_message)) {
        result.message = error_message;
        return result;
    }

    if (!jacket_file.empty() && !write_binary_file(jacket_path, jacket_bytes, error_message)) {
        result.message = error_message;
        return result;
    }

    for (const staged_chart_file& chart : staged_charts) {
        if (!write_binary_file(app_paths::chart_path(chart.chart_id), chart.bytes, error_message)) {
            result.message = error_message;
            return result;
        }
    }

    result.success = true;
    result.message = "Song downloaded.";
    return result;
}

}  // namespace

bool needs_download(const song_entry_state& song) {
    if (!song.installed || song.update_available) {
        return true;
    }

    return std::any_of(song.charts.begin(), song.charts.end(), [](const chart_entry_state& chart) {
        return !chart.installed || chart.update_available;
    });
}

void start_download(state& state) {
    if (state.download_in_progress) {
        return;
    }

    const song_entry_state* song = selected_song(state);
    if (song == nullptr || !needs_download(*song) || !song->charts_loaded) {
        return;
    }

    state.download_in_progress = true;
    const song_entry_state selected = *song;
    const std::string server_url = state.catalog_server_url;
    state.download_progress = std::make_shared<download_progress_state>();
    ui::push_notice(state.notices, "Downloading song...", ui::notice_tone::info, 1.8f);
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
        result.message = "Song download failed.";
    }
    state.download_in_progress = false;
    state.download_progress.reset();

    if (result.success) {
        mark_song_downloaded(state.official_songs, result.song_id);
        mark_song_downloaded(state.community_songs, result.song_id);
        mark_song_downloaded(state.owned_songs, result.song_id);
        ui::push_notice(state.notices, result.message, ui::notice_tone::success, 2.4f);
        reload_catalog(state);
    } else {
        ui::push_notice(state.notices,
                        result.message.empty() ? "Song download failed." : result.message,
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
