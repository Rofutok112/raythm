#include "song_select/song_select_state.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <utility>

#include "managed_content_storage.h"
#include "path_utils.h"
#include "song_select/song_select_layout.h"
#include "tween.h"
#include "ui_notice.h"
#include "ui_scroll.h"

namespace {

std::string ascii_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool contains_case_insensitive(const std::string& value, const std::string& query) {
    if (query.empty()) {
        return true;
    }
    return ascii_lower_copy(value).find(ascii_lower_copy(query)) != std::string::npos;
}

bool contains_any_case_insensitive(const std::vector<std::string>& values, const std::string& query) {
    return std::any_of(values.begin(), values.end(), [&](const std::string& value) {
        return contains_case_insensitive(value, query);
    });
}

std::string normalize_server_url(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

song_select::jacket_cache::pending_texture load_local_jacket_bytes(const std::filesystem::path& path) {
    song_select::jacket_cache::pending_texture result;
    const managed_content_storage::managed_file_read_result managed =
        managed_content_storage::read_managed_file(path);
    if (managed.managed) {
        if (managed.success) {
            result.bytes = managed.bytes;
            result.file_type = path.extension().string();
        }
        return result;
    }

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

Texture2D load_jacket_texture_from_pending(const song_select::jacket_cache::pending_texture& pending) {
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

void ensure_jacket_cache(song_select::state& state) {
    if (!state.jackets) {
        state.jackets = std::make_shared<song_select::jacket_cache>();
    }
}

struct jacket_identity {
    std::string directory;
    std::string file;
    std::string managed_package_id;
    std::string managed_jacket_hash;
    std::string managed_remote_jacket_hash;
};

jacket_identity identity_for_jacket(const song_select::song_entry& song) {
    return {
        song.song.directory,
        song.song.meta.jacket_file,
        song.managed_manifest.has_value() ? song.managed_manifest->package_id : "",
        song.managed_manifest.has_value() ? song.managed_manifest->jacket_hash : "",
        song.managed_manifest.has_value() ? song.managed_manifest->remote_jacket_hash : "",
    };
}

bool same_jacket_identity(const jacket_identity& lhs, const jacket_identity& rhs) {
    return lhs.directory == rhs.directory &&
           lhs.file == rhs.file &&
           lhs.managed_package_id == rhs.managed_package_id &&
           lhs.managed_jacket_hash == rhs.managed_jacket_hash &&
           lhs.managed_remote_jacket_hash == rhs.managed_remote_jacket_hash;
}

std::unordered_map<std::string, jacket_identity> jacket_identity_map(
    const std::vector<song_select::song_entry>& songs) {
    std::unordered_map<std::string, jacket_identity> result;
    result.reserve(songs.size());
    for (const song_select::song_entry& song : songs) {
        if (!song.song.meta.song_id.empty()) {
            result[song.song.meta.song_id] = identity_for_jacket(song);
        }
    }
    return result;
}

}  // namespace

namespace song_select {

state::state()
    : songs(catalog.songs),
      load_errors(catalog.load_errors),
      jackets(catalog.jackets),
      catalog_loading(catalog.catalog_loading),
      catalog_loaded_once(catalog.catalog_loaded_once),
      selected_song_index(selection.selected_song_index),
      difficulty_index(selection.difficulty_index),
      scroll_y(scroll.scroll_y),
      scroll_y_target(scroll.scroll_y_target),
      chart_scroll_y(scroll.chart_scroll_y),
      chart_scroll_y_target(scroll.chart_scroll_y_target),
      embedded_chart_scroll_y(scroll.embedded_chart_scroll_y),
      embedded_chart_scroll_y_target(scroll.embedded_chart_scroll_y_target),
      selected_song_expanded(selection.selected_song_expanded),
      selected_song_expand_t(selection.selected_song_expand_t),
      play_search_input(filter.play_search_input),
      chart_source(filter.chart_source),
      play_filter_modal_open(filter.play_filter_modal_open),
      play_mod_modal_open(filter.play_mod_modal_open),
      mods(filter.mods),
      chart_key_filter(filter.chart_key_filter),
      chart_min_level(filter.chart_min_level),
      chart_max_level(filter.chart_max_level),
      chart_level_filter_dragging(filter.chart_level_filter_dragging),
      chart_level_filter_dragging_min(filter.chart_level_filter_dragging_min),
      preview_bar_dragging(preview.preview_bar_dragging),
      preview_bar_resume_after_drag(preview.preview_bar_resume_after_drag),
      preview_bar_drag_position_seconds(preview.preview_bar_drag_position_seconds),
      song_change_anim_t(preview.song_change_anim_t),
      chart_change_anim_t(preview.chart_change_anim_t),
      scene_fade_in(preview.scene_fade_in),
      scrollbar_dragging(scroll.scrollbar_dragging),
      scrollbar_drag_offset(scroll.scrollbar_drag_offset),
      context_menu(dialog.context_menu),
      confirmation_dialog(dialog.confirmation_dialog),
      recent_result_offset(preview.recent_result_offset),
      auth(auth_ui.auth),
      login_dialog(auth_ui.login_dialog) {
}

state::state(const state& other)
    : state() {
    *this = other;
}

state& state::operator=(const state& other) {
    if (this == &other) {
        return *this;
    }

    catalog = other.catalog;
    selection = other.selection;
    filter = other.filter;
    scroll = other.scroll;
    preview = other.preview;
    dialog = other.dialog;
    ranking_panel = other.ranking_panel;
    auth_ui = other.auth_ui;
    return *this;
}

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
            if (song_copy.meta.jacket_file.empty()) {
                promise.set_value({});
                return;
            }

            const std::filesystem::path jacket_path =
                path_utils::join_utf8(song_copy.directory, song_copy.meta.jacket_file);
            std::error_code ec;
            const bool regular_file =
                std::filesystem::exists(jacket_path, ec) && std::filesystem::is_regular_file(jacket_path, ec);
            if (!regular_file &&
                !managed_content_storage::read_managed_file(jacket_path).managed) {
                promise.set_value({});
                return;
            }

            promise.set_value(load_local_jacket_bytes(jacket_path));
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
            entry.texture = load_jacket_texture_from_pending(pending);
            entry.loaded = entry.texture.id != 0;
            entry.missing = !entry.loaded;
        } catch (...) {
            entry.missing = true;
        }
    }
}

void jacket_cache::reconcile_catalog(const std::vector<song_entry>& previous_songs,
                                     const std::vector<song_entry>& next_songs) {
    const std::unordered_map<std::string, jacket_identity> previous = jacket_identity_map(previous_songs);
    const std::unordered_map<std::string, jacket_identity> next = jacket_identity_map(next_songs);

    for (auto it = entries_.begin(); it != entries_.end();) {
        const auto previous_it = previous.find(it->first);
        const auto next_it = next.find(it->first);
        const bool keep = previous_it != previous.end() &&
                          next_it != next.end() &&
                          same_jacket_identity(previous_it->second, next_it->second);
        if (keep) {
            ++it;
        } else {
            it = erase_entry(it);
        }
    }
}

void jacket_cache::clear() {
    for (auto it = entries_.begin(); it != entries_.end();) {
        it = erase_entry(it);
    }
}

std::unordered_map<std::string, jacket_cache::cache_entry>::iterator jacket_cache::erase_entry(
    std::unordered_map<std::string, cache_entry>::iterator it) {
    if (it->second.loaded) {
        UnloadTexture(it->second.texture);
    }
    return entries_.erase(it);
}

float scroll_offset_for_selected_song(const state& state);

const song_entry* selected_song(const state& state) {
    if (state.songs.empty() || state.selected_song_index < 0 ||
        state.selected_song_index >= static_cast<int>(state.songs.size())) {
        return nullptr;
    }

    return &state.songs[static_cast<size_t>(state.selected_song_index)];
}

bool can_match_online_song(const song_entry& song) {
    return song.storage == storage_policy::managed_package;
}

bool can_match_online_chart(const chart_option& chart) {
    return chart.storage == storage_policy::managed_package;
}

bool can_use_online_chart_routes(const chart_option& chart) {
    if (!can_match_online_chart(chart)) {
        return false;
    }
    if (online_content::is_queueable(chart.online_identity)) {
        return true;
    }
    return std::any_of(chart.remote_links.begin(), chart.remote_links.end(),
                       [](const online_content::chart_identity& link) {
                           return online_content::is_queueable(link);
                       });
}

bool has_queueable_remote_link(const chart_option& chart, const std::string& server_url) {
    if (!can_use_online_chart_routes(chart)) {
        return false;
    }
    const std::string normalized_server_url = normalize_server_url(server_url);
    if (online_content::is_queueable(chart.online_identity)) {
        if (normalized_server_url.empty() ||
            normalize_server_url(chart.online_identity->server_url) == normalized_server_url) {
            return true;
        }
    }
    for (const online_content::chart_identity& link : chart.remote_links) {
        if (online_content::is_queueable(link) &&
            (normalized_server_url.empty() ||
             normalize_server_url(link.server_url) == normalized_server_url)) {
            return true;
        }
    }
    return false;
}

bool chart_matches_filters(const state& state, const chart_option& chart) {
    if (state.filter.multiplayer_queueable_only) {
        if (!has_queueable_remote_link(chart, state.filter.multiplayer_queue_server_url)) {
            return false;
        }
    }

    switch (state.chart_source) {
    case chart_source_filter::official:
        if (chart.source_status != content_status::official) {
            return false;
        }
        break;
    case chart_source_filter::community:
        if (chart.source_status != content_status::community) {
            return false;
        }
        break;
    case chart_source_filter::all:
        break;
    }

    if (state.chart_key_filter != 0) {
        const int key_count = chart.meta.key_count > 0 ? chart.meta.key_count : 4;
        if (key_count != state.chart_key_filter) {
            return false;
        }
    }
    if (chart.meta.level < state.chart_min_level || chart.meta.level > state.chart_max_level) {
        return false;
    }

    return true;
}

bool song_matches_search(const song_entry& song, const std::string& query) {
    if (query.empty()) {
        return true;
    }
    if (contains_case_insensitive(song.song.meta.title, query) ||
        contains_case_insensitive(song.song.meta.artist, query) ||
        contains_case_insensitive(song.song.meta.genre, query) ||
        contains_any_case_insensitive(song.song.meta.genres, query) ||
        contains_any_case_insensitive(song.song.meta.keywords, query)) {
        return true;
    }
    return std::any_of(song.charts.begin(), song.charts.end(), [&](const chart_option& chart) {
        return contains_case_insensitive(chart.meta.difficulty, query) ||
               contains_case_insensitive(chart.meta.chart_author, query);
    });
}

bool song_metadata_matches_search(const song_entry& song, const std::string& query) {
    return query.empty() ||
           contains_case_insensitive(song.song.meta.title, query) ||
           contains_case_insensitive(song.song.meta.artist, query) ||
           contains_case_insensitive(song.song.meta.genre, query) ||
           contains_any_case_insensitive(song.song.meta.genres, query) ||
           contains_any_case_insensitive(song.song.meta.keywords, query);
}

bool chart_matches_search(const chart_option& chart, const std::string& query) {
    return query.empty() ||
           contains_case_insensitive(chart.meta.difficulty, query) ||
           contains_case_insensitive(chart.meta.chart_author, query);
}

bool song_has_visible_chart(const state& state, const song_entry& song) {
    return std::any_of(song.charts.begin(), song.charts.end(), [&](const chart_option& chart) {
        return chart_matches_filters(state, chart);
    });
}

bool chartless_song_matches_filters(const state& state, const song_entry& song) {
    if (!state.filter.include_chartless_songs ||
        !song.charts.empty() ||
        state.filter.multiplayer_queueable_only) {
        return false;
    }
    return true;
}

std::vector<int> filtered_song_indices(const state& state) {
    std::vector<int> indices;
    indices.reserve(state.songs.size());
    for (int song_index = 0; song_index < static_cast<int>(state.songs.size()); ++song_index) {
        const song_entry& song = state.songs[static_cast<size_t>(song_index)];
        if (!song_matches_search(song, state.play_search_input.value)) {
            continue;
        }
        if (song_has_visible_chart(state, song) ||
            chartless_song_matches_filters(state, song)) {
            indices.push_back(song_index);
        }
    }
    return indices;
}

std::vector<const chart_option*> filtered_charts_for_song(const state& state, const song_entry* song) {
    std::vector<const chart_option*> filtered;
    if (song == nullptr) {
        return filtered;
    }

    filtered.reserve(song->charts.size());
    const bool song_match = song_metadata_matches_search(*song, state.play_search_input.value);
    for (const chart_option& chart : song->charts) {
        if (chart_matches_filters(state, chart) &&
            (song_match || chart_matches_search(chart, state.play_search_input.value))) {
            filtered.push_back(&chart);
        }
    }
    return filtered;
}

std::vector<const chart_option*> filtered_charts_for_selected_song(const state& state) {
    return filtered_charts_for_song(state, selected_song(state));
}

const chart_option* selected_chart_for(const state& state, const std::vector<const chart_option*>& filtered) {
    if (filtered.empty()) {
        return nullptr;
    }

    const int index = std::min<int>(state.difficulty_index, static_cast<int>(filtered.size()) - 1);
    return filtered[static_cast<size_t>(index)];
}

void reset_for_enter(state& state) {
    ensure_jacket_cache(state);
    state.jackets->clear();
    state.catalog_loading = false;
    state.catalog_loaded_once = false;
    state.selected_song_index = 0;
    state.difficulty_index = 0;
    state.scroll_y = 0.0f;
    state.scroll_y_target = 0.0f;
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
    state.embedded_chart_scroll_y = 0.0f;
    state.embedded_chart_scroll_y_target = 0.0f;
    state.selected_song_expanded = true;
    state.selected_song_expand_t = 1.0f;
    state.play_search_input = {};
    state.chart_source = chart_source_filter::all;
    state.filter.include_chartless_songs = false;
    state.play_filter_modal_open = false;
    state.play_mod_modal_open = false;
    state.mods = {};
    state.filter.multiplayer_queueable_only = false;
    state.filter.multiplayer_queue_server_url.clear();
    state.chart_key_filter = 0;
    state.chart_min_level = 0.0f;
    state.chart_max_level = 99.0f;
    state.chart_level_filter_dragging = false;
    state.chart_level_filter_dragging_min = false;
    state.preview_bar_dragging = false;
    state.preview_bar_resume_after_drag = false;
    state.preview_bar_drag_position_seconds = 0.0;
    state.song_change_anim_t = 1.0f;
    state.chart_change_anim_t = 1.0f;
    state.scene_fade_in.restart(scene_fade::direction::in, 0.3f, 0.65f);
    state.scrollbar_dragging = false;
    state.scrollbar_drag_offset = 0.0f;
    state.context_menu = {};
    state.confirmation_dialog = {};
    state.recent_result_offset.reset();
    state.ranking_panel = {};
    state.login_dialog = {};
}

void tick_animations(state& state, float dt) {
    state.song_change_anim_t = tween::retreat(state.song_change_anim_t, dt, 4.0f);
    state.chart_change_anim_t = tween::retreat(state.chart_change_anim_t, dt, 5.0f);
    if (state.selected_song_expanded) {
        state.selected_song_expand_t = tween::advance(state.selected_song_expand_t, dt, 7.5f);
    } else {
        state.selected_song_expand_t = tween::retreat(state.selected_song_expand_t, dt, 7.5f);
    }
    state.ranking_panel.reveal_anim += dt;
    if (state.login_dialog.open) {
        state.login_dialog.open_anim = tween::advance(state.login_dialog.open_anim, dt, 8.0f);
    } else {
        state.login_dialog.open_anim = 0.0f;
    }
    state.scene_fade_in.update(dt);
}

void apply_catalog(state& state, catalog_data catalog,
                   const std::string& preferred_song_id,
                   const std::string& preferred_chart_id) {
    ensure_jacket_cache(state);
    const song_entry* previous_song = selected_song(state);
    const std::string previous_song_id = previous_song != nullptr ? previous_song->song.meta.song_id : "";
    std::string previous_chart_id;
    const auto previous_filtered = filtered_charts_for_selected_song(state);
    if (const chart_option* previous_chart = selected_chart_for(state, previous_filtered)) {
        previous_chart_id = previous_chart->meta.chart_id;
    }

    state.jackets->reconcile_catalog(state.songs, catalog.songs);
    state.songs = std::move(catalog.songs);
    state.load_errors = std::move(catalog.load_errors);
    state.catalog_loading = false;
    state.catalog_loaded_once = true;
    state.selected_song_index = 0;
    state.difficulty_index = 0;
    state.scroll_y = 0.0f;
    state.scroll_y_target = 0.0f;
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
    state.embedded_chart_scroll_y = 0.0f;
    state.embedded_chart_scroll_y_target = 0.0f;
    state.selected_song_expanded = true;
    state.selected_song_expand_t = 1.0f;
    state.play_filter_modal_open = false;
    state.play_mod_modal_open = false;
    state.chart_level_filter_dragging = false;
    state.chart_level_filter_dragging_min = false;
    state.preview_bar_dragging = false;
    state.preview_bar_resume_after_drag = false;
    state.preview_bar_drag_position_seconds = 0.0;
    state.scrollbar_dragging = false;
    state.scrollbar_drag_offset = 0.0f;
    state.context_menu = {};
    state.confirmation_dialog = {};

    if (!preferred_song_id.empty()) {
        for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
            if (state.songs[static_cast<size_t>(i)].song.meta.song_id == preferred_song_id) {
                state.selected_song_index = i;
                break;
            }
        }
    }

    if (!state.songs.empty() && !preferred_chart_id.empty()) {
        const auto& charts = state.songs[static_cast<size_t>(state.selected_song_index)].charts;
        for (int i = 0; i < static_cast<int>(charts.size()); ++i) {
            if (charts[static_cast<size_t>(i)].meta.chart_id == preferred_chart_id) {
                state.difficulty_index = i;
                break;
            }
        }
    }

    const std::vector<int> visible_song_indices = filtered_song_indices(state);
    if (!visible_song_indices.empty() &&
        std::find(visible_song_indices.begin(), visible_song_indices.end(), state.selected_song_index) ==
            visible_song_indices.end()) {
        state.selected_song_index = visible_song_indices.front();
        state.difficulty_index = 0;
    }
    const auto filtered_charts = filtered_charts_for_selected_song(state);
    if (filtered_charts.empty()) {
        state.difficulty_index = 0;
    } else {
        state.difficulty_index = std::clamp(state.difficulty_index, 0, static_cast<int>(filtered_charts.size()) - 1);
    }

    if (!state.songs.empty()) {
        const song_entry* next_song = selected_song(state);
        const std::string next_song_id = next_song != nullptr ? next_song->song.meta.song_id : "";
        std::string next_chart_id;
        const auto next_filtered = filtered_charts_for_selected_song(state);
        if (const chart_option* next_chart = selected_chart_for(state, next_filtered)) {
            next_chart_id = next_chart->meta.chart_id;
        }
        const bool song_changed = previous_song_id != next_song_id;
        const bool chart_changed = previous_chart_id != next_chart_id;
        if (song_changed) {
            state.song_change_anim_t = 1.0f;
        }
        if (song_changed || chart_changed) {
            state.chart_change_anim_t = 1.0f;
        }
        const float restored_scroll = scroll_offset_for_selected_song(state);
        state.scroll_y = restored_scroll;
        state.scroll_y_target = restored_scroll;
    }
}

bool apply_catalog_chart_level_update(state& state, const catalog_data& catalog) {
    struct level_entry {
        std::string path;
        float level = 0.0f;
    };

    std::unordered_map<std::string, level_entry> levels_by_chart_id;
    for (const song_entry& song : catalog.songs) {
        for (const chart_option& chart : song.charts) {
            if (!chart.meta.chart_id.empty() && chart.meta.level > 0.0f) {
                levels_by_chart_id[chart.meta.chart_id] = {
                    chart.path,
                    chart.meta.level,
                };
            }
        }
    }

    bool matched_any_chart = false;
    for (song_entry& song : state.songs) {
        for (chart_option& chart : song.charts) {
            const auto level_it = levels_by_chart_id.find(chart.meta.chart_id);
            if (level_it == levels_by_chart_id.end() ||
                level_it->second.path != chart.path) {
                continue;
            }
            matched_any_chart = true;
            chart.meta.level = level_it->second.level;
        }
    }
    return matched_any_chart;
}

namespace {

void select_after_catalog_item_removed(state& state,
                                       const std::string& preferred_song_id,
                                       const std::string& preferred_chart_id) {
    if (state.jackets) {
        state.jackets->clear();
    }
    state.context_menu = {};
    state.confirmation_dialog = {};
    state.preview_bar_dragging = false;
    state.preview_bar_resume_after_drag = false;
    state.preview_bar_drag_position_seconds = 0.0;
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
    state.embedded_chart_scroll_y = 0.0f;
    state.embedded_chart_scroll_y_target = 0.0f;

    if (state.songs.empty()) {
        state.selected_song_index = 0;
        state.difficulty_index = 0;
        state.scroll_y = 0.0f;
        state.scroll_y_target = 0.0f;
        state.song_change_anim_t = 1.0f;
        state.chart_change_anim_t = 1.0f;
        return;
    }

    state.selected_song_index = 0;
    if (!preferred_song_id.empty()) {
        for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
            if (state.songs[static_cast<size_t>(i)].song.meta.song_id == preferred_song_id) {
                state.selected_song_index = i;
                break;
            }
        }
    }

    const std::vector<int> visible_song_indices = filtered_song_indices(state);
    if (visible_song_indices.empty()) {
        state.selected_song_index = 0;
        state.difficulty_index = 0;
    } else if (std::find(visible_song_indices.begin(), visible_song_indices.end(), state.selected_song_index) ==
               visible_song_indices.end()) {
        state.selected_song_index = visible_song_indices.front();
        state.difficulty_index = 0;
    }

    const auto filtered_charts = filtered_charts_for_selected_song(state);
    state.difficulty_index = 0;
    if (!preferred_chart_id.empty()) {
        for (int i = 0; i < static_cast<int>(filtered_charts.size()); ++i) {
            if (filtered_charts[static_cast<size_t>(i)]->meta.chart_id == preferred_chart_id) {
                state.difficulty_index = i;
                break;
            }
        }
    }
    if (!filtered_charts.empty()) {
        state.difficulty_index = std::clamp(state.difficulty_index, 0, static_cast<int>(filtered_charts.size()) - 1);
    }

    state.song_change_anim_t = 1.0f;
    state.chart_change_anim_t = 1.0f;
    const float restored_scroll = scroll_offset_for_selected_song(state);
    state.scroll_y = restored_scroll;
    state.scroll_y_target = restored_scroll;
}

}  // namespace

bool remove_deleted_song_from_catalog(state& state,
                                      const std::string& deleted_song_id,
                                      const std::string& preferred_song_id,
                                      const std::string& preferred_chart_id) {
    const auto song_it = std::find_if(state.songs.begin(), state.songs.end(),
                                      [&](const song_entry& song) {
                                          return song.song.meta.song_id == deleted_song_id;
                                      });
    if (song_it == state.songs.end()) {
        return false;
    }

    state.songs.erase(song_it);
    select_after_catalog_item_removed(state, preferred_song_id, preferred_chart_id);
    return true;
}

bool remove_deleted_chart_from_catalog(state& state,
                                       const std::string& deleted_song_id,
                                       const std::string& deleted_chart_id,
                                       const std::string& preferred_song_id,
                                       const std::string& preferred_chart_id) {
    const auto song_it = std::find_if(state.songs.begin(), state.songs.end(),
                                      [&](const song_entry& song) {
                                          return song.song.meta.song_id == deleted_song_id;
                                      });
    if (song_it == state.songs.end()) {
        return false;
    }

    auto& charts = song_it->charts;
    const auto chart_it = std::find_if(charts.begin(), charts.end(),
                                       [&](const chart_option& chart) {
                                           return chart.meta.chart_id == deleted_chart_id;
                                       });
    if (chart_it == charts.end()) {
        return false;
    }

    charts.erase(chart_it);
    select_after_catalog_item_removed(state, preferred_song_id, preferred_chart_id);
    return true;
}

bool apply_song_selection(state& state, int song_index, int chart_index) {
    if (state.songs.empty()) {
        return false;
    }

    const int clamped_song_index = std::clamp(song_index, 0, static_cast<int>(state.songs.size()) - 1);
    const bool song_changed = clamped_song_index != state.selected_song_index;
    const int previous_chart_index = state.difficulty_index;
    state.selected_song_index = clamped_song_index;

    const auto filtered = filtered_charts_for_selected_song(state);
    if (filtered.empty()) {
        state.difficulty_index = 0;
    } else {
        state.difficulty_index = std::clamp(chart_index, 0, static_cast<int>(filtered.size()) - 1);
    }

    if (song_changed) {
        state.song_change_anim_t = 1.0f;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        state.embedded_chart_scroll_y = 0.0f;
        state.embedded_chart_scroll_y_target = 0.0f;
        state.selected_song_expanded = true;
        state.selected_song_expand_t = 0.0f;
    }
    if (song_changed || previous_chart_index != state.difficulty_index) {
        state.chart_change_anim_t = 1.0f;
    }
    return song_changed;
}

bool apply_chart_filters(state& state,
                         chart_source_filter source,
                         int key_filter,
                         float min_level,
                         float max_level) {
    min_level = std::clamp(min_level, 0.0f, 15.0f);
    max_level = std::clamp(max_level, 0.0f, 99.0f);
    if (min_level > max_level) {
        std::swap(min_level, max_level);
    }
    const bool changed =
        state.chart_source != source ||
        state.chart_key_filter != key_filter ||
        std::fabs(state.chart_min_level - min_level) > 0.001f ||
        std::fabs(state.chart_max_level - max_level) > 0.001f;
    if (!changed) {
        return false;
    }

    state.chart_source = source;
    state.chart_key_filter = key_filter;
    state.chart_min_level = min_level;
    state.chart_max_level = max_level;
    state.embedded_chart_scroll_y = 0.0f;
    state.embedded_chart_scroll_y_target = 0.0f;

    const auto indices = filtered_song_indices(state);
    if (indices.empty()) {
        state.difficulty_index = 0;
        return true;
    }
    if (std::find(indices.begin(), indices.end(), state.selected_song_index) == indices.end()) {
        apply_song_selection(state, indices.front(), 0);
        return true;
    }

    const auto filtered = filtered_charts_for_selected_song(state);
    if (filtered.empty()) {
        state.difficulty_index = 0;
    } else {
        state.difficulty_index = std::clamp(state.difficulty_index, 0, static_cast<int>(filtered.size()) - 1);
    }
    state.chart_change_anim_t = 1.0f;
    return true;
}

bool clear_chart_filters(state& state) {
    return apply_chart_filters(state, chart_source_filter::all, 0, 0.0f, 99.0f);
}

void open_song_context_menu(state& state, int song_index, Rectangle rect) {
    state.context_menu.open = true;
    state.context_menu.target = context_menu_target::song;
    state.context_menu.section = context_menu_section::root;
    state.context_menu.song_index = song_index;
    state.context_menu.chart_index = -1;
    state.context_menu.rect = rect;
}

void open_chart_context_menu(state& state, int song_index, int chart_index, Rectangle rect) {
    state.context_menu.open = true;
    state.context_menu.target = context_menu_target::chart;
    state.context_menu.section = context_menu_section::root;
    state.context_menu.song_index = song_index;
    state.context_menu.chart_index = chart_index;
    state.context_menu.rect = rect;
}

void open_list_background_context_menu(state& state, Rectangle rect) {
    state.context_menu.open = true;
    state.context_menu.target = context_menu_target::list_background;
    state.context_menu.section = context_menu_section::root;
    state.context_menu.song_index = state.selected_song_index;
    state.context_menu.chart_index = state.difficulty_index;
    state.context_menu.rect = rect;
}

void close_context_menu(state& state) {
    state.context_menu = {};
}

void queue_status_message(state& state, std::string message, bool is_error) {
    (void)state;
    ui::notify(std::move(message), is_error ? ui::notice_tone::error : ui::notice_tone::success);
}

float expanded_row_height(const state& state, int song_index) {
    if (song_index == state.selected_song_index) {
        return layout::kRowHeight + 14.0f +
               static_cast<float>(filtered_charts_for_selected_song(state).size()) * 30.0f;
    }
    return layout::kRowHeight;
}

float song_list_content_top() {
    return layout::kSongListTopPadding;
}

float song_list_first_item_y(const state& state) {
    return layout::kSongListViewRect.y + song_list_content_top() - state.scroll_y;
}

float content_height(const state& state) {
    float total = song_list_content_top();
    for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
        total += expanded_row_height(state, i);
    }
    return total;
}

float scroll_offset_for_selected_song(const state& state) {
    if (state.songs.empty() || state.selected_song_index < 0 ||
        state.selected_song_index >= static_cast<int>(state.songs.size())) {
        return 0.0f;
    }

    float row_top = song_list_content_top();
    for (int i = 0; i < state.selected_song_index; ++i) {
        row_top += expanded_row_height(state, i);
    }

    const float row_bottom = row_top + expanded_row_height(state, state.selected_song_index);
    const float view_height = layout::kSongListViewRect.height;
    const float max_scroll = ui::max_scroll_offset(content_height(state), view_height);

    return ui::scroll_offset_with_item_visible(
        row_top - 12.0f,
        row_top,
        row_bottom,
        view_height,
        max_scroll,
        12.0f);
}

std::string fallback_song_id_after_song_delete(const state& state, int song_index) {
    if (state.songs.size() <= 1) {
        return "";
    }
    if (song_index + 1 < static_cast<int>(state.songs.size())) {
        return state.songs[static_cast<size_t>(song_index + 1)].song.meta.song_id;
    }
    if (song_index > 0) {
        return state.songs[static_cast<size_t>(song_index - 1)].song.meta.song_id;
    }
    return "";
}

std::string fallback_chart_id_after_chart_delete(const state& state, int song_index, int chart_index) {
    if (song_index < 0 || song_index >= static_cast<int>(state.songs.size())) {
        return "";
    }

    const auto& charts = state.songs[static_cast<size_t>(song_index)].charts;
    if (charts.size() <= 1) {
        return "";
    }
    if (chart_index + 1 < static_cast<int>(charts.size())) {
        return charts[static_cast<size_t>(chart_index + 1)].meta.chart_id;
    }
    if (chart_index > 0) {
        return charts[static_cast<size_t>(chart_index - 1)].meta.chart_id;
    }
    return "";
}

}  // namespace song_select
