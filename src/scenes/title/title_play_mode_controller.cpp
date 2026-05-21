#include "title/title_play_mode_controller.h"

#include "title/play_session_controller.h"
#include "title/title_local_song_select_controller.h"

void title_play_mode_controller::update(scene_manager& manager,
                                        song_select::state& state,
                                        song_select::preview_controller& preview_controller,
                                        const title_play_transfer_controller& transfer_controller,
                                        float play_view_anim,
                                        Rectangle play_entry_origin_rect,
                                        float dt,
                                        const callbacks& callbacks) {
    if (transfer_controller.busy()) {
        return;
    }

    const title_play_view::update_result result =
        title_local_song_select_controller::update(
            state, title_play_view::mode::play, play_view_anim, play_entry_origin_rect, dt);

    if (result.back_requested) {
        callbacks.enter_home();
        return;
    }
    if (result.delete_song_requested) {
        return;
    }
    if (result.delete_chart_requested) {
        return;
    }
    if (result.play_requested) {
        title_play_session::start_selected_chart(manager, state, preview_controller);
        return;
    }
    if (result.preview_toggle_requested) {
        if (preview_controller.is_playing()) {
            preview_controller.pause();
        } else {
            preview_controller.resume(song_select::selected_song(state));
        }
        return;
    }
    if (result.update_song_requested) {
        callbacks.open_update_catalog(false);
        return;
    }
    if (result.update_chart_requested) {
        callbacks.open_update_catalog(true);
        return;
    }
    if (result.song_selection_changed) {
        callbacks.sync_media();
        return;
    }
    if (result.chart_selection_changed || result.ranking_source_changed) {
        callbacks.request_ranking_reload();
        return;
    }
}
