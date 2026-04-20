#include "title/online_download_view.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "app_paths.h"
#include "chart_difficulty.h"
#include "path_utils.h"
#include "scene_common.h"
#include "song_loader.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace title_online_view {
namespace {

constexpr Rectangle kBackRect = {48.0f, 38.0f, 98.0f, 38.0f};
constexpr Rectangle kOfficialTabRect = {186.0f, 40.0f, 128.0f, 38.0f};
constexpr Rectangle kCommunityTabRect = {322.0f, 40.0f, 146.0f, 38.0f};
constexpr Rectangle kSearchRect = {736.0f, 36.0f, 474.0f, 44.0f};
constexpr Rectangle kSongLaneRect = {72.0f, 124.0f, 382.0f, 516.0f};
constexpr Rectangle kDetailRect = {490.0f, 124.0f, 728.0f, 516.0f};
constexpr Rectangle kHeroJacketRect = {520.0f, 178.0f, 196.0f, 196.0f};
constexpr Rectangle kChartListRect = {520.0f, 406.0f, 670.0f, 150.0f};
constexpr Rectangle kPrimaryActionRect = {520.0f, 584.0f, 214.0f, 40.0f};
constexpr Rectangle kOpenLocalRect = {748.0f, 584.0f, 184.0f, 40.0f};

constexpr float kSongRowHeight = 74.0f;
constexpr float kSongRowGap = 10.0f;
constexpr float kChartRowHeight = 42.0f;
constexpr float kChartRowGap = 8.0f;

float ease_out_cubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv * inv;
}

float lerp_value(float from, float to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return from + (to - from) * clamped;
}

Rectangle lerp_rect(Rectangle from, Rectangle to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return {
        lerp_value(from.x, to.x, clamped),
        lerp_value(from.y, to.y, clamped),
        lerp_value(from.width, to.width, clamped),
        lerp_value(from.height, to.height, clamped),
    };
}

Rectangle centered_scaled_rect(Rectangle anchor, Rectangle target, float scale, Vector2 offset = {0.0f, 0.0f}) {
    const Vector2 center = {
        anchor.x + anchor.width * 0.5f + offset.x,
        anchor.y + anchor.height * 0.5f + offset.y,
    };
    const float width = target.width * scale;
    const float height = target.height * scale;
    return {
        center.x - width * 0.5f,
        center.y - height * 0.5f,
        width,
        height,
    };
}

Rectangle fallback_origin_rect() {
    return {
        static_cast<float>(kScreenWidth) * 0.5f + 120.0f,
        376.0f,
        160.0f,
        60.0f,
    };
}

Rectangle resolve_origin_rect(Rectangle origin_rect) {
    return origin_rect.width > 0.0f && origin_rect.height > 0.0f ? origin_rect : fallback_origin_rect();
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool contains_case_insensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    return to_lower_ascii(haystack).find(to_lower_ascii(needle)) != std::string::npos;
}

std::string key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

Color key_mode_color(int key_count) {
    const auto& theme = *g_theme;
    return key_count == 6 ? theme.rank_c : theme.rank_b;
}

std::string format_bpm_range(float min_bpm, float max_bpm) {
    if (min_bpm <= 0.0f && max_bpm <= 0.0f) {
        return "-";
    }
    if (std::fabs(max_bpm - min_bpm) < 0.05f) {
        return TextFormat("%.0f", min_bpm);
    }
    return TextFormat("%.0f-%.0f", min_bpm, max_bpm);
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

std::vector<int> filtered_indices_for(const std::vector<song_entry_state>& songs, const std::string& query) {
    std::vector<int> indices;
    indices.reserve(songs.size());

    for (int index = 0; index < static_cast<int>(songs.size()); ++index) {
        const song_entry_state& song = songs[static_cast<size_t>(index)];
        if (query.empty() ||
            contains_case_insensitive(song.song.song.meta.title, query) ||
            contains_case_insensitive(song.song.song.meta.artist, query) ||
            contains_case_insensitive(song.song.song.meta.song_id, query)) {
            indices.push_back(index);
        }
    }

    return indices;
}

std::vector<int> filtered_indices(const state& state) {
    const auto& songs = state.mode == catalog_mode::official ? state.official_songs : state.community_songs;
    return filtered_indices_for(songs, state.search_input.value);
}

int& selected_song_index_ref(state& state) {
    return state.mode == catalog_mode::official ? state.official_selected_song_index
                                                : state.community_selected_song_index;
}

const int& selected_song_index_ref(const state& state) {
    return state.mode == catalog_mode::official ? state.official_selected_song_index
                                                : state.community_selected_song_index;
}

int& selected_chart_index_ref(state& state) {
    return state.mode == catalog_mode::official ? state.official_selected_chart_index
                                                : state.community_selected_chart_index;
}

const int& selected_chart_index_ref(const state& state) {
    return state.mode == catalog_mode::official ? state.official_selected_chart_index
                                                : state.community_selected_chart_index;
}

const std::vector<song_entry_state>& active_songs(const state& state) {
    return state.mode == catalog_mode::official ? state.official_songs : state.community_songs;
}

void ensure_selection_valid(state& state) {
    const auto indices = filtered_indices(state);
    int& selected_song_index = selected_song_index_ref(state);
    int& selected_chart_index = selected_chart_index_ref(state);

    if (indices.empty()) {
        selected_song_index = 0;
        selected_chart_index = 0;
        state.song_scroll_y = 0.0f;
        state.song_scroll_y_target = 0.0f;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        return;
    }

    if (std::find(indices.begin(), indices.end(), selected_song_index) == indices.end()) {
        selected_song_index = indices.front();
        selected_chart_index = 0;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
    }

    const auto& songs = active_songs(state);
    if (selected_song_index < 0 || selected_song_index >= static_cast<int>(songs.size())) {
        selected_song_index = indices.front();
    }

    const auto& charts = songs[static_cast<size_t>(selected_song_index)].charts;
    if (charts.empty()) {
        selected_chart_index = 0;
    } else {
        selected_chart_index = std::clamp(selected_chart_index, 0, static_cast<int>(charts.size()) - 1);
    }
}

void sync_preview(state& state, song_select::preview_controller& preview_controller) {
    preview_controller.select_song(preview_song(state));
}

float song_list_content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    return static_cast<float>(count) * (kSongRowHeight + kSongRowGap) - kSongRowGap;
}

float max_song_scroll(Rectangle area, int count) {
    return std::max(0.0f, song_list_content_height(count) - area.height + 18.0f);
}

Rectangle song_row_rect(Rectangle area, int display_index, float scroll_y) {
    return {
        area.x,
        area.y + 10.0f + static_cast<float>(display_index) * (kSongRowHeight + kSongRowGap) - scroll_y,
        area.width,
        kSongRowHeight
    };
}

float chart_list_content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    return static_cast<float>(count) * (kChartRowHeight + kChartRowGap) - kChartRowGap;
}

float max_chart_scroll(Rectangle area, int count) {
    return std::max(0.0f, chart_list_content_height(count) - area.height + 6.0f);
}

Rectangle chart_row_rect(Rectangle area, int index, float scroll_y) {
    return {
        area.x,
        area.y + static_cast<float>(index) * (kChartRowHeight + kChartRowGap) - scroll_y,
        area.width,
        kChartRowHeight
    };
}

int hit_test_song_list(const state& state, Rectangle area, Vector2 point) {
    const auto indices = filtered_indices(state);
    if (!CheckCollisionPointRec(point, area)) {
        return -1;
    }

    for (int display_index = 0; display_index < static_cast<int>(indices.size()); ++display_index) {
        if (CheckCollisionPointRec(point, song_row_rect(area, display_index, state.song_scroll_y))) {
            return indices[static_cast<size_t>(display_index)];
        }
    }
    return -1;
}

int hit_test_chart_list(const state& state, Rectangle area, Vector2 point) {
    const song_entry_state* song = selected_song(state);
    if (song == nullptr || !CheckCollisionPointRec(point, area)) {
        return -1;
    }

    for (int index = 0; index < static_cast<int>(song->charts.size()); ++index) {
        if (CheckCollisionPointRec(point, chart_row_rect(area, index, state.chart_scroll_y))) {
            return index;
        }
    }
    return -1;
}

std::string song_status_label(const song_entry_state& song) {
    if (song.update_available) {
        return "UPDATE";
    }
    if (song.installed) {
        return "LOCAL";
    }
    return "REMOTE";
}

Color song_status_color(const song_entry_state& song) {
    const auto& t = *g_theme;
    if (song.update_available) {
        return t.accent;
    }
    if (song.installed) {
        return t.success;
    }
    return t.text_muted;
}

std::string chart_status_label(const chart_entry_state& chart) {
    if (chart.update_available) {
        return "UPDATE";
    }
    if (chart.installed) {
        return "LOCAL";
    }
    return "GET";
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
    state.jackets.clear();
    const std::vector<song_select::song_entry> local_songs =
        load_song_entries_from_directory(app_paths::songs_root(), true);
    const local_song_lookup local_lookup = build_local_lookup(local_songs);

    state.community_songs.clear();
    state.community_songs.reserve(local_songs.size());
    for (const song_select::song_entry& song : local_songs) {
        state.community_songs.push_back(build_song_state(song, local_lookup));
    }

    const std::vector<song_select::song_entry> official_songs =
        load_song_entries_from_directory(app_paths::assets_root() / "songs", false);
    state.official_songs.clear();
    state.official_songs.reserve(official_songs.size());
    for (const song_select::song_entry& song : official_songs) {
        state.official_songs.push_back(build_song_state(song, local_lookup));
    }

    if (state.official_songs.empty() && !state.community_songs.empty()) {
        state.mode = catalog_mode::community;
    } else if (state.community_songs.empty() && !state.official_songs.empty()) {
        state.mode = catalog_mode::official;
    }

    ensure_selection_valid(state);
}

void on_enter(state& state, song_select::preview_controller& preview_controller) {
    reload_catalog(state);
    sync_preview(state, preview_controller);
}

void on_exit(state& state) {
    state.jackets.clear();
}

const song_entry_state* selected_song(const state& state) {
    const auto indices = filtered_indices(state);
    if (indices.empty()) {
        return nullptr;
    }

    const auto& songs = active_songs(state);
    const int selected_song_index = selected_song_index_ref(state);
    if (selected_song_index < 0 || selected_song_index >= static_cast<int>(songs.size())) {
        return nullptr;
    }
    if (std::find(indices.begin(), indices.end(), selected_song_index) == indices.end()) {
        return nullptr;
    }
    return &songs[static_cast<size_t>(selected_song_index)];
}

const chart_entry_state* selected_chart(const state& state) {
    const song_entry_state* song = selected_song(state);
    if (song == nullptr || song->charts.empty()) {
        return nullptr;
    }

    const int selected_chart_index = selected_chart_index_ref(state);
    if (selected_chart_index < 0 || selected_chart_index >= static_cast<int>(song->charts.size())) {
        return nullptr;
    }
    return &song->charts[static_cast<size_t>(selected_chart_index)];
}

const song_select::song_entry* preview_song(const state& state) {
    const song_entry_state* song = selected_song(state);
    return song != nullptr ? &song->song : nullptr;
}

bool can_open_local(const state& state) {
    const song_entry_state* song = selected_song(state);
    return song != nullptr && song->installed;
}

std::string selected_song_id(const state& state) {
    const song_entry_state* song = selected_song(state);
    return song != nullptr ? song->song.song.meta.song_id : "";
}

layout make_layout(float anim_t, Rectangle origin_rect) {
    const float t = ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);

    const Rectangle seed_back = centered_scaled_rect(origin, kBackRect, 0.9f, {-448.0f, -198.0f});
    const Rectangle seed_official_tab = centered_scaled_rect(origin, kOfficialTabRect, 0.84f, {-276.0f, -194.0f});
    const Rectangle seed_community_tab = centered_scaled_rect(origin, kCommunityTabRect, 0.84f, {-132.0f, -194.0f});
    const Rectangle seed_search = centered_scaled_rect(origin, kSearchRect, 0.74f, {266.0f, -192.0f});
    const Rectangle seed_song_lane = centered_scaled_rect(origin, kSongLaneRect, 0.68f, {-316.0f, 56.0f});
    const Rectangle seed_detail = centered_scaled_rect(origin, kDetailRect, 0.74f, {168.0f, 44.0f});
    const Rectangle seed_jacket = centered_scaled_rect(origin, kHeroJacketRect, 0.82f, {-50.0f, -18.0f});
    const Rectangle seed_chart_list = centered_scaled_rect(origin, kChartListRect, 0.86f, {70.0f, 152.0f});
    const Rectangle seed_primary = centered_scaled_rect(origin, kPrimaryActionRect, 0.9f, {-28.0f, 254.0f});
    const Rectangle seed_open_local = centered_scaled_rect(origin, kOpenLocalRect, 0.9f, {176.0f, 254.0f});

    return {
        lerp_rect(seed_back, kBackRect, t),
        lerp_rect(seed_official_tab, kOfficialTabRect, t),
        lerp_rect(seed_community_tab, kCommunityTabRect, t),
        lerp_rect(seed_search, kSearchRect, t),
        lerp_rect(seed_song_lane, kSongLaneRect, t),
        lerp_rect(seed_detail, kDetailRect, t),
        lerp_rect(seed_jacket, kHeroJacketRect, t),
        lerp_rect(seed_chart_list, kChartListRect, t),
        lerp_rect(seed_primary, kPrimaryActionRect, t),
        lerp_rect(seed_open_local, kOpenLocalRect, t),
    };
}

update_result update(state& state, float anim_t, Rectangle origin_rect, float dt) {
    update_result result;
    const layout current = make_layout(anim_t, origin_rect);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const float wheel = GetMouseWheelMove();

    if (ui::is_clicked(current.back_rect) || IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
        return result;
    }

    if (ui::is_clicked(current.official_tab_rect) && state.mode != catalog_mode::official) {
        state.mode = catalog_mode::official;
        state.song_scroll_y = 0.0f;
        state.song_scroll_y_target = 0.0f;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        ensure_selection_valid(state);
        result.song_selection_changed = true;
        return result;
    }
    if (ui::is_clicked(current.community_tab_rect) && state.mode != catalog_mode::community) {
        state.mode = catalog_mode::community;
        state.song_scroll_y = 0.0f;
        state.song_scroll_y_target = 0.0f;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        ensure_selection_valid(state);
        result.song_selection_changed = true;
        return result;
    }

    ensure_selection_valid(state);

    if (left_pressed) {
        const int clicked_song = hit_test_song_list(state, current.song_lane_rect, mouse);
        if (clicked_song >= 0) {
            int& selected_song_index = selected_song_index_ref(state);
            if (selected_song_index != clicked_song) {
                selected_song_index = clicked_song;
                selected_chart_index_ref(state) = 0;
                state.chart_scroll_y = 0.0f;
                state.chart_scroll_y_target = 0.0f;
                result.song_selection_changed = true;
                return result;
            }
        }

        const int clicked_chart = hit_test_chart_list(state, current.chart_list_rect, mouse);
        if (clicked_chart >= 0) {
            int& selected_chart_index = selected_chart_index_ref(state);
            if (selected_chart_index != clicked_chart) {
                selected_chart_index = clicked_chart;
                result.chart_selection_changed = true;
                return result;
            }
        }
    }

    if (ui::is_clicked(current.primary_action_rect)) {
        result.action = requested_action::primary;
        return result;
    }
    if (can_open_local(state) && ui::is_clicked(current.open_local_rect)) {
        result.action = requested_action::open_local;
        return result;
    }

    const bool allow_navigation_keys = !state.search_input.active;
    if (allow_navigation_keys) {
        auto indices = filtered_indices(state);
        int& selected_song_index = selected_song_index_ref(state);
        int& selected_chart_index = selected_chart_index_ref(state);

        if (!indices.empty()) {
            auto selected_it = std::find(indices.begin(), indices.end(), selected_song_index);
            int display_index = selected_it == indices.end() ? 0 : static_cast<int>(selected_it - indices.begin());

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                display_index = std::max(0, display_index - 1);
                selected_song_index = indices[static_cast<size_t>(display_index)];
                selected_chart_index = 0;
                state.chart_scroll_y = 0.0f;
                state.chart_scroll_y_target = 0.0f;
                result.song_selection_changed = true;
            } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                display_index = std::min(static_cast<int>(indices.size()) - 1, display_index + 1);
                selected_song_index = indices[static_cast<size_t>(display_index)];
                selected_chart_index = 0;
                state.chart_scroll_y = 0.0f;
                state.chart_scroll_y_target = 0.0f;
                result.song_selection_changed = true;
            }
        }

        const song_entry_state* song = selected_song(state);
        if (song != nullptr && !song->charts.empty()) {
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                selected_chart_index = std::max(0, selected_chart_index - 1);
                result.chart_selection_changed = true;
            } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                selected_chart_index = std::min(static_cast<int>(song->charts.size()) - 1, selected_chart_index + 1);
                result.chart_selection_changed = true;
            }
        }
    }

    const int filtered_song_count = static_cast<int>(filtered_indices(state).size());
    const song_entry_state* song = selected_song(state);
    const int chart_count = song != nullptr ? static_cast<int>(song->charts.size()) : 0;
    if (CheckCollisionPointRec(mouse, current.song_lane_rect) && wheel != 0.0f) {
        state.song_scroll_y_target -= wheel * 42.0f;
    } else if (CheckCollisionPointRec(mouse, current.chart_list_rect) && wheel != 0.0f) {
        state.chart_scroll_y_target -= wheel * 36.0f;
    }

    state.song_scroll_y_target = std::clamp(state.song_scroll_y_target, 0.0f,
                                            max_song_scroll(current.song_lane_rect, filtered_song_count));
    state.song_scroll_y += (state.song_scroll_y_target - state.song_scroll_y) * std::min(1.0f, dt * 12.0f);
    if (std::fabs(state.song_scroll_y - state.song_scroll_y_target) < 0.5f) {
        state.song_scroll_y = state.song_scroll_y_target;
    }

    state.chart_scroll_y_target = std::clamp(state.chart_scroll_y_target, 0.0f,
                                             max_chart_scroll(current.chart_list_rect, chart_count));
    state.chart_scroll_y += (state.chart_scroll_y_target - state.chart_scroll_y) * std::min(1.0f, dt * 12.0f);
    if (std::fabs(state.chart_scroll_y - state.chart_scroll_y_target) < 0.5f) {
        state.chart_scroll_y = state.chart_scroll_y_target;
    }

    return result;
}

void draw(state& state, float anim_t, Rectangle origin_rect) {
    const auto& t = *g_theme;
    const float play_t = ease_out_cubic(anim_t);
    if (play_t <= 0.01f) {
        return;
    }

    const layout current = make_layout(anim_t, origin_rect);
    const float content_fade_t = std::clamp((play_t - 0.16f) / 0.66f, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(255.0f * content_fade_t);
    const double now = GetTime();
    const Color button_base = t.row_soft;
    const Color button_hover = t.row_soft_hover;
    const Color button_selected = t.row_soft_selected;
    const unsigned char normal_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_alpha) / 255);
    const unsigned char hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_hover_alpha) / 255);
    const unsigned char selected_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_alpha) / 255);

    const bool official_active = state.mode == catalog_mode::official;
    const bool community_active = state.mode == catalog_mode::community;

    ui::draw_button_colored(current.back_rect, "HOME", 16,
                            with_alpha(button_base, normal_row_alpha),
                            with_alpha(button_hover, hover_row_alpha),
                            with_alpha(t.text, alpha), 1.5f);
    ui::draw_button_colored(current.official_tab_rect, "OFFICIAL", 16,
                            with_alpha(official_active ? button_selected : button_base,
                                       official_active ? selected_row_alpha : normal_row_alpha),
                            with_alpha(official_active ? button_selected : button_hover,
                                       official_active ? selected_row_alpha : hover_row_alpha),
                            with_alpha(official_active ? t.text : t.text_secondary, alpha), 1.5f);
    ui::draw_button_colored(current.community_tab_rect, "COMMUNITY", 16,
                            with_alpha(community_active ? button_selected : button_base,
                                       community_active ? selected_row_alpha : normal_row_alpha),
                            with_alpha(community_active ? button_selected : button_hover,
                                       community_active ? selected_row_alpha : hover_row_alpha),
                            with_alpha(community_active ? t.text : t.text_secondary, alpha), 1.5f);

    ui::draw_text_input(current.search_rect, state.search_input, "SEARCH", "songs / artists / id",
                        nullptr, ui::draw_layer::base, 16, 64, ui::default_text_input_filter, 94.0f);

    DrawLineEx({current.song_lane_rect.x + current.song_lane_rect.width + 18.0f, current.song_lane_rect.y + 8.0f},
               {current.song_lane_rect.x + current.song_lane_rect.width + 18.0f,
                current.song_lane_rect.y + current.song_lane_rect.height - 8.0f},
               1.2f, with_alpha(t.border_light, static_cast<unsigned char>(160.0f * play_t)));

    const auto indices = filtered_indices(state);
    const auto& songs = active_songs(state);
    ui::draw_text_in_rect(
        state.mode == catalog_mode::official
            ? (songs.empty() ? "OFFICIAL catalog is not wired yet in this build." : "Curated songs and charts.")
            : (songs.empty() ? "COMMUNITY catalog is empty." : "User-side songs and charts."),
        15,
        {current.song_lane_rect.x, current.song_lane_rect.y - 26.0f, current.song_lane_rect.width, 18.0f},
        with_alpha(t.text_muted, alpha), ui::text_align::left);

    ui::scoped_clip_rect song_clip(current.song_lane_rect);
    for (int display_index = 0; display_index < static_cast<int>(indices.size()); ++display_index) {
        const int song_index = indices[static_cast<size_t>(display_index)];
        const song_entry_state& song = songs[static_cast<size_t>(song_index)];
        const Rectangle row = song_row_rect(current.song_lane_rect, display_index, state.song_scroll_y);
        if (row.y + row.height < current.song_lane_rect.y - 4.0f ||
            row.y > current.song_lane_rect.y + current.song_lane_rect.height + 4.0f) {
            continue;
        }

        const bool selected = song_index == selected_song_index_ref(state);
        const bool hovered = ui::is_hovered(row);
        const unsigned char row_alpha = selected ? selected_row_alpha
            : hovered ? hover_row_alpha
                      : normal_row_alpha;
        DrawRectangleRec(row, with_alpha(selected ? button_selected : button_base, row_alpha));
        DrawRectangleLinesEx(row, 1.0f, with_alpha(t.border_light, static_cast<unsigned char>(150.0f * play_t)));

        const Rectangle jacket_rect = {row.x + 10.0f, row.y + 10.0f, 54.0f, 54.0f};
        if (const Texture2D* jacket = state.jackets.get(song.song.song)) {
            DrawTexturePro(*jacket,
                           {0.0f, 0.0f, static_cast<float>(jacket->width), static_cast<float>(jacket->height)},
                           jacket_rect, {0.0f, 0.0f}, 0.0f, with_alpha(WHITE, alpha));
        } else {
            DrawRectangleRec(jacket_rect, with_alpha(t.section, row_alpha));
            ui::draw_text_in_rect("JK", 18, jacket_rect, with_alpha(t.text_muted, alpha));
        }
        DrawRectangleLinesEx(jacket_rect, 1.2f, with_alpha(t.border_image, alpha));

        draw_marquee_text(song.song.song.meta.title.c_str(),
                          {row.x + 78.0f, row.y + 11.0f, row.width - 168.0f, 20.0f},
                          20, with_alpha(t.text, alpha), now);
        draw_marquee_text(song.song.song.meta.artist.c_str(),
                          {row.x + 78.0f, row.y + 38.0f, row.width - 168.0f, 16.0f},
                          14, with_alpha(t.text_muted, alpha), now);
        ui::draw_text_in_rect(song_status_label(song).c_str(), 12,
                              {row.x + row.width - 88.0f, row.y + 14.0f, 72.0f, 14.0f},
                              with_alpha(song_status_color(song), alpha), ui::text_align::right);
        ui::draw_text_in_rect(TextFormat("%d charts", static_cast<int>(song.charts.size())), 12,
                              {row.x + row.width - 88.0f, row.y + 38.0f, 72.0f, 14.0f},
                              with_alpha(t.text_muted, alpha), ui::text_align::right);
    }

    const song_entry_state* song = selected_song(state);
    const chart_entry_state* chart = selected_chart(state);
    if (song == nullptr) {
        ui::draw_text_in_rect("No songs match the current catalog and search.", 28,
                              current.detail_rect,
                              with_alpha(t.text_muted, alpha), ui::text_align::center);
        ui::draw_notice_queue_bottom_right(state.notices, {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)});
        return;
    }

    if (const Texture2D* hero_jacket = state.jackets.get(song->song.song)) {
        DrawTexturePro(*hero_jacket,
                       {0.0f, 0.0f, static_cast<float>(hero_jacket->width), static_cast<float>(hero_jacket->height)},
                       current.hero_jacket_rect, {0.0f, 0.0f}, 0.0f, with_alpha(WHITE, alpha));
    } else {
        DrawRectangleRec(current.hero_jacket_rect, with_alpha(t.section, alpha));
        ui::draw_text_in_rect("JACKET", 24, current.hero_jacket_rect, with_alpha(t.text_muted, alpha));
    }
    DrawRectangleLinesEx(current.hero_jacket_rect, 1.6f, with_alpha(t.border_image, alpha));

    const Rectangle title_rect = {760.0f, 182.0f, 420.0f, 30.0f};
    const Rectangle artist_rect = {760.0f, 220.0f, 420.0f, 20.0f};
    const Rectangle meta_rect = {760.0f, 258.0f, 420.0f, 80.0f};
    draw_marquee_text(song->song.song.meta.title.c_str(), title_rect, 30, with_alpha(t.text, alpha), now);
    draw_marquee_text(song->song.song.meta.artist.c_str(), artist_rect, 18,
                      with_alpha(t.text_secondary, alpha), now);
    ui::draw_text_in_rect(
        state.mode == catalog_mode::official ? "OFFICIAL CATALOG" : "COMMUNITY CATALOG",
        14,
        {meta_rect.x, meta_rect.y, meta_rect.width, 16.0f},
        with_alpha(state.mode == catalog_mode::official ? t.accent : t.success, alpha),
        ui::text_align::left);
    ui::draw_text_in_rect(
        TextFormat("BPM %.0f   %d charts   song id %s",
                   song->song.song.meta.base_bpm,
                   static_cast<int>(song->charts.size()),
                   song->song.song.meta.song_id.c_str()),
        15,
        {meta_rect.x, meta_rect.y + 24.0f, meta_rect.width, 18.0f},
        with_alpha(t.text_muted, alpha), ui::text_align::left);
    ui::draw_text_in_rect(
        song->update_available ? "A local copy exists and looks older than this catalog entry."
                               : (song->installed ? "This song already exists locally." :
                                                    "This song is not installed locally yet."),
        15,
        {meta_rect.x, meta_rect.y + 50.0f, meta_rect.width, 34.0f},
        with_alpha(t.text_dim, alpha), ui::text_align::left);

    ui::draw_text_in_rect("Charts", 18,
                          {current.chart_list_rect.x, current.chart_list_rect.y - 28.0f,
                           current.chart_list_rect.width, 18.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);

    ui::scoped_clip_rect chart_clip(current.chart_list_rect);
    for (int index = 0; index < static_cast<int>(song->charts.size()); ++index) {
        const chart_entry_state& item = song->charts[static_cast<size_t>(index)];
        const Rectangle row = chart_row_rect(current.chart_list_rect, index, state.chart_scroll_y);
        if (row.y + row.height < current.chart_list_rect.y - 4.0f ||
            row.y > current.chart_list_rect.y + current.chart_list_rect.height + 4.0f) {
            continue;
        }

        const bool selected = chart != nullptr && index == selected_chart_index_ref(state);
        const bool hovered = ui::is_hovered(row);
        const unsigned char row_alpha = selected ? selected_row_alpha
            : hovered ? hover_row_alpha
                      : normal_row_alpha;
        DrawRectangleRec(row, with_alpha(selected ? button_selected : button_base, row_alpha));
        DrawRectangleLinesEx(row, 1.0f, with_alpha(t.border_light, static_cast<unsigned char>(140.0f * play_t)));
        ui::draw_text_in_rect(
            TextFormat("%s  %s", key_mode_label(item.chart.meta.key_count).c_str(), item.chart.meta.difficulty.c_str()),
            16,
            {row.x + 12.0f, row.y + 8.0f, 220.0f, 18.0f},
            with_alpha(key_mode_color(item.chart.meta.key_count), alpha), ui::text_align::left);
        ui::draw_text_in_rect(
            TextFormat("Lv.%.1f  %d Notes  BPM %s",
                       item.chart.meta.level, item.chart.note_count,
                       format_bpm_range(item.chart.min_bpm, item.chart.max_bpm).c_str()),
            13,
            {row.x + 12.0f, row.y + 23.0f, row.width - 130.0f, 14.0f},
            with_alpha(t.text_muted, alpha), ui::text_align::left);
        ui::draw_text_in_rect(chart_status_label(item).c_str(), 12,
                              {row.x + row.width - 80.0f, row.y + 13.0f, 64.0f, 14.0f},
                              with_alpha(item.update_available ? t.accent : (item.installed ? t.success : t.text_muted), alpha),
                              ui::text_align::right);
    }

    const char* primary_label = song->update_available ? "UPDATE SONG"
        : (song->installed ? "DOWNLOAD AGAIN" : "DOWNLOAD SONG");
    ui::draw_button_colored(current.primary_action_rect, primary_label, 16,
                            with_alpha(button_selected, selected_row_alpha),
                            with_alpha(button_selected, hover_row_alpha),
                            with_alpha(t.text, alpha), 1.5f);
    ui::draw_button_colored(current.open_local_rect, "OPEN LOCAL", 16,
                            with_alpha(button_base, song->installed ? normal_row_alpha : static_cast<unsigned char>(normal_row_alpha / 2)),
                            with_alpha(button_hover, song->installed ? hover_row_alpha : static_cast<unsigned char>(hover_row_alpha / 2)),
                            with_alpha(song->installed ? t.text : t.text_muted, alpha), 1.5f);

    ui::draw_notice_queue_bottom_right(state.notices,
                                       {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)});
}

}  // namespace title_online_view
