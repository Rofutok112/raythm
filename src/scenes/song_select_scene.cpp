#include "song_select_scene.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <utility>

#include "core/app_paths.h"
#include "core/window_dialog_support.h"
#include "network/auth_client.h"
#include "file_dialog.h"
#include "mv/mv_storage.h"
#include "path_utils.h"
#include "player_note_offsets.h"
#include "raylib.h"
#include "scene_manager.h"
#include "song_select/song_select_detail_view.h"
#include "song_select/song_select_confirmation_dialog.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_last_played.h"
#include "song_select/song_select_login_dialog.h"
#include "song_select/song_select_list_view.h"
#include "song_select/song_select_navigation.h"
#include "song_select/song_select_ranking_view.h"
#include "theme.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

std::string format_offset_label(int offset_ms) {
    return (offset_ms > 0 ? "+" : "") + std::to_string(offset_ms) + "ms";
}

std::string register_web_url() {
    return auth::normalize_server_url(auth::kDefaultServerUrl) + "/register";
}

}  // namespace

song_select_scene::song_select_scene(scene_manager& manager, std::string preferred_song_id,
                                     std::string preferred_chart_id,
                                     std::optional<song_select::recent_result_offset> recent_result_offset)
    : scene(manager),
      preferred_song_id_(std::move(preferred_song_id)),
      preferred_chart_id_(std::move(preferred_chart_id)),
      recent_result_offset_(std::move(recent_result_offset)) {
}

void song_select_scene::on_enter() {
    if (preferred_song_id_.empty() && preferred_chart_id_.empty()) {
        const song_select::last_played_selection last_played = song_select::load_last_played_selection();
        preferred_song_id_ = last_played.song_id;
        preferred_chart_id_ = last_played.chart_id;
    }
    song_select::reset_for_enter(state_);
    state_.recent_result_offset = recent_result_offset_;
    refresh_auth_state();
    if (state_.auth.logged_in) {
        start_auth_restore();
    }
    reload_song_library(preferred_song_id_, preferred_chart_id_);
}

void song_select_scene::on_exit() {
    song_select::close_context_menu(state_);
    state_.confirmation_dialog = {};
    state_.login_dialog.open = false;
    if (pending_song_import_request_.has_value()) {
        song_select::cleanup_song_import_request(*pending_song_import_request_);
        pending_song_import_request_.reset();
    }
    preview_controller_.stop();
}

void song_select_scene::reload_song_library(const std::string& preferred_song_id,
                                            const std::string& preferred_chart_id) {
    song_select::apply_catalog(state_, song_select::load_catalog(), preferred_song_id, preferred_chart_id);
    sync_selected_song_media();
}

void song_select_scene::sync_selected_song_media() {
    preview_controller_.select_song(song_select::selected_song(state_));
    reload_selected_chart_ranking();
}

void song_select_scene::reload_selected_chart_ranking() {
    const auto filtered = song_select::filtered_charts_for_selected_song(state_);
    const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered);
    const std::string chart_id = chart != nullptr ? chart->meta.chart_id : "";
    state_.ranking_panel.listing =
        ranking_service::load_chart_ranking(chart_id, state_.ranking_panel.selected_source, 50);
    state_.ranking_panel.source_dropdown_open = false;
    state_.ranking_panel.scroll_y = 0.0f;
    state_.ranking_panel.scroll_y_target = 0.0f;
    state_.ranking_panel.scrollbar_dragging = false;
    state_.ranking_panel.scrollbar_drag_offset = 0.0f;
}

void song_select_scene::refresh_auth_state() {
    const auth::session_summary summary = auth::load_session_summary();
    state_.auth.logged_in = summary.logged_in;
    state_.auth.email = summary.email;
    state_.auth.display_name = summary.display_name;
    state_.auth.email_verified = summary.email_verified;
}

void song_select_scene::apply_delete_result(const song_select::delete_result& result) {
    state_.confirmation_dialog = {};
    if (!result.success) {
        song_select::queue_status_message(state_, result.message, true);
        return;
    }

    reload_song_library(result.preferred_song_id, result.preferred_chart_id);
    song_select::queue_status_message(state_, result.message, false);
}

void song_select_scene::apply_transfer_result(const song_select::transfer_result& result) {
    if (result.cancelled) {
        return;
    }

    if (!result.success) {
        song_select::queue_status_message(state_, result.message, true);
        return;
    }

    if (result.reload_catalog) {
        reload_song_library(result.preferred_song_id, result.preferred_chart_id);
    }
    song_select::queue_status_message(state_, result.message, false);
}

void song_select_scene::start_auth_restore() {
    auth_restore_active_ = true;
    auth_restore_ = std::async(std::launch::async, []() {
        return auth::restore_saved_session();
    });
}

void song_select_scene::poll_auth_restore() {
    if (!auth_restore_active_) {
        return;
    }

    if (auth_restore_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    auth_restore_active_ = false;
    const auth::operation_result result = auth_restore_.get();
    refresh_auth_state();
    if (!result.success) {
        if (state_.login_dialog.open) {
            state_.login_dialog.status_message = result.message;
            state_.login_dialog.status_message_is_error = true;
        } else {
            song_select::queue_status_message(state_, result.message, true);
        }
    }
}

void song_select_scene::start_login_request(song_select::login_dialog_command command) {
    if (auth_request_active_) {
        return;
    }

    if (command == song_select::login_dialog_command::open_register_web) {
        const bool opened = window_dialog_support::open_url(register_web_url());
        state_.login_dialog.status_message = opened
            ? "Opened the account page in your browser."
            : "Failed to open the account page.";
        state_.login_dialog.status_message_is_error = !opened;
        return;
    }

    auth_request_active_ = true;
    state_.login_dialog.status_message_is_error = false;
    const std::string server_url = auth::kDefaultServerUrl;
    const std::string email = state_.login_dialog.email_input.value;
    const std::string password = state_.login_dialog.password_input.value;

    switch (command) {
    case song_select::login_dialog_command::request_restore:
        state_.login_dialog.status_message = "Restoring session...";
        auth_request_ = std::async(std::launch::async, []() {
            return auth::restore_saved_session();
        });
        break;
    case song_select::login_dialog_command::request_login:
        state_.login_dialog.status_message = "Connecting to raythm-Server...";
        auth_request_ = std::async(std::launch::async, [server_url, email, password]() {
            return auth::login_user(server_url, email, password);
        });
        break;
    case song_select::login_dialog_command::request_logout:
        state_.login_dialog.status_message = "Logging out...";
        auth_request_ = std::async(std::launch::async, []() {
            return auth::logout_saved_session();
        });
        break;
    case song_select::login_dialog_command::open_register_web:
    case song_select::login_dialog_command::none:
    case song_select::login_dialog_command::close:
        auth_request_active_ = false;
        break;
    }
}

void song_select_scene::poll_login_request() {
    if (!auth_request_active_) {
        return;
    }

    if (auth_request_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    auth_request_active_ = false;
    const auth::operation_result result = auth_request_.get();
    refresh_auth_state();
    state_.login_dialog.password_input.value.clear();
    state_.login_dialog.status_message = result.message;
    state_.login_dialog.status_message_is_error = !result.success;

    const auth::session_summary summary = auth::load_session_summary();
    state_.login_dialog.email_input.value = summary.email;
}

void song_select_scene::poll_song_import_prepare() {
    if (!background_song_import_prepare_active_) {
        return;
    }

    if (background_song_import_prepare_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    background_song_import_prepare_active_ = false;
    background_transfer_label_.clear();
    song_select::song_import_prepare_result prepared = background_song_import_prepare_.get();
    if (!prepared.request.has_value()) {
        apply_transfer_result(prepared.transfer);
        return;
    }

    if (prepared.request->overwrite_existing) {
        open_overwrite_song_confirmation(*prepared.request);
        return;
    }

    start_song_import(*prepared.request);
}

void song_select_scene::poll_background_transfer() {
    if (!background_transfer_active_) {
        return;
    }

    if (background_transfer_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    background_transfer_active_ = false;
    background_transfer_label_.clear();
    song_select::transfer_result result = background_transfer_.get();
    if (pending_song_import_request_.has_value()) {
        song_select::cleanup_song_import_request(*pending_song_import_request_);
        pending_song_import_request_.reset();
    }
    apply_transfer_result(result);
}

void song_select_scene::start_song_import_prepare(std::string source_path) {
    background_song_import_prepare_active_ = true;
    background_transfer_label_ = "Reading song package...";
    song_select::queue_status_message(state_, "Reading song package...", false);
    const song_select::state catalog_state = state_;
    background_song_import_prepare_ = std::async(
        std::launch::async,
        [catalog_state, source_path = std::move(source_path)]() {
            return song_select::prepare_song_import_from_path(catalog_state, source_path);
        });
}

void song_select_scene::start_song_export(song_select::song_export_request request) {
    background_transfer_active_ = true;
    background_transfer_label_ = "Exporting song package...";
    song_select::queue_status_message(state_, "Exporting song package...", false);
    background_transfer_ = std::async(std::launch::async, [request = std::move(request)]() {
        return song_select::export_song_package(request);
    });
}

void song_select_scene::start_song_import(song_select::song_import_request request) {
    background_transfer_active_ = true;
    background_transfer_label_ = "Importing song package...";
    song_select::queue_status_message(state_, "Importing song package...", false);
    pending_song_import_request_ = request;
    background_transfer_ = std::async(std::launch::async, [request = std::move(request)]() {
        return song_select::import_song_package(request);
    });
}

void song_select_scene::open_overwrite_song_confirmation(song_select::song_import_request request) {
    pending_song_import_request_ = std::move(request);
    song_select::open_confirmation_dialog(
        state_, song_select::pending_confirmation_action::overwrite_song_import,
        "Overwrite Song",
        "A user song with the same song ID already exists. Overwrite it?",
        "",
        "OVERWRITE");
}

void song_select_scene::open_overwrite_chart_confirmation(song_select::chart_import_request request) {
    pending_chart_import_request_ = std::move(request);
    song_select::open_confirmation_dialog(
        state_, song_select::pending_confirmation_action::overwrite_chart_import,
        "Overwrite Chart",
        "A user chart with the same chart ID already exists. Overwrite it?",
        "",
        "OVERWRITE");
}

bool song_select_scene::adjust_selected_song_local_offset(int delta_ms) {
    const song_select::song_entry* selected_song = song_select::selected_song(state_);
    if (selected_song == nullptr) {
        return false;
    }

    const auto filtered = song_select::filtered_charts_for_selected_song(state_);
    const song_select::chart_option* selected_chart = song_select::selected_chart_for(state_, filtered);
    if (selected_chart == nullptr) {
        return false;
    }

    auto& charts = state_.songs[static_cast<size_t>(state_.selected_song_index)].charts;
    auto chart_it = std::find_if(charts.begin(), charts.end(), [&](const song_select::chart_option& candidate) {
        return candidate.meta.chart_id == selected_chart->meta.chart_id;
    });
    if (chart_it == charts.end()) {
        return false;
    }

    auto& chart = *chart_it;
    const int next_offset = std::clamp(chart.local_note_offset_ms + delta_ms, -1000, 1000);
    if (!save_player_chart_offset(chart.meta.chart_id, next_offset)) {
        song_select::queue_status_message(state_, "Failed to save local offset.", true);
        return false;
    }

    chart.local_note_offset_ms = next_offset;
    return true;
}

bool song_select_scene::apply_recent_result_offset() {
    if (!state_.recent_result_offset.has_value()) {
        return false;
    }

    const song_select::song_entry* selected = song_select::selected_song(state_);
    if (selected == nullptr || selected->song.meta.song_id != state_.recent_result_offset->song_id) {
        return false;
    }

    const auto filtered = song_select::filtered_charts_for_selected_song(state_);
    const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered);
    if (chart == nullptr || chart->meta.chart_id != state_.recent_result_offset->chart_id) {
        return false;
    }

    const int adjustment_ms = static_cast<int>(std::lround(state_.recent_result_offset->avg_offset_ms));
    if (!adjust_selected_song_local_offset(adjustment_ms)) {
        return false;
    }

    state_.recent_result_offset.reset();
    recent_result_offset_.reset();
    return true;
}

bool song_select_scene::handle_song_list_pointer(Vector2 mouse, bool left_pressed, bool right_pressed) {
    if (state_.context_menu.open && ui::is_hovered(state_.context_menu.rect, song_select::layout::kContextMenuLayer)) {
        return false;
    }
    if (!left_pressed && !right_pressed) {
        return false;
    }

    const auto hit = song_select::hit_test_song_list(state_, mouse);
    if (!hit.has_value()) {
        if (right_pressed && CheckCollisionPointRec(mouse, song_select::layout::kSongListRect)) {
            song_select::open_list_background_context_menu(
                state_, song_select::layout::make_context_menu_rect(
                            mouse,
                            song_select::context_menu_item_count(
                                state_, song_select::context_menu_target::list_background,
                                song_select::context_menu_section::root,
                                state_.selected_song_index, state_.difficulty_index)));
            return true;
        }
        return false;
    }

    if (hit->chart_index.has_value()) {
        const int chart_index = *hit->chart_index;
        if (right_pressed) {
            song_select::apply_song_selection(state_, hit->song_index, chart_index);
            song_select::open_chart_context_menu(
                state_, hit->song_index, chart_index,
                song_select::layout::make_context_menu_rect(
                    mouse,
                    song_select::context_menu_item_count(
                        state_, song_select::context_menu_target::chart,
                        song_select::context_menu_section::root,
                        hit->song_index, chart_index)));
            return true;
        }

        song_select::close_context_menu(state_);
        if (state_.difficulty_index == chart_index) {
            const song_select::song_entry* song = song_select::selected_song(state_);
            const auto filtered = song_select::filtered_charts_for_selected_song(state_);
            const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered);
            if (song != nullptr && chart != nullptr) {
                manager_.change_scene(song_select::make_play_scene(manager_, *song, *chart));
            }
        } else {
            song_select::apply_song_selection(state_, hit->song_index, chart_index);
            reload_selected_chart_ranking();
        }
        return true;
    }

    if (right_pressed) {
        const int chart_index = hit->song_index == state_.selected_song_index ? state_.difficulty_index : 0;
        const bool song_changed = song_select::apply_song_selection(state_, hit->song_index, chart_index);
        if (song_changed) {
            sync_selected_song_media();
        }
        song_select::open_song_context_menu(
            state_, hit->song_index,
            song_select::layout::make_context_menu_rect(
                mouse,
                song_select::context_menu_item_count(
                    state_, song_select::context_menu_target::song,
                    song_select::context_menu_section::root,
                    hit->song_index, chart_index)));
        return true;
    }

    song_select::close_context_menu(state_);
    const bool song_changed = song_select::apply_song_selection(
        state_, hit->song_index, hit->song_index == state_.selected_song_index ? state_.difficulty_index : 0);
    if (song_changed) {
        sync_selected_song_media();
    } else {
        reload_selected_chart_ranking();
    }
    return true;
}

void song_select_scene::apply_context_menu_command(song_select::context_menu_command command) {
    switch (command) {
    case song_select::context_menu_command::none:
        return;
    case song_select::context_menu_command::open_song_section:
        state_.context_menu.section = song_select::context_menu_section::song;
        state_.context_menu.rect = song_select::layout::make_context_menu_rect(
            {state_.context_menu.rect.x, state_.context_menu.rect.y},
            song_select::context_menu_item_count(
                state_, state_.context_menu.target, state_.context_menu.section,
                state_.context_menu.song_index, state_.context_menu.chart_index));
        return;
    case song_select::context_menu_command::open_chart_section:
        state_.context_menu.section = song_select::context_menu_section::chart;
        state_.context_menu.rect = song_select::layout::make_context_menu_rect(
            {state_.context_menu.rect.x, state_.context_menu.rect.y},
            song_select::context_menu_item_count(
                state_, state_.context_menu.target, state_.context_menu.section,
                state_.context_menu.song_index, state_.context_menu.chart_index));
        return;
    case song_select::context_menu_command::open_mv_section:
        state_.context_menu.section = song_select::context_menu_section::mv;
        state_.context_menu.rect = song_select::layout::make_context_menu_rect(
            {state_.context_menu.rect.x, state_.context_menu.rect.y},
            song_select::context_menu_item_count(
                state_, state_.context_menu.target, state_.context_menu.section,
                state_.context_menu.song_index, state_.context_menu.chart_index));
        return;
    case song_select::context_menu_command::back_to_root:
        state_.context_menu.section = song_select::context_menu_section::root;
        state_.context_menu.rect = song_select::layout::make_context_menu_rect(
            {state_.context_menu.rect.x, state_.context_menu.rect.y},
            song_select::context_menu_item_count(
                state_, state_.context_menu.target, state_.context_menu.section,
                state_.context_menu.song_index, state_.context_menu.chart_index));
        return;
    case song_select::context_menu_command::close_menu:
        song_select::close_context_menu(state_);
        return;
    case song_select::context_menu_command::new_song:
        song_select::close_context_menu(state_);
        manager_.change_scene(song_select::make_song_create_scene(manager_));
        return;
    case song_select::context_menu_command::import_song:
        song_select::close_context_menu(state_);
        {
            const std::string source_path = file_dialog::open_song_package_file();
            if (!source_path.empty()) {
                start_song_import_prepare(source_path);
            }
        }
        return;
    case song_select::context_menu_command::edit_song:
        if (state_.context_menu.song_index >= 0 &&
            state_.context_menu.song_index < static_cast<int>(state_.songs.size())) {
            const song_select::song_entry& song = state_.songs[static_cast<size_t>(state_.context_menu.song_index)];
            song_select::close_context_menu(state_);
            manager_.change_scene(song_select::make_edit_song_scene(manager_, song));
        }
        return;
    case song_select::context_menu_command::new_chart:
        if (state_.context_menu.song_index >= 0 &&
            state_.context_menu.song_index < static_cast<int>(state_.songs.size())) {
            const song_select::song_entry& song = state_.songs[static_cast<size_t>(state_.context_menu.song_index)];
            song_select::close_context_menu(state_);
            manager_.change_scene(song_select::make_new_chart_scene(manager_, song, state_.difficulty_index));
        }
        return;
    case song_select::context_menu_command::import_chart:
    {
        song_select::close_context_menu(state_);
        song_select::transfer_result result;
        if (const auto request = song_select::prepare_chart_import(state_, result); request.has_value()) {
            if (request->overwrite_existing) {
                open_overwrite_chart_confirmation(*request);
            } else {
                apply_transfer_result(song_select::import_chart_package(*request));
            }
        } else {
            apply_transfer_result(result);
        }
        return;
    }
    case song_select::context_menu_command::export_song:
    {
        const int song_index = state_.context_menu.song_index;
        song_select::close_context_menu(state_);
        if (const auto request = song_select::prepare_song_export(state_, song_index); request.has_value()) {
            start_song_export(*request);
        }
        return;
    }
    case song_select::context_menu_command::request_delete_song:
        song_select::open_confirmation_dialog(
            state_, song_select::pending_confirmation_action::delete_song,
            "", "", "", "DELETE", state_.context_menu.song_index, -1);
        song_select::close_context_menu(state_);
        return;
    case song_select::context_menu_command::edit_chart:
        if (state_.context_menu.song_index >= 0 &&
            state_.context_menu.song_index < static_cast<int>(state_.songs.size())) {
            const auto& song = state_.songs[static_cast<size_t>(state_.context_menu.song_index)];
            if (state_.context_menu.chart_index >= 0 &&
                state_.context_menu.chart_index < static_cast<int>(song.charts.size())) {
                const auto& chart = song.charts[static_cast<size_t>(state_.context_menu.chart_index)];
                song_select::close_context_menu(state_);
                manager_.change_scene(song_select::make_edit_chart_scene(manager_, song, chart));
            }
        }
        return;
    case song_select::context_menu_command::export_chart:
    {
        const int song_index = state_.context_menu.song_index;
        const int chart_index = state_.context_menu.chart_index;
        song_select::close_context_menu(state_);
        apply_transfer_result(song_select::export_chart_package(
            state_, song_index, chart_index));
        return;
    }
    case song_select::context_menu_command::request_delete_chart:
        song_select::open_confirmation_dialog(
            state_, song_select::pending_confirmation_action::delete_chart,
            "", "", "", "DELETE",
            state_.context_menu.song_index, state_.context_menu.chart_index);
        song_select::close_context_menu(state_);
        return;
    case song_select::context_menu_command::new_mv:
    case song_select::context_menu_command::edit_mv:
        if (state_.context_menu.song_index >= 0 &&
            state_.context_menu.song_index < static_cast<int>(state_.songs.size())) {
            const auto& song = state_.songs[static_cast<size_t>(state_.context_menu.song_index)];
            song_select::close_context_menu(state_);
            manager_.change_scene(song_select::make_mv_editor_scene(manager_, song));
        }
        return;
    case song_select::context_menu_command::delete_mv:
        song_select::open_confirmation_dialog(
            state_, song_select::pending_confirmation_action::delete_mv,
            "Delete MV Script",
            "Are you sure you want to delete this MV script?",
            "This action cannot be undone.",
            "DELETE",
            state_.context_menu.song_index);
        song_select::close_context_menu(state_);
        return;
    case song_select::context_menu_command::import_mv:
        if (state_.context_menu.song_index >= 0 &&
            state_.context_menu.song_index < static_cast<int>(state_.songs.size())) {
            const auto& song = state_.songs[static_cast<size_t>(state_.context_menu.song_index)];
            song_select::close_context_menu(state_);
            const std::string src = file_dialog::open_mv_script_file();
            if (!src.empty()) {
                mv::mv_package package = mv::find_first_package_for_song(song.song.meta.song_id)
                    .value_or(mv::make_default_package_for_song(song.song.meta));
                package.meta.song_id = song.song.meta.song_id;
                package.meta.script_file = "script.rmv";
                package.directory = path_utils::to_utf8(app_paths::mv_dir(package.meta.mv_id));
                std::error_code ec;
                if (!mv::write_mv_json(package.meta, package.directory)) {
                    song_select::queue_status_message(state_, "Failed to prepare MV package.", true);
                } else {
                    std::filesystem::copy_file(src, mv::script_path(package),
                                               std::filesystem::copy_options::overwrite_existing, ec);
                    if (ec) {
                        song_select::queue_status_message(state_, "Failed to import MV script.", true);
                    } else {
                        song_select::queue_status_message(state_, "MV script imported.", false);
                    }
                }
            }
        }
        return;
    case song_select::context_menu_command::export_mv:
        if (state_.context_menu.song_index >= 0 &&
            state_.context_menu.song_index < static_cast<int>(state_.songs.size())) {
            const auto& song = state_.songs[static_cast<size_t>(state_.context_menu.song_index)];
            song_select::close_context_menu(state_);
            if (const auto package = mv::find_first_package_for_song(song.song.meta.song_id); package.has_value()) {
                const auto src = mv::script_path(*package);
                if (std::filesystem::exists(src)) {
                    const std::string dest = file_dialog::save_mv_script_file(package->meta.mv_id + ".rmv");
                    if (!dest.empty()) {
                        std::error_code ec;
                        std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
                        if (ec) {
                            song_select::queue_status_message(state_, "Failed to export MV script.", true);
                        } else {
                            song_select::queue_status_message(state_, "MV script exported.", false);
                        }
                    }
                } else {
                    song_select::queue_status_message(state_, "Failed to export MV script.", true);
                }
            }
        }
        return;
    }
}

void song_select_scene::apply_confirmation_command(song_select::confirmation_command command) {
    switch (command) {
    case song_select::confirmation_command::none:
        return;
    case song_select::confirmation_command::cancel:
        if (pending_song_import_request_.has_value()) {
            song_select::cleanup_song_import_request(*pending_song_import_request_);
            pending_song_import_request_.reset();
        }
        pending_chart_import_request_.reset();
        state_.confirmation_dialog = {};
        return;
    case song_select::confirmation_command::confirm:
        if (state_.confirmation_dialog.action == song_select::pending_confirmation_action::delete_mv) {
            const int si = state_.confirmation_dialog.song_index;
            state_.confirmation_dialog = {};
            if (si >= 0 && si < static_cast<int>(state_.songs.size())) {
                const auto& song_id = state_.songs[static_cast<size_t>(si)].song.meta.song_id;
                if (const auto package = mv::find_first_package_for_song(song_id); package.has_value()) {
                    std::error_code ec;
                    std::filesystem::remove_all(path_utils::from_utf8(package->directory), ec);
                    if (ec) {
                        song_select::queue_status_message(state_, "Failed to delete MV script.", true);
                    } else {
                        song_select::queue_status_message(state_, "MV script deleted.", false);
                    }
                } else {
                    song_select::queue_status_message(state_, "MV script not found.", true);
                }
            }
            return;
        }
        if (state_.confirmation_dialog.action == song_select::pending_confirmation_action::delete_song) {
            preview_controller_.stop();
            apply_delete_result(song_select::delete_song(state_, state_.confirmation_dialog.song_index));
        } else if (state_.confirmation_dialog.action == song_select::pending_confirmation_action::delete_chart) {
            apply_delete_result(song_select::delete_chart(state_, state_.confirmation_dialog.song_index,
                                                          state_.confirmation_dialog.chart_index));
        } else if (state_.confirmation_dialog.action == song_select::pending_confirmation_action::overwrite_song_import) {
            state_.confirmation_dialog = {};
            if (pending_song_import_request_.has_value()) {
                song_select::song_import_request request = *pending_song_import_request_;
                start_song_import(request);
            }
        } else if (state_.confirmation_dialog.action == song_select::pending_confirmation_action::overwrite_chart_import) {
            state_.confirmation_dialog = {};
            if (pending_chart_import_request_.has_value()) {
                apply_transfer_result(song_select::import_chart_package(*pending_chart_import_request_));
                pending_chart_import_request_.reset();
            }
        }
        return;
    }
}

void song_select_scene::update(float dt) {
    ui::begin_hit_regions();
    const bool modal_ui_active = state_.confirmation_dialog.open || state_.login_dialog.open ||
                                 background_song_import_prepare_active_ || background_transfer_active_;
    if (modal_ui_active) {
        state_.ranking_panel.source_dropdown_open = false;
    }
    if (state_.context_menu.open) {
        ui::register_hit_region(state_.context_menu.rect, song_select::layout::kContextMenuLayer);
    }
    if (state_.ranking_panel.source_dropdown_open && !modal_ui_active) {
        ui::register_hit_region(song_select::layout::ranking_source_dropdown_menu_rect(), ui::draw_layer::overlay);
    }
    if (state_.confirmation_dialog.open) {
        ui::register_hit_region(song_select::layout::kConfirmDialogRect, song_select::layout::kModalLayer);
    }
    if (state_.login_dialog.open) {
        ui::register_hit_region(song_select::login_dialog_rect(state_), song_select::layout::kModalLayer);
    }

    preview_controller_.update(dt, song_select::selected_song(state_));
    song_select::tick_animations(state_, dt);
    poll_auth_restore();
    poll_login_request();
    poll_song_import_prepare();
    poll_background_transfer();

    if (state_.confirmation_dialog.open &&
        !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        state_.confirmation_dialog.suppress_initial_pointer_cancel = false;
    }

    if (background_song_import_prepare_active_ || background_transfer_active_) {
        return;
    }

    if (state_.login_dialog.open) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        if (!auth_request_active_ &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            !CheckCollisionPointRec(mouse, song_select::login_dialog_rect(state_))) {
            state_.login_dialog.open = false;
            return;
        }
        if (IsKeyPressed(KEY_ESCAPE) && !auth_request_active_) {
            state_.login_dialog.open = false;
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (state_.confirmation_dialog.open) {
            state_.confirmation_dialog = {};
            return;
        }
        if (state_.context_menu.open) {
            song_select::close_context_menu(state_);
            return;
        }
        manager_.change_scene(song_select::make_title_scene(manager_));
        return;
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const float wheel = GetMouseWheelMove();

    if (state_.confirmation_dialog.open) {
        return;
    }

    if (state_.context_menu.open &&
        (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) &&
        !ui::is_hovered(state_.context_menu.rect, song_select::layout::kContextMenuLayer) &&
        !CheckCollisionPointRec(mouse, song_select::layout::kSongListRect)) {
        song_select::close_context_menu(state_);
        return;
    }

    if (state_.ranking_panel.source_dropdown_open &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(mouse, song_select::layout::kRankingSourceDropdownRect) &&
        !CheckCollisionPointRec(mouse, song_select::layout::ranking_source_dropdown_menu_rect())) {
        state_.ranking_panel.source_dropdown_open = false;
        return;
    }

    if (IsKeyPressed(KEY_F1) || ui::is_clicked(song_select::layout::kSettingsButtonRect, song_select::layout::kSceneLayer)) {
        song_select::close_context_menu(state_);
        manager_.change_scene(song_select::make_settings_scene(manager_));
        return;
    }

    if (ui::is_clicked(song_select::layout::kLoginButtonRect, song_select::layout::kSceneLayer)) {
        song_select::close_context_menu(state_);
        song_select::open_login_dialog(state_, auth::load_session_summary());
        return;
    }

    if (ui::is_clicked(song_select::layout::local_offset_double_left_rect(), song_select::layout::kSceneLayer)) {
        adjust_selected_song_local_offset(-5);
        return;
    }

    if (ui::is_clicked(song_select::layout::local_offset_left_rect(), song_select::layout::kSceneLayer)) {
        adjust_selected_song_local_offset(-1);
        return;
    }

    if (ui::is_clicked(song_select::layout::local_offset_right_rect(), song_select::layout::kSceneLayer)) {
        adjust_selected_song_local_offset(1);
        return;
    }

    if (ui::is_clicked(song_select::layout::local_offset_double_right_rect(), song_select::layout::kSceneLayer)) {
        adjust_selected_song_local_offset(5);
        return;
    }

    if (ui::is_clicked(song_select::layout::auto_apply_button_rect(), song_select::layout::kSceneLayer) &&
        apply_recent_result_offset()) {
        return;
    }

    const bool song_list_hovered = ui::is_hovered(song_select::layout::kSongListViewRect, song_select::layout::kSceneLayer);
    const bool ranking_list_hovered = ui::is_hovered(song_select::layout::kRankingListRect, song_select::layout::kSceneLayer);

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        song_select::close_context_menu(state_);
        if (song_select::apply_song_selection(state_, std::max(0, state_.selected_song_index - 1), 0)) {
            sync_selected_song_media();
        }
    } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        song_select::close_context_menu(state_);
        if (song_select::apply_song_selection(state_, std::min(static_cast<int>(state_.songs.size()) - 1,
                                                               state_.selected_song_index + 1), 0)) {
            sync_selected_song_media();
        }
    }

    const ui::scrollbar_interaction scrollbar = ui::update_vertical_scrollbar(
        song_select::layout::kSongListScrollbarTrackRect,
        song_select::content_height(state_),
        state_.scroll_y_target,
        state_.scrollbar_dragging,
        state_.scrollbar_drag_offset,
        song_select::layout::kSceneLayer);
    state_.scroll_y_target = scrollbar.scroll_offset;
    if (scrollbar.changed || scrollbar.dragging) {
        state_.scroll_y = state_.scroll_y_target;
    }

    if (!scrollbar.dragging && song_list_hovered && wheel != 0.0f) {
        state_.scroll_y_target -= wheel * song_select::layout::kScrollWheelStep;
    }

    const float max_scroll = ui::vertical_scroll_metrics(song_select::layout::kSongListScrollbarTrackRect,
                                                         song_select::content_height(state_),
                                                         state_.scroll_y_target).max_scroll;
    state_.scroll_y_target = std::clamp(state_.scroll_y_target, 0.0f, max_scroll);
    state_.scroll_y += (state_.scroll_y_target - state_.scroll_y) *
                       std::min(1.0f, song_select::layout::kScrollLerpSpeed * dt);
    if (std::fabs(state_.scroll_y - state_.scroll_y_target) < 0.5f) {
        state_.scroll_y = state_.scroll_y_target;
    }

    const ui::scrollbar_interaction ranking_scrollbar = ui::update_vertical_scrollbar(
        song_select::layout::kRankingScrollbarTrackRect,
        song_select::ranking_content_height(state_),
        state_.ranking_panel.scroll_y_target,
        state_.ranking_panel.scrollbar_dragging,
        state_.ranking_panel.scrollbar_drag_offset,
        song_select::layout::kSceneLayer);
    state_.ranking_panel.scroll_y_target = ranking_scrollbar.scroll_offset;
    if (ranking_scrollbar.changed || ranking_scrollbar.dragging) {
        state_.ranking_panel.scroll_y = state_.ranking_panel.scroll_y_target;
    }

    if (!ranking_scrollbar.dragging && ranking_list_hovered && wheel != 0.0f) {
        state_.ranking_panel.scroll_y_target -= wheel * song_select::layout::kScrollWheelStep;
    }

    const float max_ranking_scroll = ui::vertical_scroll_metrics(song_select::layout::kRankingScrollbarTrackRect,
                                                                 song_select::ranking_content_height(state_),
                                                                 state_.ranking_panel.scroll_y_target).max_scroll;
    state_.ranking_panel.scroll_y_target = std::clamp(state_.ranking_panel.scroll_y_target, 0.0f, max_ranking_scroll);
    state_.ranking_panel.scroll_y += (state_.ranking_panel.scroll_y_target - state_.ranking_panel.scroll_y) *
                                     std::min(1.0f, song_select::layout::kScrollLerpSpeed * dt);
    if (std::fabs(state_.ranking_panel.scroll_y - state_.ranking_panel.scroll_y_target) < 0.5f) {
        state_.ranking_panel.scroll_y = state_.ranking_panel.scroll_y_target;
    }

    const auto filtered = song_select::filtered_charts_for_selected_song(state_);
    if (!filtered.empty()) {
        state_.difficulty_index = std::clamp(state_.difficulty_index, 0, static_cast<int>(filtered.size()) - 1);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            song_select::close_context_menu(state_);
            state_.difficulty_index = std::max(0, state_.difficulty_index - 1);
            state_.chart_change_anim_t = 1.0f;
            reload_selected_chart_ranking();
        } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            song_select::close_context_menu(state_);
            state_.difficulty_index = std::min(static_cast<int>(filtered.size()) - 1, state_.difficulty_index + 1);
            state_.chart_change_anim_t = 1.0f;
            reload_selected_chart_ranking();
        }

        const song_select::song_entry* song = song_select::selected_song(state_);
        const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered);
        if (song != nullptr && chart != nullptr) {
            if (IsKeyPressed(KEY_ENTER)) {
                song_select::close_context_menu(state_);
                manager_.change_scene(song_select::make_play_scene(manager_, *song, *chart));
                return;
            }

            if (IsKeyPressed(KEY_E)) {
                song_select::close_context_menu(state_);
                manager_.change_scene(song_select::make_edit_chart_scene(manager_, *song, *chart));
                return;
            }

            if (IsKeyPressed(KEY_N)) {
                song_select::close_context_menu(state_);
                manager_.change_scene(song_select::make_new_chart_scene(manager_, *song, state_.difficulty_index));
                return;
            }
        }
    }

    if (handle_song_list_pointer(mouse, IsMouseButtonPressed(MOUSE_BUTTON_LEFT), IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        return;
    }

    if (state_.songs.empty()) {
        return;
    }
}

void song_select_scene::draw() {
    const auto& theme = *g_theme;
    virtual_screen::begin();
    ClearBackground(theme.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, theme.bg, theme.bg_alt);
    ui::begin_draw_queue();
    song_select::draw_frame(state_);
    song_select::draw_song_list(state_);

    if (state_.songs.empty()) {
        song_select::draw_empty_state(state_);
        song_select::draw_status_message(state_);
        if (background_song_import_prepare_active_ || background_transfer_active_) {
            song_select::draw_busy_overlay(background_transfer_label_);
        }
        apply_context_menu_command(song_select::draw_context_menu(state_));
        const song_select::login_dialog_command login_command =
            song_select::draw_login_dialog(state_, auth_request_active_);
        if (login_command == song_select::login_dialog_command::close) {
            state_.login_dialog.open = false;
        } else if (login_command != song_select::login_dialog_command::none) {
            start_login_request(login_command);
        }
        ui::flush_draw_queue();
        virtual_screen::end();
        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    song_select::draw_song_details(state_, preview_controller_);
    const bool ranking_source_dropdown_interactive =
        !state_.confirmation_dialog.open &&
        !state_.login_dialog.open &&
        !background_song_import_prepare_active_ &&
        !background_transfer_active_;
    const song_select::ranking_panel_result ranking_result =
        song_select::draw_ranking_panel(state_, ranking_source_dropdown_interactive);
    song_select::draw_status_message(state_);
    if (background_song_import_prepare_active_ || background_transfer_active_) {
        song_select::draw_busy_overlay(background_transfer_label_);
    }
    apply_context_menu_command(song_select::draw_context_menu(state_));
    apply_confirmation_command(song_select::draw_confirmation_dialog(state_));
    const song_select::login_dialog_command login_command =
        song_select::draw_login_dialog(state_, auth_request_active_);
    if (login_command == song_select::login_dialog_command::close) {
        state_.login_dialog.open = false;
    } else if (login_command != song_select::login_dialog_command::none) {
        start_login_request(login_command);
    }
    if (ranking_result.source_dropdown_toggled) {
        state_.ranking_panel.source_dropdown_open = !state_.ranking_panel.source_dropdown_open;
    }
    if (ranking_result.source_clicked_index >= 0) {
        const ranking_service::source next_source =
            ranking_result.source_clicked_index == 0 ? ranking_service::source::local : ranking_service::source::online;
        state_.ranking_panel.source_dropdown_open = false;
        if (state_.ranking_panel.selected_source != next_source) {
            state_.ranking_panel.selected_source = next_source;
            reload_selected_chart_ranking();
        }
    } else if (ranking_result.source_dropdown_close_requested) {
        state_.ranking_panel.source_dropdown_open = false;
    }
    ui::flush_draw_queue();

    state_.scene_fade_in.draw();

    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
