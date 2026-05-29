#include <cassert>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>

#include "song_select/song_select_layout.h"
#include "song_select/song_select_state.h"
#include "ui_notice.h"

namespace {

void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ":" << line << ": check failed: " << expression << "\n";
        std::exit(EXIT_FAILURE);
    }
}

#undef assert
#define assert(expr) check((expr), #expr, __FILE__, __LINE__)

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

song_select::chart_option make_online_chart(const char* chart_id,
                                            const char* difficulty,
                                            float level,
                                            const char* server_url) {
    song_select::chart_option chart = make_chart(chart_id, difficulty, level);
    chart.storage = storage_policy::managed_package;
    chart.status = content_status::community;
    chart.source_status = content_status::community;
    chart.online_identity = online_content::chart_identity{
        .server_url = server_url,
        .remote_song_id = "remote-song",
        .remote_chart_id = chart_id,
        .content_source = online_content::source::community,
        .remote_chart_version = 1,
    };
    return chart;
}

song_select::chart_option make_remote_linked_chart(const char* chart_id,
                                                   const char* difficulty,
                                                   float level,
                                                   const char* server_url) {
    song_select::chart_option chart = make_chart(chart_id, difficulty, level);
    chart.storage = storage_policy::managed_package;
    chart.status = content_status::community;
    chart.source_status = content_status::community;
    chart.remote_links.push_back(online_content::chart_identity{
        .server_url = server_url,
        .remote_song_id = "remote-song",
        .remote_chart_id = chart_id,
        .content_source = online_content::source::community,
        .remote_chart_version = 1,
    });
    return chart;
}

song_select::chart_option make_legacy_local_remote_linked_chart(const char* chart_id,
                                                                const char* difficulty,
                                                                float level,
                                                                const char* server_url) {
    song_select::chart_option chart = make_chart(chart_id, difficulty, level);
    chart.remote_links.push_back(online_content::chart_identity{
        .server_url = server_url,
        .remote_song_id = "legacy-remote-song",
        .remote_chart_id = chart_id,
        .content_source = online_content::source::community,
        .remote_chart_version = 1,
    });
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
    assert(std::fabs(song_select::layout::kLoginButtonRect.y - song_select::layout::kSettingsButtonRect.y) < 0.01f);
    assert(std::fabs(song_select::layout::kLoginButtonRect.x + song_select::layout::kLoginButtonRect.width +
                     song_select::layout::kButtonGap -
                     song_select::layout::kSettingsButtonRect.x) < 0.01f);

    const float expected_height = song_select::layout::kSongListTopPadding +
                                  song_select::layout::kRowHeight + 14.0f + 2.0f * 30.0f +
                                  song_select::layout::kRowHeight;
    assert(std::fabs(song_select::content_height(state) - expected_height) < 0.01f);

    ui::clear_global_notices();
    song_select::queue_status_message(state, "First notice", true);
    song_select::queue_status_message(state, "Second notice", false);
    assert(ui::global_notice_queue().items.size() == 2);
    assert(ui::global_notice_queue().items.front().message == "First notice");
    assert(ui::global_notice_queue().items.back().message == "Second notice");

    ui::tick_global_notices(1.0f);
    assert(ui::global_notice_queue().items.size() == 2);
    ui::tick_global_notices(1.0f);
    assert(ui::global_notice_queue().items.empty());

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

    song_select::catalog_data large_catalog;
    for (int i = 0; i < 16; ++i) {
        const std::string song_id = "song-" + std::to_string(i);
        const std::string chart_id = "chart-" + std::to_string(i);
        large_catalog.songs.push_back(make_song(song_id.c_str(), song_id.c_str(),
                                                {make_chart(chart_id.c_str(), "Normal", 5)}));
    }

    song_select::state scrolled_state;
    song_select::apply_catalog(scrolled_state, std::move(large_catalog), "song-15", "chart-15");
    assert(scrolled_state.selected_song_index == 15);
    assert(scrolled_state.scroll_y > 0.0f);
    assert(std::fabs(scrolled_state.scroll_y - scrolled_state.scroll_y_target) < 0.01f);

    song_select::catalog_data multiplayer_catalog;
    assert(!song_select::can_use_online_chart_routes(
        make_legacy_local_remote_linked_chart("legacy-route-check", "Normal", 4, "https://api.example")));
    assert(song_select::can_use_online_chart_routes(
        make_remote_linked_chart("managed-route-check", "Normal", 4, "https://api.example")));
    multiplayer_catalog.songs.push_back(make_song("local-song", "Local",
                                                  {make_chart("local-chart", "Normal", 4)}));
    multiplayer_catalog.songs.push_back(make_song("legacy-local-song", "Legacy Local",
                                                  {make_legacy_local_remote_linked_chart(
                                                      "legacy-local-chart", "Normal", 4,
                                                      "https://api.example/")}));
    multiplayer_catalog.songs.push_back(make_song("linked-song", "Linked",
                                                  {make_remote_linked_chart("linked-chart", "Normal", 4,
                                                                            "https://api.example/")}));
    multiplayer_catalog.songs.push_back(make_song("other-server-song", "Other",
                                                  {make_online_chart("other-chart", "Normal", 4,
                                                                     "https://other.example")}));
    multiplayer_catalog.songs.push_back(make_song("online-song", "Online",
                                                  {make_online_chart("online-chart", "Normal", 4,
                                                                     "https://api.example")}));

    song_select::state multiplayer_state;
    multiplayer_state.filter.multiplayer_queueable_only = true;
    multiplayer_state.filter.multiplayer_queue_server_url = "https://api.example";
    song_select::apply_catalog(multiplayer_state, std::move(multiplayer_catalog), "local-song", "local-chart");
    const std::vector<int> multiplayer_song_indices = song_select::filtered_song_indices(multiplayer_state);
    assert(multiplayer_song_indices.size() == 2);
    assert(multiplayer_state.catalog.songs[multiplayer_song_indices[0]].song.meta.song_id == "linked-song");
    assert(multiplayer_state.catalog.songs[multiplayer_song_indices[1]].song.meta.song_id == "online-song");
    assert(multiplayer_state.catalog.songs[multiplayer_state.selected_song_index].song.meta.song_id == "linked-song");
    const auto multiplayer_charts = song_select::filtered_charts_for_selected_song(multiplayer_state);
    assert(multiplayer_charts.size() == 1);
    assert(multiplayer_charts.front()->meta.chart_id == "linked-chart");

    return 0;
}
