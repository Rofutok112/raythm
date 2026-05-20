#include "title/online_download_internal.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "path_utils.h"
#include "song_select/song_catalog_service.h"
#include "title/local_content_index.h"
#include "title/online_download_remote_client.h"

namespace title_online_view {
namespace {

constexpr int kInitialSongPageSize = 12;
constexpr int kSongPageSize = 12;
constexpr int kChartPageSize = 8;
constexpr int kDiscoveryShelfSize = 12;

struct local_song_ref {
    const song_select::song_entry* song = nullptr;
    std::string local_song_id;
};

using local_song_lookup = std::unordered_map<std::string, local_song_ref>;

local_song_lookup build_local_lookup(const std::vector<song_select::song_entry>& local_songs,
                                     const std::string& server_url,
                                     const local_content_index::snapshot& index) {
    local_song_lookup lookup;
    for (const song_select::song_entry& song : local_songs) {
        const std::string& local_song_id = song.song.meta.song_id;
        lookup[local_song_id] = local_song_ref{
            .song = &song,
            .local_song_id = local_song_id,
        };

        const std::optional<local_content_index::online_song_binding> binding =
            local_content_index::find_song_by_local(index, server_url, local_song_id);
        if (binding.has_value() && !binding->remote_song_id.empty()) {
            lookup[binding->remote_song_id] = local_song_ref{
                .song = &song,
                .local_song_id = local_song_id,
            };
        }
    }
    return lookup;
}

const song_select::song_entry* find_local_song(const std::vector<song_select::song_entry>& local_songs,
                                               const std::string& song_id) {
    for (const song_select::song_entry& song : local_songs) {
        if (song.song.meta.song_id == song_id) {
            return &song;
        }
    }
    return nullptr;
}

content_status source_status_from_remote(const std::string& content_source) {
    return content_source == "official" ? content_status::official : content_status::community;
}

content_status remote_display_status(content_status local_status, content_status remote_source_status) {
    if (local_status == content_status::modified || local_status == content_status::update) {
        return local_status;
    }
    return remote_source_status;
}

song_select::song_entry make_remote_song_entry(const remote_song_payload& song, const std::string& server_url) {
    song_select::song_entry entry;
    entry.song.meta.song_id = song.id;
    entry.song.meta.title = song.title;
    entry.song.meta.artist = song.artist;
    entry.song.meta.genre = song.genre;
    entry.song.meta.genres = song.genres;
    entry.song.meta.keywords = song.keywords;
    entry.song.meta.base_bpm = song.base_bpm;
    entry.song.meta.offset = song.offset;
    entry.song.meta.has_offset = song.has_offset;
    entry.song.meta.timing_events = song.timing_events;
    entry.song.meta.duration_seconds = song.duration_seconds;
    entry.song.meta.audio_url = make_absolute_remote_url(server_url, song.audio_url);
    entry.song.meta.jacket_url = make_absolute_remote_url(server_url, song.jacket_url);
    entry.song.meta.preview_start_ms = song.preview_start_ms;
    entry.song.meta.preview_start_seconds = static_cast<float>(song.preview_start_ms) / 1000.0f;
    entry.song.meta.song_version = song.song_version;
    entry.song.meta.chart_count = song.chart_count;
    entry.song.meta.play_count = song.play_count;
    entry.song.meta.has_play_count = song.has_play_count;
    entry.source_status = source_status_from_remote(song.content_source);
    entry.song.directory.clear();
    return entry;
}

song_entry_state build_song_state(const remote_song_payload& remote_song,
                                  const std::string& server_url,
                                  const local_song_lookup& local_lookup) {
    song_entry_state state_entry;
    state_entry.song = make_remote_song_entry(remote_song, server_url);

    local_song_ref local_song;
    if (const auto it = local_lookup.find(remote_song.id); it != local_lookup.end()) {
        local_song = it->second;
    }

    state_entry.installed = local_song.song != nullptr;
    state_entry.installed_local_song_id = local_song.local_song_id;
    if (local_song.song != nullptr) {
        state_entry.song.status =
            remote_display_status(local_song.song->status, state_entry.song.source_status);
    }
    state_entry.update_available = local_song.song != nullptr &&
                                   local_song.song->song.meta.song_version < state_entry.song.song.meta.song_version;
    state_entry.charts_loaded = false;
    state_entry.charts_loading = false;
    state_entry.charts_has_more = false;
    state_entry.charts_failed = false;
    state_entry.next_chart_cursor.clear();
    return state_entry;
}

song_entry_state build_owned_song_state(const song_select::song_entry& local_song,
                                        const remote_song_payload& remote_song,
                                        const std::string& server_url,
                                        const local_content_index::snapshot& index) {
    song_entry_state state_entry;
    state_entry.song = local_song;
    state_entry.song.song.meta.song_id = remote_song.id;
    state_entry.song.song.meta.audio_url = make_absolute_remote_url(server_url, remote_song.audio_url);
    state_entry.song.song.meta.jacket_url = make_absolute_remote_url(server_url, remote_song.jacket_url);
    state_entry.song.song.meta.preview_start_ms = remote_song.preview_start_ms;
    state_entry.song.song.meta.preview_start_seconds =
        static_cast<float>(remote_song.preview_start_ms) / 1000.0f;
    state_entry.song.song.meta.duration_seconds = remote_song.duration_seconds;
    state_entry.song.song.meta.genres = remote_song.genres;
    state_entry.song.song.meta.keywords = remote_song.keywords;
    state_entry.song.song.meta.chart_count = remote_song.chart_count;
    state_entry.song.song.meta.play_count = remote_song.play_count;
    state_entry.song.song.meta.has_play_count = remote_song.has_play_count;
    state_entry.song.source_status = source_status_from_remote(remote_song.content_source);
    state_entry.installed = true;
    state_entry.installed_local_song_id = local_song.song.meta.song_id;
    state_entry.update_available = local_song.song.meta.song_version < remote_song.song_version;
    state_entry.charts_loaded = true;
    state_entry.charts_loading = false;
    state_entry.charts_has_more = false;
    state_entry.charts_failed = false;
    state_entry.next_chart_cursor.clear();
    state_entry.charts.reserve(local_song.charts.size());
    for (const song_select::chart_option& chart : local_song.charts) {
        song_select::chart_option remote_chart = chart;
        remote_chart.meta.song_id = remote_song.id;
        const std::optional<local_content_index::online_chart_binding> binding =
            local_content_index::find_chart_by_local(index, server_url, chart.meta.chart_id);
        remote_chart.meta.chart_id = binding.has_value() && !binding->remote_chart_id.empty()
            ? binding->remote_chart_id
            : chart.meta.chart_id;
        state_entry.charts.push_back({
            remote_chart,
            chart.meta.chart_id,
            {},
            true,
            false,
        });
    }
    return state_entry;
}

void append_song_page(std::vector<song_entry_state>& target, std::vector<song_entry_state> page_items) {
    for (song_entry_state& item : page_items) {
        const auto existing = std::find_if(target.begin(), target.end(), [&](const song_entry_state& song) {
            return song.song.song.meta.song_id == item.song.song.meta.song_id;
        });
        if (existing == target.end()) {
            target.push_back(std::move(item));
        }
    }
}

const char* shelf_key_for_view(discovery_view view) {
    switch (view) {
    case discovery_view::overview:
        return "";
    case discovery_view::new_arrivals:
        return "new";
    case discovery_view::rising:
        return "rising";
    case discovery_view::hidden_gems:
        return "hidden_gems";
    case discovery_view::recommended:
        return "recommended";
    case discovery_view::needs_charts:
        return "needs_charts";
    }
    return "";
}

std::vector<discovery_shelf_state> build_discovery_shelves(
    const remote_discovery_fetch_result& discovery,
    const local_song_lookup& local_lookup) {
    std::vector<discovery_shelf_state> shelves;
    shelves.reserve(discovery.shelves.size());
    for (const remote_discovery_shelf_payload& remote_shelf : discovery.shelves) {
        discovery_shelf_state shelf;
        shelf.key = remote_shelf.key;
        shelf.title = remote_shelf.title;
        shelf.songs.reserve(remote_shelf.songs.size());
        for (const remote_song_payload& remote_song : remote_shelf.songs) {
            shelf.songs.push_back(build_song_state(remote_song, discovery.server_url, local_lookup));
        }
        shelves.push_back(std::move(shelf));
    }
    return shelves;
}

void append_chart_page(song_entry_state& song_state,
                       const std::vector<song_select::song_entry>& local_songs,
                       const remote_chart_page_fetch_result& page_result) {
    const local_content_index::snapshot index = local_content_index::load_snapshot();
    const std::string local_song_id = !song_state.installed_local_song_id.empty()
        ? song_state.installed_local_song_id
        : song_state.song.song.meta.song_id;
    const song_select::song_entry* local_song = find_local_song(local_songs, local_song_id);
    for (const remote_chart_payload& chart : page_result.charts) {
        const auto exists = std::find_if(song_state.charts.begin(), song_state.charts.end(),
                                         [&](const chart_entry_state& state_chart) {
                                             return state_chart.chart.meta.chart_id == chart.id;
                                         });
        if (exists != song_state.charts.end()) {
            continue;
        }

        const std::optional<local_content_index::online_chart_binding> binding =
            local_content_index::find_chart_by_remote(index, page_result.server_url, chart.id);
        const std::optional<std::string> mapped_local_chart_id =
            binding.has_value() ? std::optional<std::string>(binding->local_chart_id) : std::nullopt;
        std::string installed_local_chart_id;
        const song_select::chart_option* installed_local_chart = nullptr;
        const bool local_chart_installed = local_song != nullptr &&
            std::any_of(local_song->charts.begin(), local_song->charts.end(),
                        [&](const song_select::chart_option& local_chart) {
                            const bool matches = local_chart.meta.chart_id == chart.id ||
                                (mapped_local_chart_id.has_value() &&
                                 local_chart.meta.chart_id == *mapped_local_chart_id);
                            if (matches) {
                                installed_local_chart_id = local_chart.meta.chart_id;
                                installed_local_chart = &local_chart;
                            }
                            return matches;
                        });
        const bool chart_update_available =
            local_chart_installed &&
            binding.has_value() &&
            chart.chart_version > (binding->remote_chart_version > 0
                ? binding->remote_chart_version
                : 1);

        const content_status chart_source_status = source_status_from_remote(chart.content_source);
        const content_status chart_status = installed_local_chart != nullptr
            ? remote_display_status(installed_local_chart->status, chart_source_status)
            : chart_source_status;
        song_state.charts.push_back({
            {
                {},
                chart_meta{
                    .chart_id = chart.id,
                    .song_id = chart.song_id,
                    .chart_version = chart.chart_version,
                    .key_count = chart.key_count,
                    .difficulty = chart.difficulty_name,
                    .level = chart.level,
                    .chart_author = chart.chart_author,
                    .format_version = chart.format_version,
                    .resolution = chart.resolution,
                    .offset = chart.offset,
                },
                chart_status,
                chart_source_status,
                0,
                std::nullopt,
                chart.note_count,
                chart.min_bpm > 0.0f ? chart.min_bpm : song_state.song.song.meta.base_bpm,
                chart.max_bpm > 0.0f ? chart.max_bpm : song_state.song.song.meta.base_bpm,
            },
            installed_local_chart_id,
            chart.uploader_id,
            local_chart_installed,
            chart_update_available,
        });
    }

    std::sort(song_state.charts.begin(), song_state.charts.end(),
              [](const chart_entry_state& left, const chart_entry_state& right) {
                  if (left.chart.meta.key_count != right.chart.meta.key_count) {
                      return left.chart.meta.key_count < right.chart.meta.key_count;
                  }
                  if (left.chart.meta.level != right.chart.meta.level) {
                      return left.chart.meta.level < right.chart.meta.level;
                  }
                  return left.chart.meta.difficulty < right.chart.meta.difficulty;
              });
}

song_entry_state* find_song_state(std::vector<song_entry_state>& songs, const std::string& song_id) {
    for (song_entry_state& song : songs) {
        if (song.song.song.meta.song_id == song_id) {
            return &song;
        }
    }
    return nullptr;
}

std::vector<song_entry_state>& songs_for_mode(state& state, catalog_mode mode) {
    switch (mode) {
    case catalog_mode::official:
        return state.official_songs;
    case catalog_mode::community:
        return state.community_songs;
    case catalog_mode::owned:
        return state.owned_songs;
    }
    return state.official_songs;
}

std::string& next_cursor_ref(state& state, catalog_mode mode) {
    return mode == catalog_mode::community ? state.community_next_cursor : state.official_next_cursor;
}

bool& has_more_ref(state& state, catalog_mode mode) {
    return mode == catalog_mode::community ? state.community_has_more : state.official_has_more;
}

void clear_reload_restore(state& state) {
    state.reload_preserve_view = false;
    state.reload_restore_detail_open = false;
    state.reload_restore_mode = state.mode;
    state.reload_restore_song_id.clear();
    state.reload_restore_chart_id.clear();
}

void capture_reload_restore(state& state, bool preserve_view) {
    clear_reload_restore(state);
    if (!preserve_view) {
        return;
    }

    state.reload_preserve_view = true;
    state.reload_restore_mode = state.mode;
    state.reload_restore_detail_open = state.detail_open;
    if (const song_entry_state* song = selected_song(state); song != nullptr) {
        state.reload_restore_song_id = song->song.song.meta.song_id;
    }
    if (const chart_entry_state* chart = selected_chart(state); chart != nullptr) {
        state.reload_restore_chart_id = chart->chart.meta.chart_id;
    }
}

void apply_reload_restore(state& state) {
    if (!state.reload_preserve_view) {
        return;
    }

    state.mode = state.reload_restore_mode;
    auto& songs = songs_for_mode(state, state.mode);
    if (songs.empty()) {
        state.detail_open = false;
        clear_reload_restore(state);
        return;
    }

    if (!state.reload_restore_song_id.empty()) {
        const auto song_it = std::find_if(songs.begin(), songs.end(), [&](const song_entry_state& song) {
            return song.song.song.meta.song_id == state.reload_restore_song_id;
        });
        if (song_it != songs.end()) {
            detail::selected_song_index_ref(state) = static_cast<int>(song_it - songs.begin());
        }
    }

    detail::ensure_selection_valid(state);

    if (!state.reload_restore_detail_open) {
        clear_reload_restore(state);
        return;
    }

    state.detail_open = true;
    const song_entry_state* song = selected_song(state);
    if (song == nullptr) {
        state.detail_open = false;
        clear_reload_restore(state);
        return;
    }

    if (!state.reload_restore_chart_id.empty() && !song->charts.empty()) {
        const auto chart_it = std::find_if(song->charts.begin(), song->charts.end(),
                                           [&](const chart_entry_state& chart) {
            return chart.chart.meta.chart_id == state.reload_restore_chart_id;
        });
        if (chart_it != song->charts.end()) {
            detail::selected_chart_index_ref(state) = static_cast<int>(chart_it - song->charts.begin());
            state.reload_restore_chart_id.clear();
        }
    }

    if (state.reload_restore_chart_id.empty() ||
        (song->charts_loaded && !song->charts_loading && !song->charts_has_more)) {
        clear_reload_restore(state);
    }
}

bool select_installed_song_in(std::vector<song_entry_state>& songs,
                              const std::string& local_song_id,
                              int& selected_index) {
    if (local_song_id.empty()) {
        return false;
    }
    const auto song_it = std::find_if(songs.begin(), songs.end(), [&](const song_entry_state& song) {
        return song.installed_local_song_id == local_song_id ||
               song.song.song.meta.song_id == local_song_id;
    });
    if (song_it == songs.end()) {
        return false;
    }
    selected_index = static_cast<int>(song_it - songs.begin());
    return true;
}

bool apply_pending_select(state& state) {
    if (state.pending_select_local_song_id.empty()) {
        return false;
    }

    const std::string local_song_id = state.pending_select_local_song_id;
    bool selected = false;
    if (select_installed_song_in(state.official_songs, local_song_id, state.official_selected_song_index)) {
        state.mode = catalog_mode::official;
        selected = true;
    } else if (select_installed_song_in(state.community_songs, local_song_id, state.community_selected_song_index)) {
        state.mode = catalog_mode::community;
        selected = true;
    } else if (select_installed_song_in(state.owned_songs, local_song_id, state.owned_selected_song_index)) {
        state.mode = catalog_mode::owned;
        selected = true;
    }

    if (!selected) {
        return false;
    }

    state.detail_open = state.pending_select_detail_open;
    state.song_scroll_y = 0.0f;
    state.song_scroll_y_target = 0.0f;
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
    detail::ensure_selection_valid(state);
    std::vector<song_entry_state>& selected_songs = songs_for_mode(state, state.mode);
    const int selected_index = detail::selected_song_index_ref(state);
    song_entry_state* selected_song_state =
        selected_index >= 0 && selected_index < static_cast<int>(selected_songs.size())
            ? &selected_songs[static_cast<size_t>(selected_index)]
            : nullptr;
    if (selected_song_state != nullptr) {
        selected_song_state->update_available = selected_song_state->update_available ||
                                                state.pending_select_local_chart_id.empty();
    }
    if (state.detail_open) {
        if (!state.pending_select_local_chart_id.empty()) {
            song_entry_state* song = selected_song_state;
            if (song != nullptr) {
                const auto chart_it = std::find_if(song->charts.begin(), song->charts.end(),
                                                   [&](const chart_entry_state& chart) {
                    return chart.installed_local_chart_id == state.pending_select_local_chart_id ||
                           chart.chart.meta.chart_id == state.pending_select_local_chart_id;
                });
                if (chart_it != song->charts.end()) {
                    chart_it->update_available = true;
                    detail::selected_chart_index_ref(state) = static_cast<int>(chart_it - song->charts.begin());
                    state.pending_select_local_chart_id.clear();
                } else if (song->charts_loaded && !song->charts_loading && !song->charts_has_more) {
                    state.pending_select_local_chart_id.clear();
                }
            }
        }
        request_charts_for_selected_song(state);
    }
    if (state.pending_select_local_chart_id.empty()) {
        state.pending_select_local_song_id.clear();
        state.pending_select_detail_open = false;
    }
    return true;
}

jacket_cache::pending_texture load_local_texture_bytes(const std::filesystem::path& path) {
    jacket_cache::pending_texture result;
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return result;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size <= 0) {
        return result;
    }

    result.bytes.resize(static_cast<size_t>(size));
    input.read(reinterpret_cast<char*>(result.bytes.data()), size);
    if (!input.good() && !input.eof()) {
        result.bytes.clear();
        return result;
    }

    result.file_type = path.extension().string();
    return result;
}

jacket_cache::pending_texture load_remote_texture_bytes(const song_data& song) {
    jacket_cache::pending_texture result;
    const remote_binary_fetch_result fetched = fetch_remote_binary(song.meta.jacket_url);
    if (!fetched.success || fetched.bytes.empty()) {
        return result;
    }

    result.bytes = fetched.bytes;
    const std::string lower_type = fetched.content_type;
    if (lower_type.rfind("image/png", 0) == 0) {
        result.file_type = ".png";
    } else if (lower_type.rfind("image/jpeg", 0) == 0 || lower_type.rfind("image/jpg", 0) == 0) {
        result.file_type = ".jpg";
    } else if (lower_type.rfind("image/bmp", 0) == 0) {
        result.file_type = ".bmp";
    } else if (lower_type.rfind("image/gif", 0) == 0) {
        result.file_type = ".gif";
    } else if (lower_type.rfind("image/webp", 0) == 0) {
        result.file_type = ".webp";
    }

    if (result.file_type.empty()) {
        if (result.bytes.size() >= 4 &&
            result.bytes[0] == 0x89 && result.bytes[1] == 0x50 &&
            result.bytes[2] == 0x4E && result.bytes[3] == 0x47) {
            result.file_type = ".png";
        } else if (result.bytes.size() >= 3 &&
                   result.bytes[0] == 0xFF && result.bytes[1] == 0xD8 && result.bytes[2] == 0xFF) {
            result.file_type = ".jpg";
        }
    }
    return result;
}

Texture2D load_texture_from_pending(const jacket_cache::pending_texture& pending) {
    if (pending.bytes.empty() || pending.file_type.empty()) {
        return {};
    }

    Image image = LoadImageFromMemory(pending.file_type.c_str(),
                                      pending.bytes.data(),
                                      static_cast<int>(pending.bytes.size()));
    if (image.data == nullptr) {
        return {};
    }

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id != 0) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    }
    return texture;
}

std::vector<song_entry_state> load_owned_songs(const std::vector<song_select::song_entry>& local_songs,
                                               const std::string& server_url) {
    std::vector<song_entry_state> owned;
    if (server_url.empty()) {
        return owned;
    }

    const local_content_index::snapshot index = local_content_index::load_snapshot();
    owned.reserve(local_songs.size());
    for (const song_select::song_entry& local_song : local_songs) {
        const std::optional<local_content_index::online_song_binding> binding =
            local_content_index::find_song_by_local(index, server_url, local_song.song.meta.song_id);
        if (!binding.has_value() || binding->origin != local_content_index::online_origin::owned_upload ||
            binding->remote_song_id.empty()) {
            continue;
        }

        const remote_song_lookup_result remote_song =
            fetch_remote_song_by_id(binding->remote_song_id, server_url);
        if (!remote_song.success) {
            continue;
        }
        owned.push_back(build_owned_song_state(local_song, remote_song.song, remote_song.server_url, index));
    }

    std::sort(owned.begin(), owned.end(), [](const song_entry_state& left, const song_entry_state& right) {
        return left.song.song.meta.title < right.song.song.meta.title;
    });
    return owned;
}

catalog_load_result load_catalog_result(source_filter source) {
    const song_select::catalog_data local_catalog = song_select::load_catalog(true);

    const remote_discovery_fetch_result discovery =
        fetch_remote_discovery(source, kDiscoveryShelfSize);

    catalog_load_result result;
    result.local_songs = local_catalog.songs;
    result.catalog_request_failed = !discovery.success;
    result.catalog_status_message = discovery.error_message;
    result.catalog_maintenance = discovery.maintenance;
    result.catalog_retry_after = discovery.retry_after;
    result.catalog_server_url = discovery.server_url;
    result.official_has_more = false;
    result.community_has_more = false;

    const local_content_index::snapshot index = local_content_index::load_snapshot();
    const local_song_lookup local_lookup =
        build_local_lookup(local_catalog.songs, discovery.server_url, index);
    if (discovery.success) {
        result.discovery_shelves = build_discovery_shelves(discovery, local_lookup);
    }

    return result;
}

void start_owned_reload(state& state) {
    if (state.owned_loading || state.catalog_server_url.empty()) {
        return;
    }

    state.owned_loading = true;
    std::promise<std::vector<song_entry_state>> promise;
    state.owned_future = promise.get_future();
    const std::vector<song_select::song_entry> local_songs = state.local_songs;
    const std::string server_url = state.catalog_server_url;
    std::thread([promise = std::move(promise), local_songs, server_url]() mutable {
        try {
            promise.set_value(load_owned_songs(local_songs, server_url));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

}  // namespace

namespace detail {

void rebuild_visible_discovery_songs(state& state) {
    state.official_songs.clear();
    const std::string target_key = shelf_key_for_view(state.view);
    std::vector<std::string> seen_song_ids;

    auto append_unique = [&](const song_entry_state& song) {
        const std::string& song_id = song.song.song.meta.song_id;
        if (std::find(seen_song_ids.begin(), seen_song_ids.end(), song_id) != seen_song_ids.end()) {
            return;
        }
        seen_song_ids.push_back(song_id);
        state.official_songs.push_back(song);
    };

    for (const discovery_shelf_state& shelf : state.discovery_shelves) {
        if (!target_key.empty() && shelf.key != target_key) {
            continue;
        }
        for (const song_entry_state& song : shelf.songs) {
            append_unique(song);
        }
    }

    state.mode = catalog_mode::official;
    state.song_scroll_y = 0.0f;
    state.song_scroll_y_target = 0.0f;
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
}

}  // namespace detail

jacket_cache::~jacket_cache() {
    clear();
}

const Texture2D* jacket_cache::get(const song_data& song) {
    poll();

    const std::string key = song.meta.song_id;
    auto& entry = entries_[key];
    if (entry.loaded) {
        return &entry.texture;
    }
    if (entry.missing || entry.requested) {
        return nullptr;
    }

    entry.requested = true;
    std::promise<pending_texture> promise;
    entry.future = promise.get_future();
    const song_data song_copy = song;
    std::thread([promise = std::move(promise), song_copy]() mutable {
        try {
            if (!song_copy.meta.jacket_file.empty()) {
                const std::filesystem::path jacket_path =
                    path_utils::join_utf8(song_copy.directory, song_copy.meta.jacket_file);
                if (std::filesystem::exists(jacket_path) && std::filesystem::is_regular_file(jacket_path)) {
                    promise.set_value(load_local_texture_bytes(jacket_path));
                    return;
                }
            }

            if (!song_copy.meta.jacket_url.empty()) {
                promise.set_value(load_remote_texture_bytes(song_copy));
                return;
            }

            promise.set_value({});
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
    return nullptr;
}

void jacket_cache::poll() {
    for (auto& [_, entry] : entries_) {
        if (entry.loaded || entry.missing || !entry.requested) {
            continue;
        }
        if (!entry.future.valid()) {
            entry.missing = true;
            continue;
        }
        if (entry.future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            continue;
        }

        try {
            pending_texture pending = entry.future.get();
            entry.texture = load_texture_from_pending(pending);
            entry.loaded = entry.texture.id != 0;
            entry.missing = !entry.loaded;
        } catch (...) {
            entry.missing = true;
        }
    }
}

void jacket_cache::clear() {
    for (auto& [_, entry] : entries_) {
        if (entry.loaded) {
            UnloadTexture(entry.texture);
        }
    }
    entries_.clear();
}

void reload_catalog(state& state, bool preserve_view) {
    if (state.catalog_loading) {
        return;
    }

    capture_reload_restore(state, preserve_view);
    state.catalog_loading = true;
    state.catalog_request_failed = false;
    state.catalog_maintenance = false;
    state.catalog_status_message.clear();
    state.catalog_retry_after.clear();
    state.official_next_cursor.clear();
    state.community_next_cursor.clear();
    state.official_has_more = false;
    state.community_has_more = false;
    state.song_page_loading = false;
    state.chart_page_loading = false;
    state.owned_loading = false;
    state.owned_loaded_once = false;
    if (!preserve_view) {
        state.official_songs.clear();
        state.community_songs.clear();
        state.owned_songs.clear();
        state.discovery_shelves.clear();
        state.overview_shelf_scroll_x.clear();
        state.overview_shelf_scroll_x_target.clear();
        state.local_songs.clear();
        state.song_scroll_y = 0.0f;
        state.song_scroll_y_target = 0.0f;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        state.detail_open = false;
        state.jackets.clear();
    }
    std::promise<catalog_load_result> promise;
    state.catalog_future = promise.get_future();
    const source_filter source = state.source;
    std::thread([promise = std::move(promise), source]() mutable {
        try {
            promise.set_value(load_catalog_result(source));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

bool poll_catalog(state& state) {
    if (!state.catalog_loading) {
        return false;
    }
    if (state.catalog_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    catalog_load_result result;
    try {
        result = state.catalog_future.get();
    } catch (const std::exception& ex) {
        result.catalog_request_failed = true;
        result.catalog_maintenance = false;
        result.catalog_status_message = ex.what();
    } catch (...) {
        result.catalog_request_failed = true;
        result.catalog_maintenance = false;
        result.catalog_status_message = "Failed to load online catalog.";
    }
    state.catalog_loading = false;
    state.catalog_loaded_once = true;
    state.catalog_server_url = std::move(result.catalog_server_url);
    state.catalog_status_message = std::move(result.catalog_status_message);
    state.catalog_retry_after = std::move(result.catalog_retry_after);
    state.catalog_request_failed = result.catalog_request_failed;
    state.catalog_maintenance = result.catalog_maintenance;
    state.local_songs = std::move(result.local_songs);
    state.discovery_shelves = std::move(result.discovery_shelves);
    state.official_songs = std::move(result.official_songs);
    state.community_songs = std::move(result.community_songs);
    state.owned_songs = std::move(result.owned_songs);
    state.official_has_more = result.official_has_more;
    state.community_has_more = result.community_has_more;

    detail::rebuild_visible_discovery_songs(state);
    detail::ensure_selection_valid(state);
    apply_reload_restore(state);
    apply_pending_select(state);
    start_owned_reload(state);
    return true;
}

void request_next_song_page(state& state, catalog_mode mode) {
    if (mode == catalog_mode::owned || state.song_page_loading || state.catalog_loading) {
        return;
    }
    if (state.catalog_server_url.empty() || !has_more_ref(state, mode)) {
        return;
    }

    state.song_page_loading = true;
    state.song_page_mode = mode;
    const std::string cursor = next_cursor_ref(state, mode);
    const std::string server_url = state.catalog_server_url;
    std::promise<remote_song_page_fetch_result> promise;
    state.song_page_future = promise.get_future();
    std::thread([promise = std::move(promise), mode, cursor, server_url]() mutable {
        try {
            promise.set_value(fetch_remote_song_page(mode, cursor, kSongPageSize, server_url));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

bool poll_song_page(state& state) {
    if (!state.song_page_loading) {
        return false;
    }
    if (state.song_page_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    remote_song_page_fetch_result page_result;
    try {
        page_result = state.song_page_future.get();
    } catch (const std::exception& ex) {
        page_result.success = false;
        page_result.error_message = ex.what();
    } catch (...) {
        page_result.success = false;
        page_result.error_message = "Failed to load more songs.";
    }
    state.song_page_loading = false;
    if (!page_result.success) {
        state.catalog_status_message = page_result.error_message;
        state.catalog_retry_after = page_result.retry_after;
        state.catalog_maintenance = page_result.maintenance;
        return true;
    }

    const local_content_index::snapshot index = local_content_index::load_snapshot();
    const local_song_lookup local_lookup = build_local_lookup(state.local_songs, page_result.server_url, index);
    std::vector<song_entry_state> page_items;
    page_items.reserve(page_result.songs.size());
    for (const remote_song_payload& song : page_result.songs) {
        page_items.push_back(build_song_state(song, page_result.server_url, local_lookup));
    }

    append_song_page(songs_for_mode(state, state.song_page_mode), std::move(page_items));
    has_more_ref(state, state.song_page_mode) = page_result.has_more && !page_result.next_cursor.empty();
    if (has_more_ref(state, state.song_page_mode)) {
        next_cursor_ref(state, state.song_page_mode) = page_result.next_cursor;
    }
    detail::ensure_selection_valid(state);
    apply_reload_restore(state);
    apply_pending_select(state);
    return true;
}

void request_charts_for_selected_song(state& state) {
    if (state.chart_page_loading || state.catalog_server_url.empty()) {
        return;
    }

    const song_entry_state* selected = selected_song(state);
    song_entry_state* song = selected == nullptr
        ? nullptr
        : find_song_state(songs_for_mode(state, state.mode), selected->song.song.meta.song_id);
    if (song == nullptr || song->charts_loading) {
        return;
    }
    if (song->charts_loaded && !song->charts_has_more) {
        return;
    }

    song->charts_loading = true;
    song->charts_failed = false;
    state.chart_page_loading = true;
    state.chart_page_mode = state.mode;
    state.chart_page_song_id = song->song.song.meta.song_id;
    const std::string server_url = state.catalog_server_url;
    const std::string cursor = song->next_chart_cursor;
    std::promise<remote_chart_page_fetch_result> promise;
    state.chart_page_future = promise.get_future();
    const std::string song_id = state.chart_page_song_id;
    std::thread([promise = std::move(promise), server_url, cursor, song_id]() mutable {
        try {
            promise.set_value(fetch_remote_chart_page(server_url, song_id, cursor, kChartPageSize));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

bool poll_chart_page(state& state) {
    if (!state.chart_page_loading) {
        return false;
    }
    if (state.chart_page_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    remote_chart_page_fetch_result page_result;
    try {
        page_result = state.chart_page_future.get();
    } catch (...) {
        page_result.success = false;
    }
    state.chart_page_loading = false;
    song_entry_state* song = find_song_state(songs_for_mode(state, state.chart_page_mode), state.chart_page_song_id);
    if (song == nullptr) {
        return true;
    }

    song->charts_loading = false;
    if (!page_result.success) {
        song->charts_failed = true;
        song->charts_loaded = true;
        state.catalog_status_message = page_result.error_message;
        state.catalog_retry_after = page_result.retry_after;
        state.catalog_maintenance = page_result.maintenance;
        return true;
    }

    append_chart_page(*song, state.local_songs, page_result);
    song->charts_has_more = page_result.has_more && !page_result.next_cursor.empty();
    song->charts_loaded = !song->charts_has_more;
    if (song->charts_has_more) {
        song->next_chart_cursor = page_result.next_cursor;
    }
    detail::ensure_selection_valid(state);
    apply_reload_restore(state);
    apply_pending_select(state);
    return true;
}

bool poll_owned(state& state) {
    if (!state.owned_loading) {
        return false;
    }
    if (state.owned_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    state.owned_loading = false;
    state.owned_loaded_once = true;
    try {
        state.owned_songs = state.owned_future.get();
    } catch (...) {
        state.owned_songs.clear();
    }
    detail::ensure_selection_valid(state);
    apply_reload_restore(state);
    apply_pending_select(state);
    return true;
}

void on_enter(state& state, song_select::preview_controller& preview_controller) {
    if (!state.catalog_loaded_once && !state.catalog_loading) {
        reload_catalog(state);
    }
    state.detail_transition = state.detail_open ? 1.0f : 0.0f;
    preview_controller.select_song(preview_song(state));
}

void on_exit(state& state) {
    state.detail_transition = 0.0f;
    state.jackets.clear();
}

void select_local_update_target(state& state,
                                const std::string& local_song_id,
                                const std::string& local_chart_id,
                                bool open_detail) {
    state.pending_select_local_song_id = local_song_id;
    state.pending_select_local_chart_id = local_chart_id;
    state.pending_select_detail_open = open_detail;
    if (apply_pending_select(state)) {
        return;
    }
    if (!state.catalog_loading) {
        reload_catalog(state, true);
    }
}

}  // namespace title_online_view
