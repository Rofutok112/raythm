#pragma once

#include <string>
#include <vector>

#include "raylib.h"
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
float max_chart_scroll(Rectangle area, int count);
Rectangle chart_row_rect(Rectangle area, int index, float scroll_y);
Rectangle chart_download_icon_rect(Rectangle chart_card);
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
double preview_display_length_seconds(const song_entry_state& song);

}  // namespace title_online_view::detail
