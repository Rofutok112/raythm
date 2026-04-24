#include "title/play_session_controller.h"

#include "song_select/song_select_navigation.h"

namespace title_play_session {

void sync_preview(song_select::state& state, song_select::preview_controller& preview_controller) {
    preview_controller.select_song(song_select::selected_song(state));
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
