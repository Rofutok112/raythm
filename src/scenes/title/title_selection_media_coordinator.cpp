#include "title/title_selection_media_coordinator.h"

#include "song_select/song_select_navigation.h"
#include "title/title_audio_controller.h"
#include "title/title_play_data_controller.h"

void title_selection_media_coordinator::reset() {
    preview_key_ = {};
    ranking_key_ = {};
    ranking_source_ = ranking_service::source::local;
    preview_synced_ = false;
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
    if (force || !preview_synced_ || preview_key_ != key) {
        audio_controller.select_preview_song(song_select::selected_song(state));
        preview_key_ = key;
        preview_synced_ = true;
    }

    if (active_context != context::play) {
        return;
    }

    const ranking_service::source source = ranking_source_for_selection(state);
    state.ranking_panel.selected_source = source;
    const bool ranking_changed =
        force || !ranking_synced_ || ranking_key_ != key || ranking_source_ != source;
    if (!ranking_changed) {
        return;
    }

    data_controller.request_ranking_reload(state);
    ranking_key_ = key;
    ranking_source_ = source;
    ranking_synced_ = true;
}

void title_selection_media_coordinator::request_ranking_reload(
    song_select::state& state,
    title_play_data_controller& data_controller) {
    data_controller.request_ranking_reload(state);
    ranking_key_ = current_selection_key(state);
    ranking_source_ = state.ranking_panel.selected_source;
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
    }
    return key;
}

ranking_service::source title_selection_media_coordinator::ranking_source_for_selection(
    const song_select::state& state) {
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
    return chart != nullptr && song_select::can_use_online_chart_routes(*chart)
        ? ranking_service::source::online
        : ranking_service::source::local;
}
