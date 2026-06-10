#include "title/title_selection_media_coordinator.h"

#include "song_select/song_select_navigation.h"
#include "title/title_audio_controller.h"
#include "title/title_play_data_controller.h"

void title_selection_media_coordinator::reset() {
    audio_key_ = {};
    jacket_key_ = {};
    ranking_key_ = {};
    audio_synced_ = false;
    jacket_synced_ = false;
    ranking_synced_ = false;
}

void title_selection_media_coordinator::sync_current(
    song_select::state& state,
    title_audio_controller& audio_controller,
    title_play_data_controller& data_controller,
    context active_context,
    bool force) {
    if (active_context == context::none) {
        return;
    }

    const selection_key key = current_selection_key(state);
    const preview_key next_preview = preview_key_for(key);
    if (force || !jacket_synced_ || jacket_key_ != next_preview) {
        audio_controller.request_preview_jacket(song_select::selected_song(state));
        jacket_key_ = next_preview;
        jacket_synced_ = true;
    }
    if (force || !audio_synced_ || audio_key_ != next_preview) {
        audio_controller.request_preview_audio(song_select::selected_song(state));
        audio_key_ = next_preview;
        audio_synced_ = true;
    }

    if (active_context != context::play) {
        return;
    }

    const ranking_key next_ranking = ranking_key_for(key);
    state.ranking_panel.selected_source = next_ranking.source;
    const bool ranking_changed =
        force || !ranking_synced_ || ranking_key_ != next_ranking;
    if (!ranking_changed) {
        return;
    }

    data_controller.request_ranking_reload(state);
    ranking_key_ = next_ranking;
    ranking_synced_ = true;
}

void title_selection_media_coordinator::request_ranking_reload(
    song_select::state& state,
    title_play_data_controller& data_controller) {
    data_controller.request_ranking_reload(state);
    const selection_key key = current_selection_key(state);
    ranking_key_ = ranking_key{key.chart_id, state.ranking_panel.selected_source};
    ranking_synced_ = true;
}

title_selection_media_coordinator::selection_key
title_selection_media_coordinator::current_selection_key(const song_select::state& state) {
    selection_key key;
    if (const song_select::song_entry* song = song_select::selected_song(state)) {
        key.song_id = song->song.meta.song_id;
    }
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    if (const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered)) {
        key.chart_id = chart->meta.chart_id;
        key.ranking_source = song_select::can_use_online_chart_routes(*chart)
            ? ranking_service::source::online
            : ranking_service::source::local;
    }
    return key;
}

title_selection_media_coordinator::preview_key
title_selection_media_coordinator::preview_key_for(const selection_key& key) {
    return preview_key{key.song_id};
}

title_selection_media_coordinator::ranking_key
title_selection_media_coordinator::ranking_key_for(const selection_key& key) {
    return ranking_key{key.chart_id, key.ranking_source};
}
