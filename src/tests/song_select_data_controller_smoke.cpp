#include <cassert>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "song_select/song_select_data_controller.h"

namespace {

song_select::chart_option make_chart(const char* chart_id) {
    song_select::chart_option chart;
    chart.meta.chart_id = chart_id;
    chart.meta.difficulty = "Normal";
    chart.meta.level = 5;
    chart.meta.key_count = 4;
    return chart;
}

song_select::song_entry make_song(const char* song_id, const char* chart_id) {
    song_select::song_entry song;
    song.song.meta.song_id = song_id;
    song.song.meta.title = song_id;
    song.charts.push_back(make_chart(chart_id));
    return song;
}

template <typename Fn>
void spin_until(Fn fn) {
    for (int i = 0; i < 200; ++i) {
        if (fn()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(false);
}

}  // namespace

namespace song_select {

catalog_data load_catalog(bool) {
    return {};
}

}  // namespace song_select

namespace ranking_service {

listing load_chart_ranking(const std::string&, source ranking_source, int) {
    listing result;
    result.ranking_source = ranking_source;
    return result;
}

}  // namespace ranking_service

int main() {
    int catalog_load_count = 0;
    song_select::data_controller controller(
        [&](bool calculate_missing_levels) {
            ++catalog_load_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            song_select::catalog_data catalog;
            catalog.songs.push_back(make_song(catalog_load_count == 1 ? "song-a" : "song-b",
                                              calculate_missing_levels ? "chart-level" : "chart-normal"));
            return catalog;
        },
        [&](std::string chart_id, ranking_service::source source, int) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ranking_service::listing listing;
            listing.available = true;
            listing.ranking_source = source;
            listing.message = chart_id;
            return listing;
        });

    song_select::state state;
    controller.request_catalog_reload(state, {"song-a", "chart-normal", false});
    assert(state.catalog_loading);
    controller.request_catalog_reload(state, {"song-b", "chart-level", true});

    spin_until([&] {
        const song_select::catalog_reload_result result = controller.poll_catalog_reload(state);
        return result.completed && result.queued_reload_started;
    });
    assert(state.catalog_loaded_once);
    assert(song_select::selected_song(state)->song.meta.song_id == "song-a");

    spin_until([&] {
        return controller.poll_catalog_reload(state).completed;
    });
    assert(catalog_load_count == 2);
    assert(!state.catalog_loading);
    assert(song_select::selected_song(state)->song.meta.song_id == "song-b");
    assert(song_select::selected_chart_for(state, song_select::filtered_charts_for_selected_song(state))
               ->meta.chart_id == "chart-level");

    state.ranking_panel.selected_source = ranking_service::source::online;
    controller.request_ranking_reload(state);
    assert(state.ranking_panel.listing.message == "Loading online rankings...");
    controller.request_ranking_reload(state);

    spin_until([&] {
        const song_select::ranking_reload_result result = controller.poll_ranking_reload(state);
        return result.completed && result.stale && result.queued_reload_started;
    });
    assert(state.ranking_panel.listing.message == "Loading online rankings...");

    spin_until([&] {
        const song_select::ranking_reload_result result = controller.poll_ranking_reload(state);
        return result.completed && !result.stale;
    });
    assert(state.ranking_panel.listing.available);
    assert(state.ranking_panel.listing.message == "chart-level");

    return 0;
}
