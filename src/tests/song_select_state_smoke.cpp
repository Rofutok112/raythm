#include <cassert>
#include <cmath>
#include <vector>

#include "song_select/song_select_layout.h"
#include "song_select/song_select_state.h"

namespace {

song_select::chart_option make_chart(const char* chart_id, const char* difficulty, float level) {
    song_select::chart_option chart;
    chart.path = chart_id;
    chart.meta.chart_id = chart_id;
    chart.meta.difficulty = difficulty;
    chart.meta.level = level;
    chart.meta.chart_author = "tester";
    chart.meta.key_count = 4;
    return chart;
}

song_select::song_entry make_song(const char* song_id, const char* title, std::vector<song_select::chart_option> charts) {
    song_select::song_entry song;
    song.song.meta.song_id = song_id;
    song.song.meta.title = title;
    song.song.meta.artist = "artist";
    song.charts = std::move(charts);
    return song;
}

}  // namespace

int main() {
    song_select::catalog_data catalog;
    catalog.songs.push_back(make_song("song-a", "Alpha",
                                      {make_chart("chart-a-1", "Normal", 3),
                                       make_chart("chart-a-2", "Hyper", 7)}));
    catalog.songs.push_back(make_song("song-b", "Beta",
                                      {make_chart("chart-b-1", "Normal", 5)}));

    song_select::state state;
    song_select::apply_catalog(state, std::move(catalog), "song-a", "chart-a-2");
    assert(state.selected_song_index == 0);
    assert(state.difficulty_index == 1);
    assert(state.scroll_y == 0.0f);
    assert(state.scroll_y_target == 0.0f);

    const float expected_height = song_select::layout::kRowHeight + 14.0f + 2.0f * 30.0f + song_select::layout::kRowHeight;
    assert(std::fabs(song_select::content_height(state) - expected_height) < 0.01f);

    const bool changed = song_select::apply_song_selection(state, 1, 3);
    assert(changed);
    assert(state.selected_song_index == 1);
    assert(state.difficulty_index == 0);

    song_select::apply_song_selection(state, 0, 1);
    assert(song_select::expanded_row_height(state, 0) ==
           song_select::layout::kRowHeight + 14.0f + 2.0f * 30.0f);
    assert(song_select::expanded_row_height(state, 1) == song_select::layout::kRowHeight);
    assert(song_select::fallback_song_id_after_song_delete(state, 0) == "song-b");
    assert(song_select::fallback_chart_id_after_chart_delete(state, 0, 0) == "chart-a-2");

    return 0;
}
