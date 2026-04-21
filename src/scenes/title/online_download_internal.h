#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "title/online_download_view.h"

namespace title_online_view::detail {

inline constexpr int kSongGridColumns = 4;
inline constexpr int kChartGridColumns = 2;

const std::vector<song_entry_state>& active_songs(const state& state);
std::vector<int> filtered_indices(const state& state);

int& selected_song_index_ref(state& state);
const int& selected_song_index_ref(const state& state);
int& selected_chart_index_ref(state& state);
const int& selected_chart_index_ref(const state& state);

void ensure_selection_valid(state& state);

float max_song_scroll(Rectangle area, int count);
Rectangle song_row_rect(Rectangle area, int display_index, float scroll_y);
float max_chart_scroll(Rectangle area, int count);
Rectangle chart_row_rect(Rectangle area, int index, float scroll_y);
Rectangle song_list_viewport(Rectangle content_rect);
int hit_test_song_list(const state& state, Rectangle area, Vector2 point);
int hit_test_chart_list(const state& state, Rectangle area, Vector2 point);

std::string key_mode_label(int key_count);
Color key_mode_color(int key_count);
std::string format_bpm_range(float min_bpm, float max_bpm);
std::string song_status_label(const song_entry_state& song);
Color song_status_color(const song_entry_state& song);
std::string chart_status_label(const chart_entry_state& chart);
const char* catalog_caption(const state& state, const std::vector<song_entry_state>& songs);
std::string format_time_label(double seconds);

}  // namespace title_online_view::detail
