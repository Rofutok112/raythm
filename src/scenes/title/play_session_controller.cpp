#include "title/play_session_controller.h"

#include <string>

#include "ranking_service.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_navigation.h"

namespace title_play_session {

void reload_catalog(song_select::state& state,
                    song_select::preview_controller& preview_controller,
                    const std::string& preferred_song_id,
                    const std::string& preferred_chart_id,
                    bool sync_media_now) {
    song_select::apply_catalog(state, song_select::load_catalog(), preferred_song_id, preferred_chart_id);
    if (sync_media_now) {
        sync_media(state, preview_controller);
    }
}

void sync_media(song_select::state& state, song_select::preview_controller& preview_controller) {
    preview_controller.select_song(song_select::selected_song(state));
    reload_ranking(state);
}

void reload_ranking(song_select::state& state) {
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
    const std::string chart_id = chart != nullptr ? chart->meta.chart_id : "";
    state.ranking_panel.listing =
        ranking_service::load_chart_ranking(chart_id, state.ranking_panel.selected_source, 50);
    state.ranking_panel.scroll_y = 0.0f;
    state.ranking_panel.scroll_y_target = 0.0f;
    state.ranking_panel.reveal_anim = 0.0f;
}

bool start_selected_chart(scene_manager& manager,
                          song_select::state& state,
                          song_select::preview_controller& preview_controller) {
    const song_select::song_entry* song = song_select::selected_song(state);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
    if (song == nullptr || chart == nullptr) {
        return false;
    }
    preview_controller.stop();
    manager.change_scene(song_select::make_play_scene(manager, *song, *chart));
    return true;
}

}  // namespace title_play_session
