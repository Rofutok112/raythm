#include "title/play_session_controller.h"

#include "song_select/song_select_navigation.h"

namespace title_play_session {

bool start_selected_chart(scene_manager& manager,
                          song_select::state& state,
                          title_audio_controller& audio_controller) {
    const song_select::song_entry* song = song_select::selected_song(state);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
    if (song == nullptr || chart == nullptr) {
        return false;
    }
    audio_controller.stop_preview();
    manager.change_scene(song_select::make_play_scene(manager, *song, *chart, state.mods));
    return true;
}

}  // namespace title_play_session
