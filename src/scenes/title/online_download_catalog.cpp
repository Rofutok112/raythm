#include "title/online_download_internal.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <future>
#include <unordered_map>
#include <utility>
#include <vector>

#include "app_paths.h"
#include "chart_difficulty.h"
#include "path_utils.h"
#include "song_loader.h"

namespace title_online_view {
namespace {

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

song_select::chart_option make_chart_option(const std::string& chart_path, const chart_data& chart) {
    const auto [min_bpm, max_bpm] = collect_bpm_range(chart);
    chart_meta meta = chart.meta;
    meta.level = chart_difficulty::calculate_level(chart);
    return {
        chart_path,
        meta,
        0,
        std::nullopt,
        static_cast<int>(chart.notes.size()),
        min_bpm,
        max_bpm,
    };
}

std::vector<song_select::song_entry> load_song_entries_from_directory(const std::filesystem::path& songs_root,
                                                                      bool attach_external_charts) {
    std::vector<song_select::song_entry> songs;
    if (!std::filesystem::exists(songs_root) || !std::filesystem::is_directory(songs_root)) {
        return songs;
    }

    const song_load_result load_result = song_loader::load_all(path_utils::to_utf8(songs_root));
    std::vector<song_data> all_songs = load_result.songs;
    if (attach_external_charts) {
        song_loader::attach_external_charts(path_utils::to_utf8(app_paths::charts_root()), all_songs);
    }

    std::sort(all_songs.begin(), all_songs.end(), [](const song_data& left, const song_data& right) {
        return left.meta.title < right.meta.title;
    });

    songs.reserve(all_songs.size());
    for (const song_data& song : all_songs) {
        song_select::song_entry entry;
        entry.song = song;

        for (const std::string& chart_path : song.chart_paths) {
            const chart_parse_result parse_result = song_loader::load_chart(chart_path);
            if (!parse_result.success || !parse_result.data.has_value()) {
                continue;
            }
            entry.charts.push_back(make_chart_option(chart_path, *parse_result.data));
        }

        std::sort(entry.charts.begin(), entry.charts.end(), [](const song_select::chart_option& left,
                                                               const song_select::chart_option& right) {
            if (left.meta.key_count != right.meta.key_count) {
                return left.meta.key_count < right.meta.key_count;
            }
            if (left.meta.level != right.meta.level) {
                return left.meta.level < right.meta.level;
            }
            return left.meta.difficulty < right.meta.difficulty;
        });

        songs.push_back(std::move(entry));
    }

    return songs;
}

using local_song_lookup = std::unordered_map<std::string, const song_select::song_entry*>;

local_song_lookup build_local_lookup(const std::vector<song_select::song_entry>& local_songs) {
    local_song_lookup lookup;
    for (const song_select::song_entry& song : local_songs) {
        lookup[song.song.meta.song_id] = &song;
    }
    return lookup;
}

bool local_chart_exists(const song_select::song_entry* local_song, const std::string& chart_id) {
    if (local_song == nullptr || chart_id.empty()) {
        return false;
    }
    return std::any_of(local_song->charts.begin(), local_song->charts.end(),
                       [&](const song_select::chart_option& chart) {
                           return chart.meta.chart_id == chart_id;
                       });
}

song_entry_state build_song_state(const song_select::song_entry& remote_song,
                                  const local_song_lookup& local_lookup) {
    song_entry_state state_entry;
    state_entry.song = remote_song;

    const song_select::song_entry* local_song = nullptr;
    if (const auto it = local_lookup.find(remote_song.song.meta.song_id); it != local_lookup.end()) {
        local_song = it->second;
    }

    state_entry.installed = local_song != nullptr;
    state_entry.update_available = local_song != nullptr &&
                                   local_song->song.meta.song_version < remote_song.song.meta.song_version;

    state_entry.charts.reserve(remote_song.charts.size());
    for (const song_select::chart_option& chart : remote_song.charts) {
        state_entry.charts.push_back({
            chart,
            local_chart_exists(local_song, chart.meta.chart_id),
            state_entry.update_available,
        });
    }

    return state_entry;
}

song_entry_state build_owned_song_state(const song_select::song_entry& local_song) {
    song_entry_state state_entry;
    state_entry.song = local_song;
    state_entry.installed = true;
    state_entry.update_available = false;
    state_entry.charts.reserve(local_song.charts.size());
    for (const song_select::chart_option& chart : local_song.charts) {
        state_entry.charts.push_back({
            chart,
            true,
            false,
        });
    }
    return state_entry;
}

std::vector<song_select::song_entry> load_remote_song_entries(catalog_mode mode) {
    (void)mode;
    return {};
}

catalog_load_result load_catalog_result() {
    const std::vector<song_select::song_entry> local_songs =
        load_song_entries_from_directory(app_paths::songs_root(), true);
    const local_song_lookup local_lookup = build_local_lookup(local_songs);

    const std::vector<song_select::song_entry> official_remote_songs =
        load_remote_song_entries(catalog_mode::official);
    const std::vector<song_select::song_entry> community_remote_songs =
        load_remote_song_entries(catalog_mode::community);

    catalog_load_result result;
    result.official_songs.reserve(official_remote_songs.size());
    for (const song_select::song_entry& song : official_remote_songs) {
        result.official_songs.push_back(build_song_state(song, local_lookup));
    }

    result.community_songs.reserve(community_remote_songs.size());
    for (const song_select::song_entry& song : community_remote_songs) {
        result.community_songs.push_back(build_song_state(song, local_lookup));
    }

    result.owned_songs.reserve(local_songs.size());
    for (const song_select::song_entry& song : local_songs) {
        result.owned_songs.push_back(build_owned_song_state(song));
    }

    return result;
}

}  // namespace

jacket_cache::~jacket_cache() {
    clear();
}

const Texture2D* jacket_cache::get(const song_data& song) {
    const std::string key = song.meta.song_id;
    auto& entry = entries_[key];
    if (entry.loaded) {
        return &entry.texture;
    }
    if (entry.missing || song.meta.jacket_file.empty()) {
        return nullptr;
    }

    const std::filesystem::path jacket_path = path_utils::join_utf8(song.directory, song.meta.jacket_file);
    if (!std::filesystem::exists(jacket_path) || !std::filesystem::is_regular_file(jacket_path)) {
        entry.missing = true;
        return nullptr;
    }

    const std::string jacket_path_utf8 = path_utils::to_utf8(jacket_path);
    entry.texture = LoadTexture(jacket_path_utf8.c_str());
    entry.loaded = entry.texture.id != 0;
    entry.missing = !entry.loaded;
    if (entry.loaded) {
        SetTextureFilter(entry.texture, TEXTURE_FILTER_BILINEAR);
        return &entry.texture;
    }
    return nullptr;
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
    state.catalog_future = std::async(std::launch::async, []() {
        return load_catalog_result();
    });
}

bool poll_catalog(state& state) {
    if (!state.catalog_loading) {
        return false;
    }
    if (state.catalog_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    catalog_load_result result = state.catalog_future.get();
    state.catalog_loading = false;
    state.catalog_loaded_once = true;
    state.jackets.clear();
    state.official_songs = std::move(result.official_songs);
    state.community_songs = std::move(result.community_songs);
    state.owned_songs = std::move(result.owned_songs);

    if (state.official_songs.empty() && state.community_songs.empty() && !state.owned_songs.empty()) {
        state.mode = catalog_mode::owned;
    } else if (state.official_songs.empty() && !state.community_songs.empty()) {
        state.mode = catalog_mode::community;
    } else if (state.community_songs.empty() && !state.official_songs.empty()) {
        state.mode = catalog_mode::official;
    }

    detail::ensure_selection_valid(state);
    return true;
}

void on_enter(state& state, song_select::preview_controller& preview_controller) {
    if (!state.catalog_loaded_once && !state.catalog_loading) {
        reload_catalog(state);
    }
    preview_controller.select_song(preview_song(state));
}

void on_exit(state& state) {
    state.jackets.clear();
}

}  // namespace title_online_view
