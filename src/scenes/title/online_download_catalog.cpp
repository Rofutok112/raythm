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
#include "title/online_download_remote_client.h"
#include "title/upload_mapping_store.h"

namespace title_online_view {
namespace {

constexpr int kInitialSongPageSize = 12;
constexpr int kSongPageSize = 12;
constexpr int kChartPageSize = 8;

struct local_song_ref {
    const song_select::song_entry* song = nullptr;
    std::string local_song_id;
};

using local_song_lookup = std::unordered_map<std::string, local_song_ref>;

local_song_lookup build_local_lookup(const std::vector<song_select::song_entry>& local_songs,
                                     const std::string& server_url) {
    local_song_lookup lookup;
    const title_upload_mapping::store mappings = title_upload_mapping::load();
    for (const song_select::song_entry& song : local_songs) {
        const std::string& local_song_id = song.song.meta.song_id;
        lookup[local_song_id] = local_song_ref{
            .song = &song,
            .local_song_id = local_song_id,
        };

        const std::optional<std::string> remote_song_id =
            title_upload_mapping::find_remote_song_id(mappings, server_url, local_song_id);
        if (remote_song_id.has_value() && !remote_song_id->empty()) {
            lookup[*remote_song_id] = local_song_ref{
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

song_select::song_entry make_remote_song_entry(const remote_song_payload& song, const std::string& server_url) {
    song_select::song_entry entry;
    entry.song.meta.song_id = song.id;
    entry.song.meta.title = song.title;
    entry.song.meta.artist = song.artist;
    entry.song.meta.base_bpm = song.base_bpm;
    entry.song.meta.duration_seconds = song.duration_seconds;
    entry.song.meta.audio_url = make_absolute_remote_url(server_url, song.audio_url);
    entry.song.meta.jacket_url = make_absolute_remote_url(server_url, song.jacket_url);
    entry.song.meta.preview_start_ms = song.preview_start_ms;
    entry.song.meta.preview_start_seconds = static_cast<float>(song.preview_start_ms) / 1000.0f;
    entry.song.meta.song_version = song.song_version;
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
    state_entry.update_available = local_song.song != nullptr &&
                                   local_song.song->song.meta.song_version < state_entry.song.song.meta.song_version;
    state_entry.charts_loaded = false;
    state_entry.charts_loading = false;
    state_entry.charts_has_more = false;
    state_entry.charts_failed = false;
    state_entry.next_chart_page = 1;
    return state_entry;
}

song_entry_state build_owned_song_state(const song_select::song_entry& local_song,
                                        const remote_song_payload& remote_song,
                                        const std::string& server_url) {
    song_entry_state state_entry;
    state_entry.song = local_song;
    state_entry.song.song.meta.audio_url = make_absolute_remote_url(server_url, remote_song.audio_url);
    state_entry.song.song.meta.jacket_url = make_absolute_remote_url(server_url, remote_song.jacket_url);
    state_entry.song.song.meta.preview_start_ms = remote_song.preview_start_ms;
    state_entry.song.song.meta.preview_start_seconds =
        static_cast<float>(remote_song.preview_start_ms) / 1000.0f;
    state_entry.song.song.meta.duration_seconds = remote_song.duration_seconds;
    state_entry.installed = true;
    state_entry.installed_local_song_id = local_song.song.meta.song_id;
    state_entry.update_available = local_song.song.meta.song_version < remote_song.song_version;
    state_entry.charts_loaded = true;
    state_entry.charts_loading = false;
    state_entry.charts_has_more = false;
    state_entry.charts_failed = false;
    state_entry.next_chart_page = 1;
    state_entry.charts.reserve(local_song.charts.size());
    for (const song_select::chart_option& chart : local_song.charts) {
        state_entry.charts.push_back({
            chart,
            chart.meta.chart_id,
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

void append_chart_page(song_entry_state& song_state,
                       const std::vector<song_select::song_entry>& local_songs,
                       const remote_chart_page_fetch_result& page_result) {
    const title_upload_mapping::store mappings = title_upload_mapping::load();
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

        const std::optional<std::string> mapped_local_chart_id =
            title_upload_mapping::find_local_chart_id(mappings, page_result.server_url, chart.id);
        std::string installed_local_chart_id;
        const bool local_chart_installed = local_song != nullptr &&
            std::any_of(local_song->charts.begin(), local_song->charts.end(),
                        [&](const song_select::chart_option& local_chart) {
                            const bool matches = local_chart.meta.chart_id == chart.id ||
                                (mapped_local_chart_id.has_value() &&
                                 local_chart.meta.chart_id == *mapped_local_chart_id);
                            if (matches) {
                                installed_local_chart_id = local_chart.meta.chart_id;
                            }
                            return matches;
                        });

        song_state.charts.push_back({
            {
                {},
                chart_meta{
                    .chart_id = chart.id,
                    .song_id = chart.song_id,
                    .key_count = chart.key_count,
                    .difficulty = chart.difficulty_name,
                    .level = chart.level,
                    .chart_author = chart.chart_author,
                    .format_version = chart.format_version,
                    .resolution = chart.resolution,
                    .offset = chart.offset,
                },
                chart.content_source == "official" ? content_status::official : content_status::community,
                0,
                std::nullopt,
                0,
                0.0f,
                0.0f,
            },
            installed_local_chart_id,
            local_chart_installed,
            false,
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

int& next_page_ref(state& state, catalog_mode mode) {
    return mode == catalog_mode::community ? state.community_next_page : state.official_next_page;
}

bool& has_more_ref(state& state, catalog_mode mode) {
    return mode == catalog_mode::community ? state.community_has_more : state.official_has_more;
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

    const title_upload_mapping::store mappings = title_upload_mapping::load();
    owned.reserve(local_songs.size());
    for (const song_select::song_entry& local_song : local_songs) {
        const std::optional<title_upload_mapping::mapping_origin> origin =
            title_upload_mapping::find_song_origin(mappings, server_url, local_song.song.meta.song_id);
        if (origin.value_or(title_upload_mapping::mapping_origin::downloaded) !=
            title_upload_mapping::mapping_origin::owned_upload) {
            continue;
        }

        const std::string remote_song_id = title_upload_mapping::find_remote_song_id(
            mappings, server_url, local_song.song.meta.song_id).value_or(local_song.song.meta.song_id);
        const remote_song_lookup_result remote_song =
            fetch_remote_song_by_id(remote_song_id, server_url);
        if (!remote_song.success) {
            continue;
        }
        owned.push_back(build_owned_song_state(local_song, remote_song.song, remote_song.server_url));
    }

    std::sort(owned.begin(), owned.end(), [](const song_entry_state& left, const song_entry_state& right) {
        return left.song.song.meta.title < right.song.song.meta.title;
    });
    return owned;
}

catalog_load_result load_catalog_result() {
    const song_select::catalog_data local_catalog = song_select::load_catalog();

    const remote_song_page_fetch_result official_page =
        fetch_remote_song_page(catalog_mode::official, 1, kInitialSongPageSize);
    const std::string preferred_server = official_page.success ? official_page.server_url : "";
    const remote_song_page_fetch_result community_page =
        fetch_remote_song_page(catalog_mode::community, 1, kInitialSongPageSize, preferred_server);

    catalog_load_result result;
    result.local_songs = local_catalog.songs;
    result.catalog_request_failed = !official_page.success && !community_page.success;
    result.catalog_status_message = official_page.success ? community_page.error_message : official_page.error_message;
    result.catalog_server_url = official_page.success ? official_page.server_url : community_page.server_url;
    result.official_has_more = official_page.success &&
                               static_cast<int>(official_page.songs.size()) < official_page.total;
    result.community_has_more = community_page.success &&
                                static_cast<int>(community_page.songs.size()) < community_page.total;

    const local_song_lookup official_lookup = build_local_lookup(local_catalog.songs, official_page.server_url);
    const local_song_lookup community_lookup = build_local_lookup(local_catalog.songs, community_page.server_url);
    for (const remote_song_payload& song : official_page.songs) {
        result.official_songs.push_back(build_song_state(song, official_page.server_url, official_lookup));
    }
    for (const remote_song_payload& song : community_page.songs) {
        result.community_songs.push_back(build_song_state(song, community_page.server_url, community_lookup));
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

void reload_catalog(state& state) {
    if (state.catalog_loading) {
        return;
    }

    state.catalog_loading = true;
    state.catalog_request_failed = false;
    state.catalog_status_message.clear();
    state.official_songs.clear();
    state.community_songs.clear();
    state.owned_songs.clear();
    state.local_songs.clear();
    state.official_next_page = 2;
    state.community_next_page = 2;
    state.official_has_more = false;
    state.community_has_more = false;
    state.song_scroll_y = 0.0f;
    state.song_scroll_y_target = 0.0f;
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
    state.detail_open = false;
    state.song_page_loading = false;
    state.chart_page_loading = false;
    state.owned_loading = false;
    state.owned_loaded_once = false;
    state.jackets.clear();
    std::promise<catalog_load_result> promise;
    state.catalog_future = promise.get_future();
    std::thread([promise = std::move(promise)]() mutable {
        try {
            promise.set_value(load_catalog_result());
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
        result.catalog_status_message = ex.what();
    } catch (...) {
        result.catalog_request_failed = true;
        result.catalog_status_message = "Failed to load online catalog.";
    }
    state.catalog_loading = false;
    state.catalog_loaded_once = true;
    state.catalog_server_url = std::move(result.catalog_server_url);
    state.catalog_status_message = std::move(result.catalog_status_message);
    state.catalog_request_failed = result.catalog_request_failed;
    state.local_songs = std::move(result.local_songs);
    state.official_songs = std::move(result.official_songs);
    state.community_songs = std::move(result.community_songs);
    state.owned_songs = std::move(result.owned_songs);
    state.official_has_more = result.official_has_more;
    state.community_has_more = result.community_has_more;

    if (state.official_songs.empty() && !state.community_songs.empty()) {
        state.mode = catalog_mode::community;
    } else if (state.community_songs.empty() && !state.official_songs.empty()) {
        state.mode = catalog_mode::official;
    }

    detail::ensure_selection_valid(state);
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
    const int page = next_page_ref(state, mode);
    const std::string server_url = state.catalog_server_url;
    std::promise<remote_song_page_fetch_result> promise;
    state.song_page_future = promise.get_future();
    std::thread([promise = std::move(promise), mode, page, server_url]() mutable {
        try {
            promise.set_value(fetch_remote_song_page(mode, page, kSongPageSize, server_url));
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
        return true;
    }

    const local_song_lookup local_lookup = build_local_lookup(state.local_songs, page_result.server_url);
    std::vector<song_entry_state> page_items;
    page_items.reserve(page_result.songs.size());
    for (const remote_song_payload& song : page_result.songs) {
        page_items.push_back(build_song_state(song, page_result.server_url, local_lookup));
    }

    append_song_page(songs_for_mode(state, state.song_page_mode), std::move(page_items));
    has_more_ref(state, state.song_page_mode) =
        static_cast<int>(songs_for_mode(state, state.song_page_mode).size()) < page_result.total &&
        !page_result.songs.empty();
    if (has_more_ref(state, state.song_page_mode)) {
        next_page_ref(state, state.song_page_mode) += 1;
    }
    detail::ensure_selection_valid(state);
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
    const int page = song->next_chart_page;
    std::promise<remote_chart_page_fetch_result> promise;
    state.chart_page_future = promise.get_future();
    const std::string song_id = state.chart_page_song_id;
    std::thread([promise = std::move(promise), server_url, page, song_id]() mutable {
        try {
            promise.set_value(fetch_remote_chart_page(server_url, song_id, page, kChartPageSize));
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
        return true;
    }

    append_chart_page(*song, state.local_songs, page_result);
    song->charts_has_more = static_cast<int>(song->charts.size()) < page_result.total &&
                            !page_result.charts.empty();
    song->charts_loaded = !song->charts_has_more;
    if (song->charts_has_more) {
        song->next_chart_page += 1;
    }
    detail::ensure_selection_valid(state);
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

}  // namespace title_online_view
