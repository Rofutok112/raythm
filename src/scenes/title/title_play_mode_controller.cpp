#include "title/title_play_mode_controller.h"

#include "title/play_session_controller.h"
#include "title/title_local_song_select_controller.h"

title_play_mode_controller::update_result title_play_mode_controller::update(
                                        scene_manager& manager,
                                        song_select::state& state,
                                        title_audio_controller& audio_controller,
                                        const title_play_transfer_controller& transfer_controller,
                                        const title_selection_media_snapshot& media,
                                        float play_view_anim,
                                        Rectangle play_entry_origin_rect,
                                        float dt,
                                        const callbacks& callbacks) {
    update_result output;
    if (transfer_controller.busy()) {
        return output;
    }

    const title_play_view::update_result result =
        title_local_song_select_controller::update(
            state,
            title_play_view::mode::play,
            play_view_anim,
            play_entry_origin_rect,
            dt,
            nullptr,
            media.preview);

    if (result.back_requested) {
        output.title_command = title::command::enter_home();
        return output;
    }
    if (result.delete_song_requested) {
        return output;
    }
    if (result.delete_chart_requested) {
        return output;
    }
    if (result.multiplayer_select_requested) {
        output.title_command = title::command::add_selected_to_multiplayer();
        return output;
    }
    if (result.play_requested) {
        title_play_session::start_selected_chart(manager, state, audio_controller);
        return output;
    }
    if (result.preview_pause_requested) {
        audio_controller.pause_preview();
    }
    if (result.preview_seek_requested) {
        audio_controller.seek_preview(result.preview_seek_seconds);
    }
    if (result.preview_resume_requested) {
        audio_controller.play_preview_from_current();
    }
    if (result.preview_toggle_requested) {
        audio_controller.toggle_preview_song(media.key, song_select::selected_song(state));
        return output;
    }
    if (result.update_song_requested) {
        output.title_command.type = title::command_type::open_update_catalog;
        output.update_catalog_include_chart = false;
        return output;
    }
    if (result.update_chart_requested) {
        output.title_command.type = title::command_type::open_update_catalog;
        output.update_catalog_include_chart = true;
        return output;
    }
    if (result.song_selection_changed || result.chart_selection_changed) {
        callbacks.sync_media();
        return output;
    }
    if (result.ranking_source_changed) {
        callbacks.request_ranking_reload();
        return output;
    }
    if (!result.requested_profile_user_id.empty()) {
        output.title_command = title::command::open_public_profile(result.requested_profile_user_id);
        return output;
    }
    return output;
}
