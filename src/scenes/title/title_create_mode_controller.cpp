#include "title/title_create_mode_controller.h"

#include <optional>

#include "models/data_models.h"
#include "song_select/song_select_navigation.h"
#include "title/seamless_song_select_view.h"
#include "ui_notice.h"

namespace {

bool can_upload_content(content_status status) {
    return status == content_status::local;
}

}  // namespace

void title_create_mode_controller::update(scene_manager& manager,
                                          song_select::state& state,
                                          title_play_transfer_controller& transfer_controller,
                                          float play_view_anim,
                                          Rectangle play_entry_origin_rect,
                                          float dt,
                                          const callbacks& callbacks) {
    const title_play_view::update_result result =
        title_play_view::update(state, title_play_view::mode::create, play_view_anim, play_entry_origin_rect, dt);
    const bool create_action_requested =
        result.create_song_requested ||
        result.edit_song_requested ||
        result.upload_song_requested ||
        result.import_song_requested ||
        result.export_song_requested ||
        result.create_chart_requested ||
        result.edit_chart_requested ||
        result.upload_chart_requested ||
        result.import_chart_requested ||
        result.export_chart_requested ||
        result.edit_mv_requested ||
        result.manage_library_requested;

    if (result.back_requested) {
        callbacks.enter_home();
        return;
    }
    if (result.song_selection_changed) {
        callbacks.sync_preview();
        return;
    }
    if (result.chart_selection_changed) {
        return;
    }
    if (callbacks.upload_in_progress() && create_action_requested) {
        ui::notify("Wait for the current upload to finish.", ui::notice_tone::info, 1.8f);
        return;
    }
    if (transfer_controller.busy() && create_action_requested) {
        ui::notify("Wait for the current transfer to finish.", ui::notice_tone::info, 1.8f);
        return;
    }

    const song_select::song_entry* song = song_select::selected_song(state);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);

    if (result.create_song_requested) {
        manager.change_scene(song_select::make_song_create_scene(manager));
        return;
    }
    if (result.edit_song_requested && song != nullptr) {
        manager.change_scene(song_select::make_edit_song_scene(manager, *song));
        return;
    }
    if (result.import_song_requested) {
        transfer_controller.start_song_import(state);
        return;
    }
    if (result.export_song_requested) {
        if (song == nullptr) {
            song_select::queue_status_message(state, "Select a song to export.", true);
        } else {
            transfer_controller.start_song_export(state);
        }
        return;
    }
    if (result.upload_song_requested) {
        if (song == nullptr) {
            song_select::queue_status_message(state, "Select a song to upload.", true);
        } else if (!can_upload_content(song->status)) {
            ui::notify("Only Local songs can be uploaded.", ui::notice_tone::error, 2.8f);
        } else {
            callbacks.start_song_upload(*song);
        }
        return;
    }
    if (result.create_chart_requested && song != nullptr) {
        manager.change_scene(song_select::make_new_chart_scene(manager, *song, state.difficulty_index));
        return;
    }
    if (result.edit_chart_requested && song != nullptr && chart != nullptr) {
        manager.change_scene(song_select::make_edit_chart_scene(manager, *song, *chart));
        return;
    }
    if (result.import_chart_requested) {
        if (song == nullptr) {
            song_select::queue_status_message(state, "Select a song before importing a chart.", true);
        } else {
            transfer_controller.start_chart_import(state, callbacks.transfer_callbacks(),
                                                   callbacks.sync_media_on_transfer());
        }
        return;
    }
    if (result.export_chart_requested) {
        if (song == nullptr || chart == nullptr) {
            song_select::queue_status_message(state, "Select a chart to export.", true);
        } else {
            transfer_controller.start_chart_export(state, callbacks.transfer_callbacks(),
                                                   callbacks.sync_media_on_transfer());
        }
        return;
    }
    if (result.upload_chart_requested) {
        if (song == nullptr || chart == nullptr) {
            song_select::queue_status_message(state, "Select a chart to upload.", true);
        } else if (!can_upload_content(song->status) || !can_upload_content(chart->status)) {
            ui::notify("Only Local charts from Local songs can be uploaded.", ui::notice_tone::error, 2.8f);
        } else {
            callbacks.start_chart_upload(*song, *chart);
        }
        return;
    }
    if (result.edit_mv_requested && song != nullptr) {
        manager.change_scene(song_select::make_mv_editor_scene(manager, *song));
        return;
    }
    if (result.manage_library_requested) {
        manager.change_scene(song_select::make_legacy_song_select_scene(
            manager,
            song != nullptr ? song->song.meta.song_id : "",
            chart != nullptr ? chart->meta.chart_id : "",
            std::nullopt,
            false));
        return;
    }
}
