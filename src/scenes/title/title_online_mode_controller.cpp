#include "title/title_online_mode_controller.h"

void title_online_mode_controller::update(title_online_view::state& state,
                                          online_catalog::data_controller& data_controller,
                                          float play_view_anim,
                                          Rectangle play_entry_origin_rect,
                                          float dt,
                                          const callbacks& callbacks) {
    const title_online_view::update_result result =
        title_online_view::update(state, data_controller, play_view_anim, play_entry_origin_rect, dt);
    data_controller.poll_selected_chart_ranking(state);
    data_controller.request_selected_chart_ranking(state);

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
