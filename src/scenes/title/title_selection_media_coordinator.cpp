#include "title/title_selection_media_coordinator.h"

#include <utility>

#include "network/auth_client.h"
#include "song_select/song_select_navigation.h"
#include "title/title_audio_controller.h"

namespace {

bool has_queueable_link_for_server(const song_select::chart_option& chart, const std::string& server_url) {
    if (!song_select::can_use_online_chart_routes(chart)) {
        return false;
    }
    const std::string normalized_server_url = auth::normalize_server_url(server_url);
    if (normalized_server_url.empty()) {
        return false;
    }
    if (online_content::is_queueable(chart.online_identity) &&
        auth::normalize_server_url(chart.online_identity->server_url) == normalized_server_url) {
        return true;
    }
    for (const online_content::chart_identity& link : chart.remote_links) {
        if (online_content::is_queueable(link) &&
            auth::normalize_server_url(link.server_url) == normalized_server_url) {
            return true;
        }
    }
    return false;
}

bool uses_submitted_ranking_best(const song_select::chart_option* chart) {
    if (chart == nullptr) {
        return false;
    }
    if (!song_select::can_use_online_chart_routes(*chart)) {
        return false;
    }
    if (chart->source == content_source::official ||
            chart->source == content_source::community) {
        return true;
    }

    const auth::session_summary summary = auth::load_session_summary();
    return summary.logged_in && has_queueable_link_for_server(*chart, summary.server_url);
}

}  // namespace

title_selection_media_coordinator::title_selection_media_coordinator()
    : ranking_controller_(song_select::ranking_load_controller::listing_loader(load_ranking_from_service)) {
}

void title_selection_media_coordinator::reset() {
    audio_key_ = {};
    jacket_key_ = {};
    ranking_key_ = {};
    current_key_ = {};
    audio_synced_ = false;
    jacket_synced_ = false;
    ranking_synced_ = false;
}

void title_selection_media_coordinator::reset(song_select::state& state) {
    reset();
    ranking_controller_.reset();
    state.ranking_panel.selected_source = ranking_service::source::local;
}

void title_selection_media_coordinator::sync_current(
    song_select::state& state,
    title_audio_controller& audio_controller,
    context active_context,
    bool force) {
    if (active_context == context::none) {
        return;
    }

    current_key_ = song_select::selection_key_for_state(state);
    const song_select::selection_key& key = current_key_;
    const song_select::selection_key next_preview = song_select::song_media_key_for(key);
    if (force || !jacket_synced_ || jacket_key_ != next_preview) {
        audio_controller.request_preview_jacket(next_preview, song_select::selected_song(state));
        jacket_key_ = next_preview;
        jacket_synced_ = true;
    }
    if (force || !audio_synced_ || audio_key_ != next_preview) {
        audio_controller.request_preview_audio(next_preview, song_select::selected_song(state));
        audio_key_ = next_preview;
        audio_synced_ = true;
    }

    if (active_context != context::play) {
        return;
    }

    sync_ranking(state, key, force);
}

void title_selection_media_coordinator::request_ranking_reload(song_select::state& state) {
    current_key_ = song_select::selection_key_for_state(state);
    sync_ranking(state, current_key_, true);
}

void title_selection_media_coordinator::poll_ranking_reload(song_select::state& state) {
    current_key_ = song_select::selection_key_for_state(state);
    const song_select::ranking_load_request request = ranking_request_for(state, current_key_);
    const song_select::ranking_reload_result result = ranking_controller_.poll(request);
    if (result.loaded.has_value()) {
        apply_ranking_loaded(state, std::move(*result.loaded));
    }
    if (result.started_request.has_value()) {
        apply_ranking_request_started(state, *result.started_request);
    }
}

title_selection_media_snapshot title_selection_media_coordinator::media_snapshot(
    const song_select::state& state,
    const title_audio_controller& audio_controller) const {
    title_selection_media_snapshot snapshot;
    snapshot.key = song_select::selection_key_for_state(state);
    snapshot.preview = audio_controller.preview_snapshot(song_select::selected_song(state));
    snapshot.ranking = song_select::ranking_snapshot_for_key(ranking_controller_.current(), snapshot.key);
    return snapshot;
}

song_select::ranking_load_request title_selection_media_coordinator::ranking_request_for(
    const song_select::state& state,
    const song_select::selection_key& key) {
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);

    song_select::ranking_load_request request;
    request.key = key;
    request.best_source = uses_submitted_ranking_best(chart)
        ? ranking_service::source::online
        : ranking_service::source::local;
    return request;
}

ranking_service::listing title_selection_media_coordinator::load_ranking_from_service(
    std::string chart_id,
    ranking_service::source source,
    int limit) {
    return ranking_service::load_chart_ranking(std::move(chart_id), source, limit);
}

void title_selection_media_coordinator::sync_ranking(
    song_select::state& state,
    const song_select::selection_key& key,
    bool force) {
    const song_select::selection_key next_ranking = key;
    state.ranking_panel.selected_source = next_ranking.source;
    const bool ranking_changed =
        force || !ranking_synced_ || ranking_key_ != next_ranking;
    if (!ranking_changed) {
        return;
    }

    song_select::ranking_load_request request = ranking_request_for(state, key);
    const song_select::ranking_request_result result =
        ranking_controller_.request_reload(request);
    if (result.accepted_request.has_value()) {
        apply_ranking_request_started(state, *result.accepted_request);
    }
    ranking_key_ = next_ranking;
    ranking_synced_ = true;
}

void title_selection_media_coordinator::apply_ranking_request_started(
    song_select::state& state,
    const song_select::ranking_load_request& request) const {
    reset_ranking_panel_scroll(state);
    if (request.refresh_best) {
        state.ranking_panel.best_source = request.best_source;
        state.ranking_panel.best_chart_id = request.key.chart_id;
        state.ranking_panel.best_loaded = false;
        state.ranking_panel.best_entry.reset();
    }
    mark_online_loading(state, request.key.source);
}

void title_selection_media_coordinator::apply_ranking_loaded(
    song_select::state& state,
    song_select::ranking_load_data loaded) const {
    state.ranking_panel.listing = std::move(loaded.listing);
    if (loaded.best_refreshed) {
        state.ranking_panel.best_source = loaded.best_source;
        state.ranking_panel.best_chart_id = loaded.best_chart_id;
        state.ranking_panel.best_entry = std::move(loaded.best_entry);
        state.ranking_panel.best_loaded = true;
    }
    state.ranking_panel.reveal_anim = 0.0f;
}

void title_selection_media_coordinator::mark_online_loading(song_select::state& state,
                                                           ranking_service::source source) const {
    if (source != ranking_service::source::online) {
        return;
    }

    state.ranking_panel.listing = {};
    state.ranking_panel.listing.ranking_source = source;
    state.ranking_panel.listing.available = false;
    state.ranking_panel.listing.message = "ランキング読み込み中...";
}

void title_selection_media_coordinator::reset_ranking_panel_scroll(song_select::state& state) const {
    state.ranking_panel.source_dropdown_open = false;
    state.ranking_panel.scroll_y = 0.0f;
    state.ranking_panel.scroll_y_target = 0.0f;
    state.ranking_panel.scrollbar_dragging = false;
    state.ranking_panel.scrollbar_drag_offset = 0.0f;
}
