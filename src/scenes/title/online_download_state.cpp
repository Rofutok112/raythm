#include "title/online_download_internal.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "audio_manager.h"
#include "network/auth_client.h"
#include "theme.h"

namespace title_online_view {
namespace {

constexpr float kSongCardHeight = 154.0f;
constexpr float kSongGridGapX = 18.0f;
constexpr float kSongGridGapY = 19.0f;
constexpr float kOverviewShelfHeaderHeight = 34.0f;
constexpr float kOverviewShelfArrowLaneWidth = 42.0f;
constexpr float kOverviewShelfArrowGap = 8.0f;
constexpr float kSidebarXInset = 14.0f;
constexpr float kSidebarDiscoveryTitleY = 106.0f;
constexpr float kSidebarSourceTitleY = 594.0f;
constexpr float kSidebarTitleToButtonGap = 42.0f;
constexpr float kSidebarButtonHeight = 54.0f;
constexpr float kSidebarButtonGap = 8.0f;
constexpr float kChartCardHeight = 64.0f;
constexpr float kChartGridGapX = 22.0f;
constexpr float kChartGridGapY = 10.0f;

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

bool contains_any_case_insensitive(const std::vector<std::string>& haystacks, const std::string& needle) {
    return std::any_of(haystacks.begin(), haystacks.end(), [&](const std::string& haystack) {
        return contains_case_insensitive(haystack, needle);
    });
}

std::vector<int> active_song_indices_for_shelf(const state& state, const discovery_shelf_state& shelf) {
    std::vector<int> indices;
    const auto& songs = detail::active_songs(state);
    for (const song_entry_state& shelf_song : shelf.songs) {
        const std::string& song_id = shelf_song.song.song.meta.song_id;
        if (song_id.empty()) {
            continue;
        }
        const auto it = std::find_if(songs.begin(), songs.end(), [&](const song_entry_state& song) {
            return song.song.song.meta.song_id == song_id;
        });
        if (it == songs.end()) {
            continue;
        }
        indices.push_back(static_cast<int>(it - songs.begin()));
    }
    return indices;
}

float parse_filter_float(const std::string& value, float fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        size_t parsed = 0;
        const float result = std::stof(value, &parsed);
        if (parsed == value.size()) {
            return result;
        }
    } catch (...) {
    }
    return fallback;
}

float song_list_content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    const int rows = (count + detail::kSongGridColumns - 1) / detail::kSongGridColumns;
    return static_cast<float>(rows) * (kSongCardHeight + kSongGridGapY) - kSongGridGapY;
}

float overview_shelf_card_width(Rectangle area) {
    return (area.width - kOverviewShelfArrowLaneWidth * 2.0f - kOverviewShelfArrowGap * 2.0f -
            static_cast<float>(detail::kSongGridColumns - 1) * kSongGridGapX) /
        static_cast<float>(detail::kSongGridColumns);
}

float overview_shelf_card_y(Rectangle area, int shelf_row, float scroll_y) {
    return area.y + static_cast<float>(shelf_row) * (kOverviewShelfHeaderHeight + kSongCardHeight + kSongGridGapY) +
        kOverviewShelfHeaderHeight - scroll_y;
}

Rectangle overview_shelf_card_rect(Rectangle area, int shelf_row, int item_index, float row_scroll, float scroll_y) {
    const float width = overview_shelf_card_width(area);
    return {
        area.x + kOverviewShelfArrowLaneWidth + kOverviewShelfArrowGap +
            (static_cast<float>(item_index) - row_scroll) * (width + kSongGridGapX),
        overview_shelf_card_y(area, shelf_row, scroll_y),
        width,
        kSongCardHeight
    };
}

float overview_song_list_content_height(const state& state) {
    const std::vector<detail::overview_shelf_row> rows = detail::overview_shelf_rows(state);
    if (rows.empty()) {
        return 0.0f;
    }
    return static_cast<float>(rows.size()) * (kOverviewShelfHeaderHeight + kSongCardHeight) +
        static_cast<float>(rows.size() - 1) * kSongGridGapY;
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
    return state.official_songs;
}

bool uses_overview_shelves(const state& state) {
    return state.view == discovery_view::overview && state.search_input.value.empty() &&
        !state.discovery_shelves.empty();
}

std::vector<overview_shelf_row> overview_shelf_rows(const state& state) {
    std::vector<overview_shelf_row> rows;
    if (!uses_overview_shelves(state)) {
        return rows;
    }

    for (const discovery_shelf_state& shelf : state.discovery_shelves) {
        if (shelf.key == "fresh_charts") {
            continue;
        }
        const std::vector<int> shelf_indices = active_song_indices_for_shelf(state, shelf);
        if (shelf_indices.empty()) {
            continue;
        }

        const int total_count = static_cast<int>(shelf_indices.size());
        const auto scroll_it = state.overview_shelf_scroll_x.find(shelf.key);
        const float raw_scroll = scroll_it == state.overview_shelf_scroll_x.end() ? 0.0f : scroll_it->second;
        const float max_scroll = static_cast<float>(std::max(0, total_count - kSongGridColumns));

        overview_shelf_row row;
        row.key = shelf.key;
        row.total_count = total_count;
        row.scroll_x = std::clamp(raw_scroll, 0.0f, max_scroll);
        row.song_indices = std::move(shelf_indices);
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<int> filtered_indices(const state& state) {
    std::vector<int> indices;
    const auto& songs = active_songs(state);
    indices.reserve(songs.size());

    if (state.view == discovery_view::overview && state.search_input.value.empty() &&
        !state.discovery_shelves.empty()) {
        for (const overview_shelf_row& row : overview_shelf_rows(state)) {
            indices.insert(indices.end(), row.song_indices.begin(), row.song_indices.end());
        }
        if (!indices.empty()) {
            return indices;
        }
    }

    for (int index = 0; index < static_cast<int>(songs.size()); ++index) {
        const song_entry_state& song = songs[static_cast<size_t>(index)];
        if (state.search_input.value.empty() ||
            contains_case_insensitive(song.song.song.meta.title, state.search_input.value) ||
            contains_case_insensitive(song.song.song.meta.artist, state.search_input.value) ||
            contains_case_insensitive(song.song.song.meta.genre, state.search_input.value) ||
            contains_any_case_insensitive(song.song.song.meta.genres, state.search_input.value) ||
            contains_any_case_insensitive(song.song.song.meta.keywords, state.search_input.value)) {
            indices.push_back(index);
        }
    }

    return indices;
}

std::vector<int> filtered_chart_indices(const state& state) {
    std::vector<int> indices;
    const song_entry_state* song = selected_song(state);
    if (song == nullptr) {
        return indices;
    }

    indices.reserve(song->charts.size());
    const std::string& query = state.chart_search_input.value;
    const float min_level = parse_filter_float(state.min_level_input.value, 1.0f);
    const float max_level = parse_filter_float(state.max_level_input.value, 10.0f);
    const std::optional<auth::session> session =
        state.chart_source == chart_source_filter::mine ? auth::load_saved_session() : std::nullopt;
    for (int index = 0; index < static_cast<int>(song->charts.size()); ++index) {
        const chart_entry_state& chart = song->charts[static_cast<size_t>(index)];
        if (state.chart_source == chart_source_filter::official &&
            chart.chart.source_status != content_status::official) {
            continue;
        }
        if (state.chart_source == chart_source_filter::community &&
            chart.chart.source_status != content_status::community) {
            continue;
        }
        if (state.chart_source == chart_source_filter::mine &&
            (!session.has_value() || chart.uploader_id != session->user.id)) {
            continue;
        }
        if (state.chart_key_filter > 0 && chart.chart.meta.key_count != state.chart_key_filter) {
            continue;
        }
        if (state.chart_download_filter == 1 && !chart.installed) {
            continue;
        }
        if (state.chart_download_filter == 2 && chart.installed) {
            continue;
        }
        if (chart.chart.meta.level < min_level || chart.chart.meta.level > max_level) {
            continue;
        }
        const std::string key_label = key_mode_label(chart.chart.meta.key_count);
        const std::string level_label = TextFormat("lv %.1f", chart.chart.meta.level);
        if (query.empty() ||
            contains_case_insensitive(chart.chart.meta.difficulty, query) ||
            contains_case_insensitive(chart.chart.meta.chart_author, query) ||
            contains_case_insensitive(key_label, query) ||
            contains_case_insensitive(level_label, query)) {
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

    const auto chart_indices = filtered_chart_indices(state);
    if (chart_indices.empty()) {
        selected_chart_index = 0;
    } else if (std::find(chart_indices.begin(), chart_indices.end(), selected_chart_index) == chart_indices.end()) {
        selected_chart_index = chart_indices.front();
    }
}

float max_song_scroll(Rectangle area, int count) {
    return std::max(0.0f, song_list_content_height(count) - area.height + 4.0f);
}

float max_song_scroll(const state& state, Rectangle area, int count) {
    if (detail::uses_overview_shelves(state)) {
        return std::max(0.0f, overview_song_list_content_height(state) - area.height + 4.0f);
    }
    return max_song_scroll(area, count);
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

Rectangle song_row_rect(const state& state, Rectangle area, int display_index, float scroll_y) {
    if (!detail::uses_overview_shelves(state)) {
        return song_row_rect(area, display_index, scroll_y);
    }

    const std::vector<overview_shelf_row> rows = overview_shelf_rows(state);
    int remaining_index = display_index;
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        const overview_shelf_row& shelf_row = rows[static_cast<size_t>(row)];
        const int row_count = static_cast<int>(shelf_row.song_indices.size());
        if (remaining_index < row_count) {
            return overview_shelf_card_rect(area, row, remaining_index, shelf_row.scroll_x, scroll_y);
        }
        remaining_index -= row_count;
    }
    return song_row_rect(area, display_index, scroll_y);
}

Rectangle overview_shelf_track_rect(Rectangle area) {
    return {
        area.x + kOverviewShelfArrowLaneWidth + kOverviewShelfArrowGap,
        area.y,
        area.width - kOverviewShelfArrowLaneWidth * 2.0f - kOverviewShelfArrowGap * 2.0f,
        area.height
    };
}

Rectangle overview_shelf_prev_button_rect(Rectangle area, int shelf_row, float scroll_y) {
    return {
        area.x,
        overview_shelf_card_y(area, shelf_row, scroll_y),
        kOverviewShelfArrowLaneWidth,
        kSongCardHeight
    };
}

Rectangle overview_shelf_next_button_rect(Rectangle area, int shelf_row, float scroll_y) {
    return {
        area.x + area.width - kOverviewShelfArrowLaneWidth,
        overview_shelf_card_y(area, shelf_row, scroll_y),
        kOverviewShelfArrowLaneWidth,
        kSongCardHeight
    };
}

Rectangle sidebar_button_rect(Rectangle sidebar, int index) {
    return {
        sidebar.x + kSidebarXInset,
        sidebar.y + kSidebarDiscoveryTitleY + kSidebarTitleToButtonGap +
            static_cast<float>(index) * (kSidebarButtonHeight + kSidebarButtonGap),
        sidebar.width - kSidebarXInset * 2.0f,
        kSidebarButtonHeight,
    };
}

Rectangle source_button_rect(Rectangle sidebar, int index) {
    return {
        sidebar.x + kSidebarXInset,
        sidebar.y + kSidebarSourceTitleY + kSidebarTitleToButtonGap +
            static_cast<float>(index) * (kSidebarButtonHeight + kSidebarButtonGap),
        sidebar.width - kSidebarXInset * 2.0f,
        kSidebarButtonHeight,
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
    constexpr float kIconWidth = 30.0f;
    constexpr float kIconHeight = 26.0f;
    return {
        chart_card.x + chart_card.width - 46.0f,
        chart_card.y + (chart_card.height - kIconHeight) * 0.5f,
        kIconWidth,
        kIconHeight,
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
        if (CheckCollisionPointRec(point, song_row_rect(state, area, display_index, state.song_scroll_y))) {
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

    const auto indices = filtered_chart_indices(state);
    for (int display_index = 0; display_index < static_cast<int>(indices.size()); ++display_index) {
        if (CheckCollisionPointRec(point, chart_row_rect(area, display_index, state.chart_scroll_y))) {
            return indices[static_cast<size_t>(display_index)];
        }
    }
    return -1;
}

std::string key_mode_label(int key_count) {
    return key_count > 0 ? TextFormat("%dK", key_count) : "-";
}

Color key_mode_color(int key_count) {
    const auto& theme = *g_theme;
    switch (key_count) {
    case 4:
        return theme.rank_b;
    case 5:
        return theme.rank_a;
    case 6:
        return theme.rank_c;
    case 7:
        return theme.rank_s;
    default:
        return theme.text_secondary;
    }
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
    return song.installed && (!chart.installed || chart.update_available);
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
    const auto indices = detail::filtered_chart_indices(state);
    if (song == nullptr || indices.empty()) {
        return nullptr;
    }

    const int selected_chart_index = detail::selected_chart_index_ref(state);
    if (selected_chart_index < 0 || selected_chart_index >= static_cast<int>(song->charts.size())) {
        return nullptr;
    }
    if (std::find(indices.begin(), indices.end(), selected_chart_index) == indices.end()) {
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
    if (song == nullptr) {
        return "";
    }
    if (song->installed && !song->installed_local_song_id.empty()) {
        return song->installed_local_song_id;
    }
    return song->song.song.meta.song_id;
}

}  // namespace title_online_view
