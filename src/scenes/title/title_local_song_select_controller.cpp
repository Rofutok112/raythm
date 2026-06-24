#include "title/title_local_song_select_controller.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

#include "services/content_sync_service.h"
#include "song_select/ranking_source_policy.h"
#include "song_select/song_select_confirmation_dialog.h"
#include "song_select/song_select_layout.h"
#include "title/center_panel_view.h"
#include "title/create_tools_view.h"
#include "title/ranking_panel_view.h"
#include "title/song_list_view.h"
#include "tween.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace title_local_song_select_controller {
namespace {

using title_play_view::layout;
using title_play_view::mode;
using title_play_view::update_result;
using title_play_view::auto_mod_toggle_rect;
using title_play_view::kChartFilterMaxLevel;
using title_play_view::kChartFilterMinLevel;
using title_play_view::level_filter_chip_rect;
using title_play_view::level_from_filter_t;
using title_play_view::mod_button_rect;
using title_play_view::mod_modal_rect;
using title_play_view::no_fail_mod_toggle_rect;
using title_play_view::play_filter_button_rect;
using title_play_view::play_filter_clear_button_rect;
using title_play_view::play_filter_key_button_rect;
using title_play_view::play_filter_level_slider_rect;
using title_play_view::play_filter_modal_rect;
using title_play_view::play_filter_source_button_rect;
using title_play_view::preview_next_button_rect;
using title_play_view::preview_play_button_rect;
using title_play_view::preview_prev_button_rect;
using title_play_view::song_list_rect;
using title_play_view::start_button_rect;

constexpr float kContextMenuInnerPadding = 6.0f;
constexpr float kWheelScrollStep = 63.0f;
constexpr int kPlaySongContextMenuItemCount = 1;
constexpr int kPlayChartContextMenuItemCount = 1;

Rectangle context_menu_item_rect(Rectangle menu_rect, int index = 0) {
    return {menu_rect.x + kContextMenuInnerPadding,
            menu_rect.y + kContextMenuInnerPadding + static_cast<float>(index) *
                (song_select::layout::kContextMenuItemHeight + song_select::layout::kContextMenuItemSpacing),
            menu_rect.width - kContextMenuInnerPadding * 2.0f,
            song_select::layout::kContextMenuItemHeight};
}

bool block_locked_play_if_needed(song_select::state& state,
                                 const song_select::song_entry* song,
                                 const song_select::chart_option* chart) {
    if (song == nullptr || chart == nullptr ||
        !content_is_play_locked(song->song.meta, chart->meta)) {
        return false;
    }
    song_select::queue_status_message(state, content_play_lock_reason(song->song.meta, chart->meta), true);
    return true;
}

bool apply_play_filter_change(song_select::state& state,
                              song_select::chart_source_filter source,
                              int key_filter,
                              float min_level,
                              float max_level,
                              update_result& result) {
    const int previous_song = state.selected_song_index;
    const int previous_chart = state.difficulty_index;
    if (!song_select::apply_chart_filters(state, source, key_filter, min_level, max_level)) {
        return false;
    }
    state.scroll_y = 0.0f;
    state.scroll_y_target = 0.0f;
    result.song_selection_changed = previous_song != state.selected_song_index;
    result.chart_selection_changed = previous_chart != state.difficulty_index || !result.song_selection_changed;
    return true;
}

}  // namespace
title_play_view::update_result update(song_select::state& state,
                                      title_play_view::mode view_mode,
                                      float anim_t,
                                      Rectangle origin_rect,
                                      float dt,
                                      const title_create_tools_model::view_model* create_tools_model,
                                      title_preview_snapshot preview) {
    update_result result;
    const bool preview_loading =
        preview.audio.status == song_select::preview_audio_loader::load_status::loading;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    const float wheel = GetMouseWheelMove();
    const layout current = title_play_view::make_mode_layout(anim_t, origin_rect, view_mode);
    const bool play_mode = view_mode == mode::play;
    const bool create_mode = view_mode == mode::create;
    const bool song_select_mode = play_mode || create_mode;
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const bool has_selection = song_select::selected_song(state) != nullptr &&
                               song_select::selected_chart_for(state, filtered) != nullptr;
    const bool play_song_menu_open =
        play_mode &&
        state.context_menu.open &&
        state.context_menu.target == song_select::context_menu_target::song;
    const bool play_chart_menu_open =
        play_mode &&
        state.context_menu.open &&
        state.context_menu.target == song_select::context_menu_target::chart;

    if (state.confirmation_dialog.open) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            state.confirmation_dialog = {};
        }
        return result;
    }

    if (play_song_menu_open || play_chart_menu_open) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            song_select::close_context_menu(state);
            return result;
        }
        if (left_pressed && CheckCollisionPointRec(mouse, context_menu_item_rect(state.context_menu.rect))) {
            const bool deleting_chart = state.context_menu.target == song_select::context_menu_target::chart;
            song_select::open_confirmation_dialog(
                state,
                deleting_chart ? song_select::pending_confirmation_action::delete_chart
                               : song_select::pending_confirmation_action::delete_song,
                "", "", "", "DELETE", state.context_menu.song_index,
                deleting_chart ? state.context_menu.chart_index : -1);
            song_select::close_context_menu(state);
            if (deleting_chart) {
                result.delete_chart_requested = true;
            } else {
                result.delete_song_requested = true;
            }
            return result;
        }
        if ((left_pressed || right_pressed) && !CheckCollisionPointRec(mouse, state.context_menu.rect)) {
            song_select::close_context_menu(state);
            return result;
        }
        return result;
    }

    if (song_select_mode && state.play_filter_modal_open) {
        const Rectangle modal = play_filter_modal_rect(current);
        if (IsKeyPressed(KEY_ESCAPE) ||
            (left_pressed && !CheckCollisionPointRec(mouse, modal))) {
            state.play_filter_modal_open = false;
            state.chart_level_filter_dragging = false;
            return result;
        }
    }

    if (play_mode && state.play_mod_modal_open) {
        const Rectangle modal = mod_modal_rect(current.ranking_column);
        if (IsKeyPressed(KEY_ESCAPE) ||
            (left_pressed &&
             !CheckCollisionPointRec(mouse, modal) &&
             !CheckCollisionPointRec(mouse, mod_button_rect(current.ranking_column)))) {
            state.play_mod_modal_open = false;
            return result;
        }
        if (left_pressed && CheckCollisionPointRec(mouse, auto_mod_toggle_rect(modal))) {
            state.mods.auto_play = !state.mods.auto_play;
            return result;
        }
        if (left_pressed && CheckCollisionPointRec(mouse, no_fail_mod_toggle_rect(modal))) {
            state.mods.no_fail = !state.mods.no_fail;
            return result;
        }
        if (CheckCollisionPointRec(mouse, modal)) {
            return result;
        }
    }

    if (ui::is_clicked(current.back_rect) || IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
        return result;
    }

    if (song_select_mode) {
        const std::vector<int> song_indices = song_select::filtered_song_indices(state);
        if (!song_indices.empty() &&
            std::find(song_indices.begin(), song_indices.end(), state.selected_song_index) == song_indices.end()) {
            if (song_select::apply_song_selection(state, song_indices.front(), 0)) {
                result.song_selection_changed = true;
                return result;
            }
        }
    }

    if (song_select_mode && left_pressed &&
        CheckCollisionPointRec(mouse, play_filter_button_rect(current.song_column))) {
        state.play_filter_modal_open = !state.play_filter_modal_open;
        if (state.play_filter_modal_open) {
            state.play_mod_modal_open = false;
        }
        return result;
    }

    if (play_mode && left_pressed &&
        CheckCollisionPointRec(mouse, mod_button_rect(current.ranking_column))) {
        state.play_mod_modal_open = !state.play_mod_modal_open;
        if (state.play_mod_modal_open) {
            state.play_filter_modal_open = false;
            state.chart_level_filter_dragging = false;
        }
        return result;
    }

    if (song_select_mode && state.play_filter_modal_open && left_pressed) {
        const Rectangle filter_panel = play_filter_modal_rect(current);
        const song_select::chart_source_filter source_values[] = {
            song_select::chart_source_filter::all,
            song_select::chart_source_filter::official,
            song_select::chart_source_filter::community,
        };
        for (int index = 0; index < 3; ++index) {
            if (CheckCollisionPointRec(mouse, play_filter_source_button_rect(filter_panel, index))) {
                if (apply_play_filter_change(state, source_values[index],
                                             state.chart_key_filter, state.chart_min_level, state.chart_max_level, result)) {
                    return result;
                }
            }
        }

        const int key_values[] = {0, 4, 5, 6, 7};
        for (int index = 0; index < 5; ++index) {
            if (CheckCollisionPointRec(mouse, play_filter_key_button_rect(filter_panel, index))) {
                if (apply_play_filter_change(state, state.chart_source, key_values[index],
                                             state.chart_min_level, state.chart_max_level, result)) {
                    return result;
                }
            }
        }

        if (CheckCollisionPointRec(mouse, play_filter_clear_button_rect(filter_panel))) {
            const bool search_changed = !state.play_search_input.value.empty();
            if (search_changed) {
                state.play_search_input = {};
            }
            if (apply_play_filter_change(state,
                                         song_select::chart_source_filter::all,
                                         0,
                                         kChartFilterMinLevel,
                                         kChartFilterMaxLevel,
                                         result)) {
                return result;
            }
            if (search_changed) {
                const std::vector<int> song_indices = song_select::filtered_song_indices(state);
                if (!song_indices.empty() &&
                    std::find(song_indices.begin(), song_indices.end(), state.selected_song_index) == song_indices.end()) {
                    song_select::apply_song_selection(state, song_indices.front(), 0);
                    result.song_selection_changed = true;
                }
                return result;
            }
        }
    }

    if (song_select_mode && state.play_filter_modal_open && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Rectangle filter_panel = play_filter_modal_rect(current);
        const Rectangle slider = play_filter_level_slider_rect(filter_panel);
        const Rectangle min_chip = level_filter_chip_rect(slider, state.chart_min_level);
        const Rectangle max_chip = level_filter_chip_rect(slider, state.chart_max_level);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mouse, max_chip)) {
                state.chart_level_filter_dragging = true;
                state.chart_level_filter_dragging_min = false;
            } else if (CheckCollisionPointRec(mouse, min_chip)) {
                state.chart_level_filter_dragging = true;
                state.chart_level_filter_dragging_min = true;
            }
        }
        if (state.chart_level_filter_dragging) {
            const float ratio = std::clamp((mouse.x - slider.x) / slider.width, 0.0f, 1.0f);
            const float level = level_from_filter_t(ratio);
            const bool move_min = state.chart_level_filter_dragging_min;
            const float next_min = move_min ? std::min(level, state.chart_max_level) : state.chart_min_level;
            const float next_max = move_min ? state.chart_max_level : std::max(level, state.chart_min_level);
            if (apply_play_filter_change(state, state.chart_source, state.chart_key_filter,
                                         next_min, next_max, result)) {
                return result;
            }
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        state.chart_level_filter_dragging = false;
    }
    if (song_select_mode && state.play_filter_modal_open &&
        CheckCollisionPointRec(mouse, play_filter_modal_rect(current))) {
        return result;
    }

    if (song_select_mode && !preview_loading) {
        const song_select::song_entry* song = song_select::selected_song(state);
        const Rectangle progress = current.meta_rect;
        const Rectangle progress_hit = {progress.x, progress.y - 12.0f, progress.width, progress.height + 24.0f};
        if (song != nullptr && left_pressed && CheckCollisionPointRec(mouse, progress_hit)) {
            state.preview_bar_dragging = true;
            state.preview_bar_resume_after_drag = preview.playing;
            state.preview_bar_drag_position_seconds = preview.position_seconds;
            result.preview_pause_requested = true;
        }
        if (song != nullptr && state.preview_bar_dragging) {
            const double preview_length = preview.length_seconds;
            if (preview_length > 0.0) {
                const float ratio = std::clamp((mouse.x - progress.x) / progress.width, 0.0f, 1.0f);
                state.preview_bar_drag_position_seconds = preview_length * static_cast<double>(ratio);
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                return result;
            }
            result.preview_seek_requested = true;
            result.preview_seek_seconds = state.preview_bar_drag_position_seconds;
            if (state.preview_bar_resume_after_drag) {
                result.preview_resume_requested = true;
            }
            state.preview_bar_dragging = false;
            state.preview_bar_resume_after_drag = false;
        }
    }

    if (play_mode && !state.play_search_input.active && IsKeyPressed(KEY_ENTER) && has_selection) {
        const song_select::song_entry* song = song_select::selected_song(state);
        const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
        if (block_locked_play_if_needed(state, song, chart)) {
            return result;
        }
        if (state.filter.multiplayer_queueable_only) {
            result.multiplayer_select_requested = true;
        } else {
            result.play_requested = true;
        }
        return result;
    }

    if (play_mode) {
        const song_select::song_entry* song = song_select::selected_song(state);
        const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
        if (left_pressed && has_selection && CheckCollisionPointRec(mouse, start_button_rect(current.ranking_column))) {
            if (block_locked_play_if_needed(state, song, chart)) {
                return result;
            }
            if (state.filter.multiplayer_queueable_only) {
                result.multiplayer_select_requested = true;
            } else {
                result.play_requested = true;
            }
            return result;
        }
        if (!preview_loading && left_pressed && CheckCollisionPointRec(mouse, preview_play_button_rect(current))) {
            result.preview_toggle_requested = true;
            return result;
        }
        if (left_pressed && !state.songs.empty() &&
            CheckCollisionPointRec(mouse, preview_prev_button_rect(current))) {
            const std::vector<int> song_indices = song_select::filtered_song_indices(state);
            const auto current_it = std::find(song_indices.begin(), song_indices.end(), state.selected_song_index);
            if (current_it != song_indices.end() && current_it != song_indices.begin() &&
                song_select::apply_song_selection(state, *std::prev(current_it), 0)) {
                result.song_selection_changed = true;
            }
            return result;
        }
        if (left_pressed && !state.songs.empty() &&
            CheckCollisionPointRec(mouse, preview_next_button_rect(current))) {
            const std::vector<int> song_indices = song_select::filtered_song_indices(state);
            const auto current_it = std::find(song_indices.begin(), song_indices.end(), state.selected_song_index);
            if (current_it != song_indices.end() && std::next(current_it) != song_indices.end() &&
                song_select::apply_song_selection(state, *std::next(current_it), 0)) {
                result.song_selection_changed = true;
            }
            return result;
        }
        const bool song_reinstall_available =
            song != nullptr &&
            (content_sync_service::is_update_available(song->sync_state) ||
             content_sync_service::is_modified(song->sync_state));
        const bool chart_reinstall_available =
            chart != nullptr &&
            (content_sync_service::is_update_available(chart->sync_state) ||
             content_sync_service::is_modified(chart->sync_state));
        if (left_pressed && (song_reinstall_available || chart_reinstall_available)) {
            const bool song_update_clicked =
                song_reinstall_available &&
                CheckCollisionPointRec(mouse, title_center_view::song_status_badge_rect(current.main_column));
            const bool chart_update_clicked =
                chart_reinstall_available &&
                CheckCollisionPointRec(mouse, title_center_view::chart_status_badge_rect(current.chart_detail_rect));
            if (song_update_clicked) {
                result.update_song_requested = true;
                return result;
            }
            if (chart_update_clicked) {
                result.update_chart_requested = true;
                return result;
            }
        }

        const title_ranking_view::draw_config ranking_config = {
            .header_rect = current.ranking_header_rect,
            .source_friends_rect = current.ranking_source_friends_rect,
            .source_local_rect = current.ranking_source_local_rect,
            .source_online_rect = current.ranking_source_online_rect,
            .list_rect = current.ranking_list_rect,
            .source_availability = song_select::ranking_source_policy::availability_for_chart(chart),
        };
        if (left_pressed) {
            const auto source = title_ranking_view::hit_test_source(ranking_config, mouse);
            if (source.has_value() && source.value() != state.ranking_panel.selected_source) {
                state.ranking_panel.selected_source = *source;
                result.ranking_source_changed = true;
                return result;
            }
            const auto profile_user_id =
                title_ranking_view::hit_test_profile_user_id(state.ranking_panel, ranking_config, mouse);
            if (profile_user_id.has_value()) {
                result.requested_profile_user_id = *profile_user_id;
                return result;
            }
        }
    } else if (create_mode) {
        const title_create_tools_model::view_model empty_create_tools;
        const update_result create_result =
            title_create_tools_view::update(create_tools_model != nullptr ? *create_tools_model : empty_create_tools,
                                            current, left_pressed, mouse);
        if (create_result.create_song_requested ||
            create_result.edit_song_requested ||
            create_result.import_song_requested ||
            create_result.export_song_requested ||
            create_result.upload_song_requested ||
            create_result.create_chart_requested ||
            create_result.edit_chart_requested ||
            create_result.import_chart_requested ||
            create_result.export_chart_requested ||
            create_result.upload_chart_requested ||
            create_result.edit_mv_requested) {
            return create_result;
        }
    }

    if (left_pressed) {
        const auto clicked_chart =
            title_song_list_view::chart_hit{-1, title_center_view::hit_test_chart(
                                                    current.chart_buttons_rect, state.chart_scroll_y, mouse,
                                                    static_cast<int>(filtered.size()))};
        if (clicked_chart.chart_index >= 0) {
            if (play_mode && state.difficulty_index == clicked_chart.chart_index) {
                const song_select::song_entry* song = song_select::selected_song(state);
                const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
                if (block_locked_play_if_needed(state, song, chart)) {
                    return result;
                }
                if (state.filter.multiplayer_queueable_only) {
                    result.multiplayer_select_requested = true;
                } else {
                    result.play_requested = true;
                }
            } else if (state.difficulty_index != clicked_chart.chart_index) {
                state.difficulty_index = clicked_chart.chart_index;
                state.chart_change_anim_t = 1.0f;
                result.chart_selection_changed = true;
            }
            return result;
        }
    }

    if (right_pressed && play_mode && !state.songs.empty()) {
        const auto clicked_chart =
            title_song_list_view::chart_hit{-1, title_center_view::hit_test_chart(
                                                    current.chart_buttons_rect, state.chart_scroll_y, mouse,
                                                    static_cast<int>(filtered.size()))};
        if (clicked_chart.chart_index >= 0) {
            if (state.difficulty_index != clicked_chart.chart_index) {
                state.difficulty_index = clicked_chart.chart_index;
                state.chart_change_anim_t = 1.0f;
                result.chart_selection_changed = true;
            }
            state.context_menu.open = true;
            state.context_menu.target = song_select::context_menu_target::chart;
            state.context_menu.section = song_select::context_menu_section::chart;
            state.context_menu.song_index = state.selected_song_index;
            state.context_menu.chart_index = clicked_chart.chart_index;
            state.context_menu.rect = song_select::layout::make_context_menu_rect(
                mouse, kPlayChartContextMenuItemCount);
            return result;
        }
    }

    if (right_pressed && play_mode && !state.songs.empty()) {
        const int clicked_song =
            title_song_list_view::hit_test(state, song_list_rect(current), state.scroll_y, mouse);
        if (clicked_song >= 0) {
            if (song_select::apply_song_selection(state, clicked_song, 0)) {
                result.song_selection_changed = true;
            }
            state.context_menu.open = true;
            state.context_menu.target = song_select::context_menu_target::song;
            state.context_menu.section = song_select::context_menu_section::song;
            state.context_menu.song_index = clicked_song;
            state.context_menu.chart_index = -1;
            state.context_menu.rect = song_select::layout::make_context_menu_rect(
                mouse, kPlaySongContextMenuItemCount);
            return result;
        }
    }

    if (left_pressed && !state.songs.empty()) {
        const int clicked_song =
            song_select_mode
                ? title_song_list_view::hit_test(state, song_list_rect(current), state.scroll_y, mouse)
                : title_song_list_view::hit_test(state, current.song_column, state.scroll_y, mouse);
        if (clicked_song >= 0) {
            if (song_select::apply_song_selection(state, clicked_song, 0)) {
                result.song_selection_changed = true;
            }
            return result;
        }
    }

    const std::vector<int> song_indices = song_select::filtered_song_indices(state);
    const auto selected_song_it = std::find(song_indices.begin(), song_indices.end(), state.selected_song_index);
    if (!state.play_search_input.active && !song_indices.empty() && selected_song_it != song_indices.end() &&
        (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))) {
        if (selected_song_it != song_indices.begin() &&
            song_select::apply_song_selection(state, *std::prev(selected_song_it), 0)) {
            result.song_selection_changed = true;
        }
    } else if (!state.play_search_input.active && !song_indices.empty() && selected_song_it != song_indices.end() &&
               (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))) {
        if (std::next(selected_song_it) != song_indices.end() &&
            song_select::apply_song_selection(state, *std::next(selected_song_it), 0)) {
            result.song_selection_changed = true;
        }
    }

    if (!state.play_search_input.active && !filtered.empty() && (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))) {
        const int next_index = std::clamp(state.difficulty_index - 1, 0, static_cast<int>(filtered.size()) - 1);
        if (next_index != state.difficulty_index) {
            state.difficulty_index = next_index;
            state.chart_change_anim_t = 1.0f;
            result.chart_selection_changed = true;
        }
    } else if (!state.play_search_input.active && !filtered.empty() && (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))) {
        const int next_index = std::clamp(state.difficulty_index + 1, 0, static_cast<int>(filtered.size()) - 1);
        if (next_index != state.difficulty_index) {
            state.difficulty_index = next_index;
            state.chart_change_anim_t = 1.0f;
            result.chart_selection_changed = true;
        }
    }

    const Rectangle song_scroll_area = song_select_mode ? song_list_rect(current) : current.song_column;
    if (CheckCollisionPointRec(mouse, song_scroll_area) && wheel != 0.0f) {
        state.scroll_y_target -= wheel * kWheelScrollStep;
    } else if (CheckCollisionPointRec(mouse, current.chart_buttons_rect) && wheel != 0.0f) {
        state.chart_scroll_y_target -= wheel * kWheelScrollStep;
    } else if (play_mode && CheckCollisionPointRec(mouse, current.ranking_list_rect) && wheel != 0.0f) {
        state.ranking_panel.scroll_y_target -= wheel * kWheelScrollStep;
    }

    state.scroll_y_target = std::clamp(
        state.scroll_y_target, 0.0f,
        song_select_mode
            ? title_song_list_view::max_scroll(song_list_rect(current), state)
            : title_song_list_view::max_scroll(current.song_column, state));
    state.scroll_y = tween::damp(state.scroll_y, state.scroll_y_target, dt, 12.0f, 0.5f);

    state.chart_scroll_y_target = std::clamp(
        state.chart_scroll_y_target, 0.0f,
        title_center_view::max_chart_scroll(current.chart_buttons_rect, static_cast<int>(filtered.size())));
    state.chart_scroll_y = tween::damp(state.chart_scroll_y, state.chart_scroll_y_target, dt, 12.0f, 0.5f);

    state.embedded_chart_scroll_y_target = std::clamp(state.embedded_chart_scroll_y_target, 0.0f, 0.0f);
    state.embedded_chart_scroll_y =
        tween::damp(state.embedded_chart_scroll_y, state.embedded_chart_scroll_y_target, dt, 12.0f, 0.5f);

    if (play_mode) {
        state.ranking_panel.scroll_y_target = std::clamp(
            state.ranking_panel.scroll_y_target, 0.0f,
            title_ranking_view::max_scroll(current.ranking_list_rect, state.ranking_panel.listing));
        state.ranking_panel.scroll_y =
            tween::damp(state.ranking_panel.scroll_y, state.ranking_panel.scroll_y_target, dt, 12.0f, 0.5f);
    }

    return result;
}


}  // namespace title_local_song_select_controller
