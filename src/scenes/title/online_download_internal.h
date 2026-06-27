#pragma once

#include <array>
#include <string>
#include <vector>

#include "raylib.h"
#include "title/online_download_remote_client.h"
#include "title/title_audio_controller.h"
#include "title/online_download_view.h"

namespace title_online_view::detail {

inline constexpr int kSongGridColumns = 3;
inline constexpr int kChartGridColumns = 1;

struct overview_shelf_row {
    std::string key;
    std::vector<int> song_indices;
    int total_count = 0;
    float scroll_x = 0.0f;
};

struct chart_filter_layout {
    Rectangle title_label = {};
    Rectangle search_input = {};
    Rectangle source_label = {};
    Rectangle status_label = {};
    Rectangle level_label = {};
    Rectangle keys_label = {};
};

struct chart_list_header_layout {
    Rectangle title = {};
    Rectangle subtitle = {};
    Rectangle count = {};
};

struct chart_row_layout {
    Rectangle key_mode = {};
    Rectangle difficulty = {};
    Rectangle level_badge = {};
    Rectangle notes = {};
    Rectangle author = {};
    Rectangle download_icon = {};
};

struct chart_source_filter_option {
    chart_source_filter value;
    const char* label;
};

struct chart_key_filter_option {
    int value;
    const char* label;
};

struct chart_status_filter_option {
    int value;
    const char* label;
};

inline constexpr std::array<discovery_view, 6> kDiscoveryViews = {{
    discovery_view::overview,
    discovery_view::new_arrivals,
    discovery_view::rising,
    discovery_view::hidden_gems,
    discovery_view::recommended,
    discovery_view::needs_charts,
}};

inline constexpr std::array<source_filter, 3> kSourceFilters = {{
    source_filter::all,
    source_filter::official,
    source_filter::community,
}};

inline constexpr std::array<chart_source_filter_option, 4> kChartSourceFilters = {{
    {chart_source_filter::all, "ALL"},
    {chart_source_filter::official, "OFFICIAL"},
    {chart_source_filter::community, "COMMUNITY"},
    {chart_source_filter::mine, "MINE"},
}};

inline constexpr std::array<chart_status_filter_option, 3> kChartStatusFilters = {{
    {0, "ANY"},
    {1, "LOCAL"},
    {2, "GET"},
}};

inline constexpr std::array<chart_key_filter_option, 5> kChartKeyFilters = {{
    {0, "ALL"},
    {4, "4K"},
    {5, "5K"},
    {6, "6K"},
    {7, "7K"},
}};

const std::vector<song_entry_state>& active_songs(const state& state);
std::vector<int> filtered_indices(const state& state);
std::vector<int> filtered_chart_indices(const state& state);
bool uses_overview_shelves(const state& state);
std::vector<overview_shelf_row> overview_shelf_rows(const state& state);

int& selected_song_index_ref(state& state);
const int& selected_song_index_ref(const state& state);
int& selected_chart_index_ref(state& state);
const int& selected_chart_index_ref(const state& state);

void ensure_selection_valid(state& state);
void rebuild_visible_discovery_songs(state& state);

float max_song_scroll(Rectangle area, int count);
float max_song_scroll(const state& state, Rectangle area, int count);
Rectangle song_row_rect(Rectangle area, int display_index, float scroll_y);
Rectangle song_row_rect(const state& state, Rectangle area, int display_index, float scroll_y);
Rectangle overview_shelf_track_rect(Rectangle area);
Rectangle overview_shelf_prev_button_rect(Rectangle area, int shelf_row, float scroll_y);
Rectangle overview_shelf_next_button_rect(Rectangle area, int shelf_row, float scroll_y);
Rectangle sidebar_button_rect(Rectangle sidebar, int index);
Rectangle source_button_rect(Rectangle sidebar, int index);
Rectangle chart_source_button_rect(Rectangle chart_list, int index);
Rectangle chart_key_button_rect(Rectangle chart_list, int index);
Rectangle chart_status_button_rect(Rectangle chart_list, int index);
Rectangle chart_clear_button_rect(Rectangle chart_list);
Rectangle chart_level_slider_rect(Rectangle chart_list);
chart_filter_layout chart_filter_layout_for(Rectangle filter_panel);
chart_list_header_layout chart_list_header_layout_for(Rectangle detail_panel);
Rectangle chart_empty_placeholder_rect(Rectangle chart_list);
float max_chart_scroll(Rectangle area, int count);
Rectangle chart_row_rect(Rectangle area, int index, float scroll_y);
Rectangle chart_download_icon_rect(Rectangle chart_card);
chart_row_layout chart_row_layout_for(Rectangle chart_card);
inline Rectangle preview_panel_open_button_rect(Rectangle panel) {
    constexpr float kPreviewPanelInset = 24.0f;
    constexpr float kPreviewOpenButtonBottom = 28.0f;
    constexpr float kPreviewOpenButtonHeight = 58.0f;
    return {
        panel.x + kPreviewPanelInset,
        panel.y + panel.height - kPreviewOpenButtonBottom - kPreviewOpenButtonHeight,
        panel.width - kPreviewPanelInset * 2.0f,
        kPreviewOpenButtonHeight,
    };
}

inline Rectangle preview_panel_play_button_rect(Rectangle panel) {
    constexpr float kPreviewPlayY = 400.0f;
    constexpr float kPreviewPlayWidth = 116.0f;
    constexpr float kPreviewPlayHeight = 54.0f;
    return {
        panel.x + panel.width * 0.5f - kPreviewPlayWidth * 0.5f,
        panel.y + kPreviewPlayY,
        kPreviewPlayWidth,
        kPreviewPlayHeight,
    };
}

inline Rectangle preview_panel_prev_button_rect(Rectangle panel) {
    constexpr float kPreviewPlayY = 400.0f;
    constexpr float kPreviewButtonWidth = 90.0f;
    constexpr float kPreviewButtonHeight = 54.0f;
    constexpr float kPreviewButtonGap = 8.0f;
    return {
        panel.x + panel.width * 0.5f - 58.0f - kPreviewButtonGap - kPreviewButtonWidth,
        panel.y + kPreviewPlayY,
        kPreviewButtonWidth,
        kPreviewButtonHeight,
    };
}

inline Rectangle preview_panel_next_button_rect(Rectangle panel) {
    constexpr float kPreviewPlayY = 400.0f;
    constexpr float kPreviewButtonWidth = 90.0f;
    constexpr float kPreviewButtonHeight = 54.0f;
    constexpr float kPreviewButtonGap = 8.0f;
    return {
        panel.x + panel.width * 0.5f + 58.0f + kPreviewButtonGap,
        panel.y + kPreviewPlayY,
        kPreviewButtonWidth,
        kPreviewButtonHeight,
    };
}

inline Rectangle preview_panel_progress_rect(Rectangle panel) {
    constexpr float kPreviewPanelInset = 24.0f;
    constexpr float kPreviewBarY = 468.0f;
    constexpr float kPreviewBarHeight = 12.0f;
    return {
        panel.x + kPreviewPanelInset,
        panel.y + kPreviewBarY,
        panel.width - kPreviewPanelInset * 2.0f,
        kPreviewBarHeight,
    };
}

Rectangle chart_status_badge_rect(Rectangle chart_card, float badge_width, bool reserves_download_button);
Rectangle chart_source_badge_rect(Rectangle chart_card,
                                  float badge_width,
                                  bool reserves_download_button,
                                  bool status_badge_visible);
Rectangle song_list_viewport(Rectangle content_rect);
int hit_test_song_list(const state& state, Rectangle area, Vector2 point);
int hit_test_chart_list(const state& state, Rectangle area, Vector2 point);

std::string key_mode_label(int key_count);
Color key_mode_color(int key_count);
std::string song_status_label(const song_entry_state& song);
Color song_status_color(const song_entry_state& song);
std::string chart_status_label(const chart_entry_state& chart);
bool can_download_chart(const song_entry_state& song, const chart_entry_state& chart);
const char* catalog_caption(const state& state, const std::vector<song_entry_state>& songs);
std::string format_time_label(double seconds);
double preview_display_length_seconds(const song_entry_state& song, const title_preview_snapshot& preview = {});
song_select::song_entry make_remote_song_entry(const remote_song_payload& song, const std::string& server_url);

}  // namespace title_online_view::detail
