#include "title/title_online_mode_controller.h"

#include <string>
#include <utility>

namespace {

std::string selected_ranking_chart_id(const title_online_view::state& state) {
    const title_online_view::chart_entry_state* chart = title_online_view::selected_chart(state);
    return chart != nullptr ? chart->chart.meta.chart_id : "";
}

void set_select_chart_message(title_online_view::state& state) {
    state.ranking_loading = false;
    state.ranking_listing = {};
    state.ranking_listing.ranking_source = ranking_service::source::online;
    state.ranking_listing.available = false;
    state.ranking_listing.message = "Select a chart to view global ranking.";
}

void set_loading_message(title_online_view::state& state) {
    state.ranking_loading = true;
    state.ranking_listing = {};
    state.ranking_listing.ranking_source = ranking_service::source::online;
    state.ranking_listing.available = false;
    state.ranking_listing.message = "Loading global ranking...";
}

void poll_selected_chart_ranking(title_online_view::state& state,
                                 online_catalog::ranking_load_controller& ranking_controller) {
    const online_catalog::ranking_load_controller::poll_result result =
        ranking_controller.poll(selected_ranking_chart_id(state));
    if (!result.completed) {
        return;
    }

    state.ranking_loading = false;
    if (result.loaded.has_value()) {
        state.ranking_listing = std::move(*result.loaded);
    }
}

void request_selected_chart_ranking(title_online_view::state& state,
                                    online_catalog::ranking_load_controller& ranking_controller) {
    if (!state.detail_open) {
        return;
    }

    const std::string chart_id = selected_ranking_chart_id(state);
    if (chart_id.empty()) {
        ranking_controller.reset();
        set_select_chart_message(state);
        return;
    }

    if (ranking_controller.request(chart_id)) {
        set_loading_message(state);
    }
}

}  // namespace

void title_online_mode_controller::update(title_online_view::state& state,
                                          online_catalog::data_controller& data_controller,
                                          online_catalog::ranking_load_controller& ranking_controller,
                                          title_audio_controller& audio_controller,
                                          float play_view_anim,
                                          Rectangle play_entry_origin_rect,
                                          float dt,
                                          const callbacks& callbacks) {
    const title_online_view::update_result result =
        title_online_view::update(state, data_controller, audio_controller, play_view_anim, play_entry_origin_rect, dt);
    poll_selected_chart_ranking(state, ranking_controller);
    request_selected_chart_ranking(state, ranking_controller);

    if (result.back_requested) {
        callbacks.enter_home();
        return;
    }
    if (result.action == title_online_view::requested_action::primary) {
        title_online_view::start_download(state, data_controller);
        return;
    }
    if (result.action == title_online_view::requested_action::download_chart) {
        title_online_view::start_chart_download(state, data_controller);
        return;
    }
    if (result.action == title_online_view::requested_action::restart_preview) {
        callbacks.resume_preview();
        return;
    }
    if (result.action == title_online_view::requested_action::stop_preview) {
        callbacks.pause_preview();
        return;
    }
    if (result.action == title_online_view::requested_action::open_local) {
        callbacks.open_local_selection();
        return;
    }
    if (result.song_selection_changed) {
        callbacks.select_preview_song();
    }
}
