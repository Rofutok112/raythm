#include "title/online_download_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include "audio_manager.h"
#include "theme.h"

namespace title_online_view {
namespace {

constexpr float kSongCardHeight = 256.0f;
constexpr float kSongGridGapX = 18.0f;
constexpr float kSongGridGapY = 19.0f;
constexpr float kChartCardHeight = 92.0f;
constexpr float kChartGridGapX = 22.0f;
constexpr float kChartGridGapY = 18.0f;

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

float song_list_content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    const int rows = (count + detail::kSongGridColumns - 1) / detail::kSongGridColumns;
    return static_cast<float>(rows) * (kSongCardHeight + kSongGridGapY) - kSongGridGapY;
}

float chart_list_content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    const int rows = (count + detail::kChartGridColumns - 1) / detail::kChartGridColumns;
    return static_cast<float>(rows) * (kChartCardHeight + kChartGridGapY) - kChartGridGapY;
}

}  // namespace

namespace detail {

const std::vector<song_entry_state>& active_songs(const state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_songs;
    case catalog_mode::community:
        return state.community_songs;
    case catalog_mode::owned:
        return state.owned_songs;
    }
    return state.official_songs;
}

std::vector<int> filtered_indices(const state& state) {
    std::vector<int> indices;
    const auto& songs = active_songs(state);
    indices.reserve(songs.size());

    for (int index = 0; index < static_cast<int>(songs.size()); ++index) {
        const song_entry_state& song = songs[static_cast<size_t>(index)];
        if (state.search_input.value.empty() ||
            contains_case_insensitive(song.song.song.meta.title, state.search_input.value) ||
            contains_case_insensitive(song.song.song.meta.artist, state.search_input.value)) {
            indices.push_back(index);
        }
    }

    return indices;
}

int& selected_song_index_ref(state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_selected_song_index;
    case catalog_mode::community:
        return state.community_selected_song_index;
    case catalog_mode::owned:
        return state.owned_selected_song_index;
    }
    return state.official_selected_song_index;
}

const int& selected_song_index_ref(const state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_selected_song_index;
    case catalog_mode::community:
        return state.community_selected_song_index;
    case catalog_mode::owned:
        return state.owned_selected_song_index;
    }
    return state.official_selected_song_index;
}

int& selected_chart_index_ref(state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_selected_chart_index;
    case catalog_mode::community:
        return state.community_selected_chart_index;
    case catalog_mode::owned:
        return state.owned_selected_chart_index;
    }
    return state.official_selected_chart_index;
}

const int& selected_chart_index_ref(const state& state) {
    switch (state.mode) {
    case catalog_mode::official:
        return state.official_selected_chart_index;
    case catalog_mode::community:
        return state.community_selected_chart_index;
    case catalog_mode::owned:
        return state.owned_selected_chart_index;
    }
    return state.official_selected_chart_index;
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
        state.detail_open = false;
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

float max_song_scroll(Rectangle area, int count) {
    return std::max(0.0f, song_list_content_height(count) - area.height + 4.0f);
}

Rectangle song_row_rect(Rectangle area, int display_index, float scroll_y) {
    const float width =
        (area.width - static_cast<float>(kSongGridColumns - 1) * kSongGridGapX) /
        static_cast<float>(kSongGridColumns);
    const int row = display_index / kSongGridColumns;
    const int column = display_index % kSongGridColumns;
    return {
        area.x + static_cast<float>(column) * (width + kSongGridGapX),
        area.y + static_cast<float>(row) * (kSongCardHeight + kSongGridGapY) - scroll_y,
        width,
        kSongCardHeight
    };
}

float max_chart_scroll(Rectangle area, int count) {
    return std::max(0.0f, chart_list_content_height(count) - area.height + 4.0f);
}

Rectangle chart_row_rect(Rectangle area, int index, float scroll_y) {
    const float width =
        (area.width - static_cast<float>(kChartGridColumns - 1) * kChartGridGapX) /
        static_cast<float>(kChartGridColumns);
    const int row = index / kChartGridColumns;
    const int column = index % kChartGridColumns;
    return {
        area.x + static_cast<float>(column) * (width + kChartGridGapX),
        area.y + static_cast<float>(row) * (kChartCardHeight + kChartGridGapY) - scroll_y,
        width,
        kChartCardHeight
    };
}

Rectangle chart_download_icon_rect(Rectangle chart_card) {
    return {
        chart_card.x + chart_card.width - 46.0f,
        chart_card.y + 12.0f,
        30.0f,
        26.0f,
    };
}

Rectangle song_list_viewport(Rectangle content_rect) {
    return {
        content_rect.x + 12.0f,
        content_rect.y + 34.0f,
        content_rect.width - 24.0f,
        content_rect.height - 46.0f,
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

std::string song_status_label(const song_entry_state& song) {
    if (song.update_available) {
        return "UPDATE";
    }
    return "";
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
    if (!chart.installed) {
        return "GET";
    }
    return "";
}

bool can_download_chart(const song_entry_state& song, const chart_entry_state& chart) {
    return song.installed && !song.update_available && (!chart.installed || chart.update_available);
}

const char* catalog_caption(const state& state, const std::vector<song_entry_state>& songs) {
    if (state.catalog_loading) {
        return state.catalog_loaded_once ? "Refreshing..." : "Loading...";
    }
    if (state.mode == catalog_mode::owned && state.owned_loading) {
        return "Syncing owned songs...";
    }
    switch (state.mode) {
    case catalog_mode::official:
        return songs.empty() ? "Official catalog unavailable" : "Official catalog";
    case catalog_mode::community:
        return songs.empty() ? "Community catalog unavailable" : "Community catalog";
    case catalog_mode::owned:
        return songs.empty() ? "No owned songs yet" : "Owned library";
    }
    return "Catalog";
}

std::string format_time_label(double seconds) {
    const int total = std::max(0, static_cast<int>(std::floor(seconds + 0.5)));
    return TextFormat("%d:%02d", total / 60, total % 60);
}

double preview_display_length_seconds(const song_entry_state& song) {
    const double metadata_length = static_cast<double>(song.song.song.meta.duration_seconds);
    if (!song.song.song.meta.audio_url.empty()) {
        return metadata_length;
    }

    const double stream_length = audio_manager::instance().get_preview_length_seconds();
    if (metadata_length > 0.0) {
        if (stream_length <= 0.0) {
            return metadata_length;
        }
        return stream_length;
    }
    if (stream_length > 0.0) {
        return stream_length;
    }
    return 0.0;
}

}  // namespace detail

const song_entry_state* selected_song(const state& state) {
    const auto indices = detail::filtered_indices(state);
    if (indices.empty()) {
        return nullptr;
    }

    const auto& songs = detail::active_songs(state);
    const int selected_song_index = detail::selected_song_index_ref(state);
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

    const int selected_chart_index = detail::selected_chart_index_ref(state);
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

}  // namespace title_online_view
