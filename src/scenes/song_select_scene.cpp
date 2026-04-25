#include "song_select_scene.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <future>
#include <thread>
#include <utility>

#include "core/app_paths.h"
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
#include "tween.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

std::string format_offset_label(int offset_ms) {
    return (offset_ms > 0 ? "+" : "") + std::to_string(offset_ms) + "ms";
}

}  // namespace

song_select_scene::song_select_scene(scene_manager& manager, std::string preferred_song_id,
                                     std::string preferred_chart_id,
                                     std::optional<song_select::recent_result_offset> recent_result_offset,
                                     bool open_login_dialog_on_enter)
    : scene(manager),
      preferred_song_id_(std::move(preferred_song_id)),
      preferred_chart_id_(std::move(preferred_chart_id)),
      recent_result_offset_(std::move(recent_result_offset)),
      open_login_dialog_on_enter_(open_login_dialog_on_enter) {
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
    if (open_login_dialog_on_enter_) {
        song_select::open_login_dialog(state_, auth::load_session_summary());
    }
    if (state_.auth.logged_in) {
        auth_overlay::start_restore(auth_controller_, state_.login_dialog);
    }
    catalog_loading_ = false;
    catalog_reload_pending_ = false;
    pending_catalog_song_id_.clear();
    pending_catalog_chart_id_.clear();
    reload_song_library(preferred_song_id_, preferred_chart_id_);
}

void song_select_scene::on_exit() {
    song_select::close_context_menu(state_);
    state_.confirmation_dialog = {};
    state_.login_dialog.open = false;
    transfer_controller_.on_exit();
    preview_controller_.stop();
}

void song_select_scene::reload_song_library(const std::string& preferred_song_id,
                                            const std::string& preferred_chart_id) {
    if (catalog_loading_) {
        catalog_reload_pending_ = true;
        pending_catalog_song_id_ = preferred_song_id;
        pending_catalog_chart_id_ = preferred_chart_id;
        state_.catalog_loading = true;
        return;
    }

    preferred_song_id_ = preferred_song_id;
    preferred_chart_id_ = preferred_chart_id;
    catalog_loading_ = true;
    state_.catalog_loading = true;
    std::promise<song_select::catalog_data> promise;
    catalog_future_ = promise.get_future();
    std::thread([promise = std::move(promise)]() mutable {
        try {
            promise.set_value(song_select::load_catalog());
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void song_select_scene::poll_song_library_reload() {
    if (!catalog_loading_) {
        return;
    }
    if (catalog_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    try {
        song_select::apply_catalog(state_, catalog_future_.get(), preferred_song_id_, preferred_chart_id_);
    } catch (const std::exception& ex) {
        song_select::apply_catalog(state_, {}, preferred_song_id_, preferred_chart_id_);
        state_.load_errors = {ex.what()};
    }
    catalog_loading_ = false;
    sync_selected_song_media();

    if (!catalog_reload_pending_) {
        return;
    }

    catalog_reload_pending_ = false;
    reload_song_library(pending_catalog_song_id_, pending_catalog_chart_id_);
    pending_catalog_song_id_.clear();
    pending_catalog_chart_id_.clear();
}

void song_select_scene::sync_selected_song_media() {
    preview_controller_.select_song(song_select::selected_song(state_));
    reload_selected_chart_ranking();
}

void song_select_scene::reload_selected_chart_ranking() {
    if (ranking_loading_) {
        ++ranking_generation_;
        ranking_reload_pending_ = true;
        if (state_.ranking_panel.selected_source == ranking_service::source::online) {
            state_.ranking_panel.listing = {};
            state_.ranking_panel.listing.ranking_source = state_.ranking_panel.selected_source;
            state_.ranking_panel.listing.available = false;
            state_.ranking_panel.listing.message = "Loading online rankings...";
        }
        return;
    }

    const auto filtered = song_select::filtered_charts_for_selected_song(state_);
    const song_select::chart_option* chart = song_select::selected_chart_for(state_, filtered);
    const std::string chart_id = chart != nullptr ? chart->meta.chart_id : "";
    const ranking_service::source source = state_.ranking_panel.selected_source;
    ++ranking_generation_;
    ranking_pending_generation_ = ranking_generation_;
    state_.ranking_panel.source_dropdown_open = false;
    state_.ranking_panel.scroll_y = 0.0f;
    state_.ranking_panel.scroll_y_target = 0.0f;
    state_.ranking_panel.scrollbar_dragging = false;
    state_.ranking_panel.scrollbar_drag_offset = 0.0f;
    if (source == ranking_service::source::online) {
        state_.ranking_panel.listing = {};
        state_.ranking_panel.listing.ranking_source = source;
        state_.ranking_panel.listing.available = false;
        state_.ranking_panel.listing.message = "Loading online rankings...";
    }

    ranking_loading_ = true;
    std::promise<ranking_service::listing> promise;
    ranking_future_ = promise.get_future();
    std::thread([promise = std::move(promise), chart_id, source]() mutable {
        try {
            promise.set_value(ranking_service::load_chart_ranking(chart_id, source, 50));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void song_select_scene::poll_selected_chart_ranking() {
    if (!ranking_loading_) {
        return;
    }
    if (ranking_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    ranking_service::listing listing;
    try {
        listing = ranking_future_.get();
    } catch (const std::exception& ex) {
        listing.available = false;
        listing.message = ex.what();
        listing.ranking_source = state_.ranking_panel.selected_source;
    }
    ranking_loading_ = false;
    const bool stale = ranking_pending_generation_ != ranking_generation_;
    if (!stale) {
        state_.ranking_panel.listing = std::move(listing);
    }

    if (ranking_reload_pending_) {
        ranking_reload_pending_ = false;
        reload_selected_chart_ranking();
    }
}

void song_select_scene::refresh_auth_state() {
    auth_overlay::refresh_auth_state(state_.auth);
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

void song_select_scene::open_overwrite_song_confirmation(std::vector<song_select::song_import_request> requests) {
    const size_t overwrite_count = requests.size();
    transfer_controller_.set_pending_song_import_requests(std::move(requests));
    song_select::open_confirmation_dialog(
        state_, song_select::pending_confirmation_action::overwrite_song_import,
        overwrite_count <= 1 ? "Overwrite Song" : "Overwrite Songs",
        overwrite_count <= 1 ? "A user song with the same song ID already exists. Overwrite it?"
                             : "Some selected songs already exist. Overwrite them?",
        "",
        "OVERWRITE");
}

void song_select_scene::open_overwrite_chart_confirmation(std::vector<song_select::chart_import_request> requests) {
    const size_t overwrite_count = requests.size();
    transfer_controller_.set_pending_chart_import_requests(std::move(requests));
    song_select::open_confirmation_dialog(
        state_, song_select::pending_confirmation_action::overwrite_chart_import,
        overwrite_count <= 1 ? "Overwrite Chart" : "Overwrite Charts",
        overwrite_count <= 1 ? "A user chart with the same chart ID already exists. Overwrite it?"
                             : "Some selected charts already exist. Overwrite them?",
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

void song_select_scene::update(float dt) {
    ui::begin_hit_regions();
    const bool modal_ui_active = state_.confirmation_dialog.open || state_.login_dialog.open ||
                                 transfer_controller_.busy();
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
    poll_song_library_reload();
    poll_selected_chart_ranking();
    if (const auto restore_result = auth_overlay::poll_restore(auth_controller_, state_.auth, state_.login_dialog);
        restore_result.should_show_notice) {
        song_select::queue_status_message(state_, restore_result.notice_message, restore_result.notice_is_error);
    }
    auth_overlay::poll_request(auth_controller_, state_.auth, state_.login_dialog);
    if (const auto prepared = transfer_controller_.poll_song_import_prepare(); prepared.has_value()) {
        if (prepared->requests.empty()) {
            apply_transfer_result(prepared->transfer);
        } else if (prepared->overwrite_count > 0) {
            open_overwrite_song_confirmation(prepared->requests);
        } else {
            transfer_controller_.start_song_imports(prepared->requests);
            song_select::queue_status_message(state_, transfer_controller_.busy_label(), false);
        }
    }
    if (const auto result = transfer_controller_.poll_background_transfer(); result.has_value()) {
        apply_transfer_result(*result);
    }

    if (state_.confirmation_dialog.open &&
        !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        state_.confirmation_dialog.suppress_initial_pointer_cancel = false;
    }

    if (transfer_controller_.busy()) {
        return;
    }

    if (state_.login_dialog.open) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        if (!auth_controller_.request_active &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            !CheckCollisionPointRec(mouse, song_select::login_dialog_rect(state_))) {
            state_.login_dialog.open = false;
            return;
        }
        if (IsKeyPressed(KEY_ESCAPE) && !auth_controller_.request_active) {
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
        manager_.change_scene(song_select::make_title_scene(manager_, true));
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
    state_.scroll_y = tween::damp(state_.scroll_y, state_.scroll_y_target, dt,
                                  song_select::layout::kScrollLerpSpeed, 0.5f);

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
    state_.ranking_panel.scroll_y = tween::damp(state_.ranking_panel.scroll_y,
                                                state_.ranking_panel.scroll_y_target, dt,
                                                song_select::layout::kScrollLerpSpeed, 0.5f);

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
    virtual_screen::begin_ui();
    ClearBackground(theme.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, theme.bg, theme.bg_alt);
    ui::begin_draw_queue();
    song_select::draw_frame(state_);
    song_select::draw_song_list(state_);

    if (state_.songs.empty()) {
        song_select::draw_empty_state(state_);
        if (transfer_controller_.busy()) {
            song_select::draw_busy_overlay(transfer_controller_.busy_label());
        }
        song_select::commands::apply_context_menu_command(
            manager_, state_, transfer_controller_, song_select::draw_context_menu(state_),
            [this](const song_select::transfer_result& result) { apply_transfer_result(result); },
            [this](const std::string& preferred_song_id, const std::string& preferred_chart_id) {
                reload_song_library(preferred_song_id, preferred_chart_id);
            },
            [this](std::vector<song_select::song_import_request> requests) {
                open_overwrite_song_confirmation(std::move(requests));
            },
            [this](std::vector<song_select::chart_import_request> requests) {
                open_overwrite_chart_confirmation(std::move(requests));
            });
        const song_select::login_dialog_command login_command =
            song_select::draw_login_dialog(state_, auth_controller_.request_active);
        if (login_command == song_select::login_dialog_command::close) {
            state_.login_dialog.open = false;
        } else if (login_command != song_select::login_dialog_command::none) {
            auth_overlay::start_request(auth_controller_, state_.login_dialog, login_command);
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
        !transfer_controller_.busy();
    const song_select::ranking_panel_result ranking_result =
        song_select::draw_ranking_panel(state_, ranking_source_dropdown_interactive);
    if (transfer_controller_.busy()) {
        song_select::draw_busy_overlay(transfer_controller_.busy_label());
    }
    song_select::commands::apply_context_menu_command(
        manager_, state_, transfer_controller_, song_select::draw_context_menu(state_),
        [this](const song_select::transfer_result& result) { apply_transfer_result(result); },
        [this](const std::string& preferred_song_id, const std::string& preferred_chart_id) {
            reload_song_library(preferred_song_id, preferred_chart_id);
        },
        [this](std::vector<song_select::song_import_request> requests) {
            open_overwrite_song_confirmation(std::move(requests));
        },
        [this](std::vector<song_select::chart_import_request> requests) {
            open_overwrite_chart_confirmation(std::move(requests));
        });
    song_select::commands::apply_confirmation_command(
        state_, preview_controller_, transfer_controller_, song_select::draw_confirmation_dialog(state_),
        [this](const song_select::delete_result& result) { apply_delete_result(result); },
        [this](const song_select::transfer_result& result) { apply_transfer_result(result); });
    const song_select::login_dialog_command login_command =
        song_select::draw_login_dialog(state_, auth_controller_.request_active);
    if (login_command == song_select::login_dialog_command::close) {
        state_.login_dialog.open = false;
    } else if (login_command != song_select::login_dialog_command::none) {
        auth_overlay::start_request(auth_controller_, state_.login_dialog, login_command);
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
