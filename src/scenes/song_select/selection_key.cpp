#include "song_select/selection_key.h"

#include "song_select/song_select_state.h"

namespace song_select {

selection_key selection_key_for_state(const state& state) {
    selection_key key;
    key.source = state.ranking_panel.selected_source;
    if (const song_entry* song = selected_song(state)) {
        key.song_id = song->song.meta.song_id;
    }
    const auto filtered = filtered_charts_for_selected_song(state);
    if (const chart_option* chart = selected_chart_for(state, filtered)) {
        key.chart_id = chart->meta.chart_id;
        if (key.source == ranking_service::source::online &&
            !can_use_online_chart_routes(*chart)) {
            key.source = ranking_service::source::local;
        }
    } else {
        key.source = ranking_service::source::local;
    }
    return key;
}

}  // namespace song_select
