#include "song_select/selection_key.h"

#include "song_select/ranking_source_policy.h"
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
        key.source = ranking_source_policy::effective_source(
            ranking_source_policy::availability_for_chart(chart),
            key.source);
    } else {
        key.source = ranking_service::source::local;
    }
    return key;
}

}  // namespace song_select
