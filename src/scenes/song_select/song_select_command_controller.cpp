#include "song_select_command_controller.h"

#include <filesystem>
#include <system_error>
#include <vector>

#include "core/file_dialog.h"
#include "core/path_utils.h"
#include "mv/mv_storage.h"
#include "song_select/song_import_export_service.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_navigation.h"
#include "song_select/song_select_overlay_view.h"

namespace song_select::commands {

void apply_context_menu_command(scene_manager& manager, state& state,
                                transfer::controller& transfer_controller,
                                context_menu_command command,
                                const apply_transfer_result_fn& apply_transfer_result,
                                const reload_song_library_fn& reload_song_library,
                                const open_overwrite_song_confirmation_fn& open_overwrite_song_confirmation,
                                const open_overwrite_chart_confirmation_fn& open_overwrite_chart_confirmation) {
    switch (command) {
    case context_menu_command::none:
        return;
    case context_menu_command::open_song_section:
        state.context_menu.section = context_menu_section::song;
        state.context_menu.rect = layout::make_context_menu_rect(
            {state.context_menu.rect.x, state.context_menu.rect.y},
            context_menu_item_count(
                state, state.context_menu.target, state.context_menu.section,
                state.context_menu.song_index, state.context_menu.chart_index));
        return;
    case context_menu_command::open_chart_section:
        state.context_menu.section = context_menu_section::chart;
        state.context_menu.rect = layout::make_context_menu_rect(
            {state.context_menu.rect.x, state.context_menu.rect.y},
            context_menu_item_count(
                state, state.context_menu.target, state.context_menu.section,
                state.context_menu.song_index, state.context_menu.chart_index));
        return;
    case context_menu_command::open_mv_section:
        state.context_menu.section = context_menu_section::mv;
        state.context_menu.rect = layout::make_context_menu_rect(
            {state.context_menu.rect.x, state.context_menu.rect.y},
            context_menu_item_count(
                state, state.context_menu.target, state.context_menu.section,
                state.context_menu.song_index, state.context_menu.chart_index));
        return;
    case context_menu_command::back_to_root:
        state.context_menu.section = context_menu_section::root;
        state.context_menu.rect = layout::make_context_menu_rect(
            {state.context_menu.rect.x, state.context_menu.rect.y},
            context_menu_item_count(
                state, state.context_menu.target, state.context_menu.section,
                state.context_menu.song_index, state.context_menu.chart_index));
        return;
    case context_menu_command::close_menu:
        close_context_menu(state);
        return;
    case context_menu_command::new_song:
        close_context_menu(state);
        manager.change_scene(make_song_create_scene(manager));
        return;
    case context_menu_command::import_song:
        close_context_menu(state);
        {
            const std::vector<std::string> source_paths = file_dialog::open_song_package_files();
            if (!source_paths.empty()) {
                transfer_controller.start_song_import_prepare(state, source_paths);
                queue_status_message(state, transfer_controller.busy_label(), false);
            }
        }
        return;
    case context_menu_command::edit_song:
        if (state.context_menu.song_index >= 0 &&
            state.context_menu.song_index < static_cast<int>(state.songs.size())) {
            const song_entry& song = state.songs[static_cast<size_t>(state.context_menu.song_index)];
            close_context_menu(state);
            manager.change_scene(make_edit_song_scene(manager, song));
        }
        return;
    case context_menu_command::new_chart:
        if (state.context_menu.song_index >= 0 &&
            state.context_menu.song_index < static_cast<int>(state.songs.size())) {
            const song_entry& song = state.songs[static_cast<size_t>(state.context_menu.song_index)];
            close_context_menu(state);
            manager.change_scene(make_new_chart_scene(manager, song, state.difficulty_index));
        }
        return;
    case context_menu_command::import_chart:
    {
        const int target_song_index = state.context_menu.song_index >= 0
            ? state.context_menu.song_index
            : state.selected_song_index;
        close_context_menu(state);
        transfer_result result;
        if (const auto batch = prepare_chart_imports(state, target_song_index, result); batch.has_value()) {
            if (batch->overwrite_count > 0) {
                open_overwrite_chart_confirmation(batch->requests);
            } else {
                apply_transfer_result(import_chart_packages(batch->requests));
            }
        } else {
            apply_transfer_result(result);
        }
        return;
    }
    case context_menu_command::export_song:
    {
        const int song_index = state.context_menu.song_index;
        close_context_menu(state);
        if (const auto request = prepare_song_export(state, song_index); request.has_value()) {
            transfer_controller.start_song_export(*request);
            queue_status_message(state, transfer_controller.busy_label(), false);
        }
        return;
    }
    case context_menu_command::request_delete_song:
        open_confirmation_dialog(
            state, pending_confirmation_action::delete_song,
            "", "", "", "DELETE", state.context_menu.song_index, -1);
        close_context_menu(state);
        return;
    case context_menu_command::edit_chart:
        if (state.context_menu.song_index >= 0 &&
            state.context_menu.song_index < static_cast<int>(state.songs.size())) {
            const auto& song = state.songs[static_cast<size_t>(state.context_menu.song_index)];
            if (state.context_menu.chart_index >= 0 &&
                state.context_menu.chart_index < static_cast<int>(song.charts.size())) {
                const auto& chart = song.charts[static_cast<size_t>(state.context_menu.chart_index)];
                close_context_menu(state);
                manager.change_scene(make_edit_chart_scene(manager, song, chart));
            }
        }
        return;
    case context_menu_command::export_chart:
    {
        const int song_index = state.context_menu.song_index;
        const int chart_index = state.context_menu.chart_index;
        close_context_menu(state);
        apply_transfer_result(export_chart_package(state, song_index, chart_index));
        return;
    }
    case context_menu_command::request_delete_chart:
        open_confirmation_dialog(
            state, pending_confirmation_action::delete_chart,
            "", "", "", "DELETE",
            state.context_menu.song_index, state.context_menu.chart_index);
        close_context_menu(state);
        return;
    case context_menu_command::new_mv:
    case context_menu_command::edit_mv:
        if (state.context_menu.song_index >= 0 &&
            state.context_menu.song_index < static_cast<int>(state.songs.size())) {
            const auto& song = state.songs[static_cast<size_t>(state.context_menu.song_index)];
            close_context_menu(state);
            manager.change_scene(make_mv_editor_scene(manager, song));
        }
        return;
    case context_menu_command::delete_mv:
        open_confirmation_dialog(
            state, pending_confirmation_action::delete_mv,
            "Delete MV Script",
            "Are you sure you want to delete this MV script?",
            "This action cannot be undone.",
            "DELETE",
            state.context_menu.song_index);
        close_context_menu(state);
        return;
    case context_menu_command::import_mv:
        if (state.context_menu.song_index >= 0 &&
            state.context_menu.song_index < static_cast<int>(state.songs.size())) {
            const auto& song = state.songs[static_cast<size_t>(state.context_menu.song_index)];
            close_context_menu(state);
            const std::string src = file_dialog::open_mv_script_file();
            if (!src.empty()) {
                mv::mv_package package = mv::find_first_package_for_song(song.song.meta.song_id)
                    .value_or(mv::make_default_package_for_song(song.song.meta));
                package.meta.song_id = song.song.meta.song_id;
                package.meta.script_file = "script.rmv";
                if (!mv::write_mv_json(package.meta, package.directory)) {
                    queue_status_message(state, "Failed to prepare MV package.", true);
                } else if (!mv::import_script(package, src)) {
                    queue_status_message(state, "Failed to import MV script.", true);
                } else {
                    reload_song_library(song.song.meta.song_id, "");
                    queue_status_message(state, "MV script imported.", false);
                }
            }
        }
        return;
    case context_menu_command::export_mv:
        if (state.context_menu.song_index >= 0 &&
            state.context_menu.song_index < static_cast<int>(state.songs.size())) {
            const auto& song = state.songs[static_cast<size_t>(state.context_menu.song_index)];
            close_context_menu(state);
            if (const auto package = mv::find_first_package_for_song(song.song.meta.song_id); package.has_value()) {
                const std::string dest = file_dialog::save_mv_script_file(package->meta.mv_id + ".rmv");
                if (!dest.empty()) {
                    if (!mv::export_script(*package, dest)) {
                        queue_status_message(state, "Failed to export MV script.", true);
                    } else {
                        queue_status_message(state, "MV script exported.", false);
                    }
                }
            }
        }
        return;
    }
}

void apply_confirmation_command(state& state,
                                preview_controller& preview_controller,
                                transfer::controller& transfer_controller,
                                confirmation_command command,
                                const apply_delete_result_fn& apply_delete_result,
                                const apply_transfer_result_fn& apply_transfer_result) {
    switch (command) {
    case confirmation_command::none:
        return;
    case confirmation_command::cancel:
        transfer_controller.clear_pending_song_import_request(true);
        transfer_controller.clear_pending_chart_import_request();
        state.confirmation_dialog = {};
        return;
    case confirmation_command::confirm:
        if (state.confirmation_dialog.action == pending_confirmation_action::delete_mv) {
            const int si = state.confirmation_dialog.song_index;
            state.confirmation_dialog = {};
            if (si >= 0 && si < static_cast<int>(state.songs.size())) {
                const auto& song_id = state.songs[static_cast<size_t>(si)].song.meta.song_id;
                if (const auto package = mv::find_first_package_for_song(song_id); package.has_value()) {
                    std::error_code ec;
                    std::filesystem::remove_all(path_utils::from_utf8(package->directory), ec);
                    if (ec) {
                        queue_status_message(state, "Failed to delete MV script.", true);
                    } else {
                        queue_status_message(state, "MV script deleted.", false);
                    }
                } else {
                    queue_status_message(state, "MV script not found.", true);
                }
            }
            return;
        }
        if (state.confirmation_dialog.action == pending_confirmation_action::delete_song) {
            preview_controller.stop();
            apply_delete_result(delete_song(state, state.confirmation_dialog.song_index));
        } else if (state.confirmation_dialog.action == pending_confirmation_action::delete_chart) {
            apply_delete_result(delete_chart(state, state.confirmation_dialog.song_index,
                                             state.confirmation_dialog.chart_index));
        } else if (state.confirmation_dialog.action == pending_confirmation_action::overwrite_song_import) {
            state.confirmation_dialog = {};
            if (!transfer_controller.pending_song_import_requests().empty()) {
                std::vector<song_import_request> requests = transfer_controller.pending_song_import_requests();
                transfer_controller.start_song_imports(std::move(requests));
                queue_status_message(state, transfer_controller.busy_label(), false);
            }
        } else if (state.confirmation_dialog.action == pending_confirmation_action::overwrite_chart_import) {
            state.confirmation_dialog = {};
            if (!transfer_controller.pending_chart_import_requests().empty()) {
                apply_transfer_result(import_chart_packages(transfer_controller.pending_chart_import_requests()));
                transfer_controller.clear_pending_chart_import_request();
            }
        }
        return;
    }
}

}  // namespace song_select::commands
