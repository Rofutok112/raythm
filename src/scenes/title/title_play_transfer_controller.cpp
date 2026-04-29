#include "title/title_play_transfer_controller.h"

#include <utility>
#include <vector>

#include "core/file_dialog.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_import_export_service.h"
#include "song_select/song_select_command_controller.h"
#include "song_select/song_select_confirmation_dialog.h"
#include "song_select/song_select_detail_view.h"

void title_play_transfer_controller::on_exit() {
    transfer_controller_.on_exit();
}

bool title_play_transfer_controller::busy() const {
    return transfer_controller_.busy();
}

const std::string& title_play_transfer_controller::busy_label() const {
    return transfer_controller_.busy_label();
}

void title_play_transfer_controller::cancel_confirmation(song_select::state& state) {
    transfer_controller_.clear_pending_song_import_request(true);
    transfer_controller_.clear_pending_chart_import_request();
    state.confirmation_dialog = {};
}

void title_play_transfer_controller::poll(song_select::state& state,
                                          const catalog_callbacks& callbacks,
                                          bool sync_media_on_reload) {
    if (const auto prepared = transfer_controller_.poll_song_import_prepare(); prepared.has_value()) {
        if (prepared->requests.empty()) {
            apply_transfer_result(state, prepared->transfer, callbacks, sync_media_on_reload);
        } else if (prepared->overwrite_count > 0) {
            open_overwrite_song_confirmation(state, prepared->requests);
        } else {
            transfer_controller_.start_song_imports(prepared->requests);
            song_select::queue_status_message(state, transfer_controller_.busy_label(), false);
        }
    }
    if (const auto result = transfer_controller_.poll_background_transfer(); result.has_value()) {
        apply_transfer_result(state, *result, callbacks, sync_media_on_reload);
    }
}

void title_play_transfer_controller::start_song_import(song_select::state& state) {
    const std::vector<std::string> source_paths = file_dialog::open_song_package_files();
    if (source_paths.empty()) {
        return;
    }
    transfer_controller_.start_song_import_prepare(state, source_paths);
    song_select::queue_status_message(state, transfer_controller_.busy_label(), false);
}

void title_play_transfer_controller::start_chart_import(song_select::state& state,
                                                       const catalog_callbacks& callbacks,
                                                       bool sync_media_on_reload) {
    song_select::transfer_result result;
    if (const auto batch = song_select::prepare_chart_imports(state, result); batch.has_value()) {
        if (batch->overwrite_count > 0) {
            open_overwrite_chart_confirmation(state, batch->requests);
        } else {
            apply_transfer_result(state, song_select::import_chart_packages(batch->requests),
                                  callbacks, sync_media_on_reload);
        }
    } else {
        apply_transfer_result(state, result, callbacks, sync_media_on_reload);
    }
}

void title_play_transfer_controller::start_song_export(song_select::state& state) {
    const int song_index = state.selected_song_index;
    if (const auto request = song_select::prepare_song_export(state, song_index); request.has_value()) {
        transfer_controller_.start_song_export(*request);
        song_select::queue_status_message(state, transfer_controller_.busy_label(), false);
    }
}

void title_play_transfer_controller::start_chart_export(song_select::state& state,
                                                       const catalog_callbacks& callbacks,
                                                       bool sync_media_on_reload) {
    apply_transfer_result(
        state,
        song_select::export_chart_package(state, state.selected_song_index, state.difficulty_index),
        callbacks,
        sync_media_on_reload);
}

void title_play_transfer_controller::draw_or_apply_confirmation(
    song_select::state& state,
    song_select::preview_controller& preview_controller,
    const catalog_callbacks& callbacks,
    bool sync_media_on_reload) {
    if (transfer_controller_.busy()) {
        song_select::draw_busy_overlay(transfer_controller_.busy_label());
        return;
    }

    const song_select::confirmation_command command = song_select::draw_confirmation_dialog(state);
    song_select::commands::apply_confirmation_command(
        state,
        preview_controller,
        transfer_controller_,
        command,
        [this, &state, &callbacks, sync_media_on_reload](const song_select::delete_result& result) {
            apply_delete_result(state, result, callbacks, sync_media_on_reload);
        },
        [this, &state, &callbacks, sync_media_on_reload](const song_select::transfer_result& result) {
            apply_transfer_result(state, result, callbacks, sync_media_on_reload);
        });
}

void title_play_transfer_controller::apply_delete_result(song_select::state& state,
                                                         const song_select::delete_result& result,
                                                         const catalog_callbacks& callbacks,
                                                         bool sync_media_on_reload) {
    state.confirmation_dialog = {};
    if (!result.success) {
        song_select::queue_status_message(state, result.message, true);
        return;
    }

    const song_select::song_entry* selected_song = song_select::selected_song(state);
    const std::string deleted_song_id = selected_song != nullptr ? selected_song->song.meta.song_id : "";
    callbacks.set_preferred_selection(result.preferred_song_id, result.preferred_chart_id);
    callbacks.stop_preview();
    callbacks.mark_online_song_removed(deleted_song_id);
    callbacks.reload_online_catalog();
    callbacks.request_play_catalog_reload(result.preferred_song_id, result.preferred_chart_id, sync_media_on_reload);
    song_select::queue_status_message(state, result.message, false);
}

void title_play_transfer_controller::apply_transfer_result(song_select::state& state,
                                                           const song_select::transfer_result& result,
                                                           const catalog_callbacks& callbacks,
                                                           bool sync_media_on_reload) {
    if (result.cancelled) {
        return;
    }
    if (!result.success) {
        song_select::queue_status_message(state, result.message, true);
        return;
    }

    if (result.reload_catalog) {
        callbacks.set_preferred_selection(result.preferred_song_id, result.preferred_chart_id);
        callbacks.reload_online_catalog();
        callbacks.request_play_catalog_reload(result.preferred_song_id, result.preferred_chart_id,
                                              sync_media_on_reload);
    }
    song_select::queue_status_message(state, result.message, false);
}

void title_play_transfer_controller::open_overwrite_song_confirmation(
    song_select::state& state,
    std::vector<song_select::song_import_request> requests) {
    const size_t overwrite_count = requests.size();
    transfer_controller_.set_pending_song_import_requests(std::move(requests));
    song_select::open_confirmation_dialog(
        state, song_select::pending_confirmation_action::overwrite_song_import,
        overwrite_count <= 1 ? "Overwrite Song" : "Overwrite Songs",
        overwrite_count <= 1 ? "A user song with the same song ID already exists. Overwrite it?"
                             : "Some selected songs already exist. Overwrite them?",
        "",
        "OVERWRITE");
}

void title_play_transfer_controller::open_overwrite_chart_confirmation(
    song_select::state& state,
    std::vector<song_select::chart_import_request> requests) {
    const size_t overwrite_count = requests.size();
    transfer_controller_.set_pending_chart_import_requests(std::move(requests));
    song_select::open_confirmation_dialog(
        state, song_select::pending_confirmation_action::overwrite_chart_import,
        overwrite_count <= 1 ? "Overwrite Chart" : "Overwrite Charts",
        overwrite_count <= 1 ? "A user chart with the same chart ID already exists. Overwrite it?"
                             : "Some selected charts already exist. Overwrite them?",
        "",
        "OVERWRITE");
}
