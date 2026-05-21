#include "title/title_local_song_select_controller.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

#include "audio_manager.h"
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

constexpr Rectangle kPlayBackButtonRect = {72.0f, 57.0f, 147.0f, 57.0f};
constexpr Rectangle kPlaySongColumnRect = {39.0f, 146.0f, 330.0f, 850.0f};
constexpr Rectangle kPlayMainColumnRect = {390.0f, 146.0f, 820.0f, 850.0f};
constexpr Rectangle kPlayRankingColumnRect = {1228.0f, 146.0f, 650.0f, 850.0f};
constexpr Rectangle kPlayJacketRect = {1258.0f, 182.0f, 212.0f, 212.0f};
constexpr Rectangle kPlayChartDetailRect = {1494.0f, 186.0f, 350.0f, 246.0f};
constexpr Rectangle kPlayMetaRect = {1258.0f, 418.0f, 590.0f, 10.0f};
constexpr Rectangle kPlayChartButtonsRect = {414.0f, 205.0f, 772.0f, 740.0f};
constexpr Rectangle kPlayRankingHeaderRect = {1256.0f, 616.0f, 596.0f, 26.0f};
constexpr Rectangle kPlayRankingSourceLocalRect = {1394.0f, 650.0f, 134.0f, 34.0f};
constexpr Rectangle kPlayRankingSourceOnlineRect = {1256.0f, 650.0f, 134.0f, 34.0f};
constexpr Rectangle kPlayRankingListRect = {1256.0f, 692.0f, 596.0f, 200.0f};
constexpr Rectangle kCreateSongColumnRect = {99.0f, 162.0f, 507.0f, 756.0f};
constexpr Rectangle kCreateMainColumnRect = {657.0f, 150.0f, 603.0f, 780.0f};
constexpr Rectangle kCreateRankingColumnRect = {1317.0f, 153.0f, 507.0f, 774.0f};
constexpr Rectangle kCreateJacketRect = {684.0f, 273.0f, 282.0f, 282.0f};
constexpr Rectangle kCreateChartDetailRect = {1002.0f, 273.0f, 258.0f, 135.0f};
constexpr Rectangle kCreateMetaRect = {684.0f, 582.0f, 576.0f, 90.0f};
constexpr Rectangle kCreateChartButtonsRect = {684.0f, 600.0f, 576.0f, 327.0f};
constexpr Rectangle kCreateRankingHeaderRect = {1317.0f, 153.0f, 507.0f, 54.0f};
constexpr Rectangle kCreateRankingSourceLocalRect = {1551.0f, 147.0f, 129.0f, 51.0f};
constexpr Rectangle kCreateRankingSourceOnlineRect = {1689.0f, 147.0f, 135.0f, 51.0f};
constexpr Rectangle kCreateRankingListRect = {1317.0f, 225.0f, 507.0f, 702.0f};
constexpr Rectangle kFallbackOriginRect = {840.0f, 564.0f, 240.0f, 90.0f};
constexpr Vector2 kSeedSongOffset = {-495.0f, 33.0f};
constexpr Vector2 kSeedMainOffset = {0.0f, 9.0f};
constexpr Vector2 kSeedRankingOffset = {495.0f, 39.0f};
constexpr Vector2 kSeedBackOffset = {-642.0f, -291.0f};
constexpr Vector2 kSeedJacketOffset = {-102.0f, -39.0f};
constexpr Vector2 kSeedMetaOffset = {15.0f, 138.0f};
constexpr Vector2 kSeedChartDetailOffset = {228.0f, -15.0f};
constexpr Vector2 kSeedChartButtonsOffset = {81.0f, 240.0f};
constexpr Vector2 kSeedRankingHeaderOffset = {522.0f, -276.0f};
constexpr Vector2 kSeedRankingSourceLocalOffset = {627.0f, -285.0f};
constexpr Vector2 kSeedRankingSourceOnlineOffset = {768.0f, -285.0f};
constexpr Vector2 kSeedRankingListOffset = {522.0f, 30.0f};
constexpr float kContextMenuInnerPadding = 6.0f;
constexpr float kChartFilterMinLevel = 0.0f;
constexpr float kChartFilterUsefulMaxLevel = 15.0f;
constexpr float kChartFilterMaxLevel = 99.0f;
constexpr float kChartFilterUsefulTrack = 0.97f;
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

Rectangle start_button_rect(Rectangle ranking_column) {
    return {ranking_column.x + 320.0f, ranking_column.y + ranking_column.height - 78.0f,
            ranking_column.width - 344.0f, 58.0f};
}

Rectangle preview_prev_button_rect(const layout& current) {
    return {current.ranking_column.x + 30.0f, current.ranking_column.y + 308.0f, 174.0f, 48.0f};
}

Rectangle preview_play_button_rect(const layout& current) {
    return {current.ranking_column.x + current.ranking_column.width * 0.5f - 77.0f,
            current.ranking_column.y + 305.0f, 154.0f, 54.0f};
}

Rectangle preview_next_button_rect(const layout& current) {
    return {current.ranking_column.x + current.ranking_column.width - 204.0f,
            current.ranking_column.y + 308.0f, 174.0f, 48.0f};
}

Rectangle play_filter_source_button_rect(Rectangle panel, int index) {
    const float source_y = panel.y + 122.0f;
    const float source_w = (panel.width - 48.0f) / 3.0f;
    return {panel.x + 18.0f + static_cast<float>(index) * (source_w + 6.0f), source_y, source_w, 36.0f};
}

Rectangle play_filter_key_button_rect(Rectangle panel, int index) {
    constexpr float key_w = 46.0f;
    constexpr float key_gap = 7.0f;
    const float keys_x = panel.x + (panel.width - (key_w * 5.0f + key_gap * 4.0f)) * 0.5f;
    return {keys_x + static_cast<float>(index) * (key_w + key_gap), panel.y + 368.0f, key_w, 30.0f};
}

Rectangle play_filter_clear_button_rect(Rectangle panel) {
    return {panel.x + 18.0f, panel.y + 456.0f, panel.width - 36.0f, 42.0f};
}

Rectangle play_filter_level_slider_rect(Rectangle panel) {
    return {panel.x + 34.0f, panel.y + 252.0f, panel.width - 68.0f, 18.0f};
}

float level_filter_t(float level) {
    const float clamped = std::clamp(level, kChartFilterMinLevel, kChartFilterMaxLevel);
    if (clamped <= kChartFilterUsefulMaxLevel) {
        return ((clamped - kChartFilterMinLevel) / (kChartFilterUsefulMaxLevel - kChartFilterMinLevel)) *
               kChartFilterUsefulTrack;
    }
    return 1.0f;
}

float level_from_filter_t(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    if (clamped > kChartFilterUsefulTrack) {
        return kChartFilterMaxLevel;
    }
    const float level = kChartFilterMinLevel +
                        (clamped / kChartFilterUsefulTrack) *
                            (kChartFilterUsefulMaxLevel - kChartFilterMinLevel);
    const float rounded = std::round(level * 10.0f) / 10.0f;
    return rounded >= kChartFilterMaxLevel - 0.5f ? kChartFilterMaxLevel : rounded;
}

Rectangle level_filter_chip_rect(Rectangle range, float level) {
    const float t = level_filter_t(level);
    const float x = range.x + range.width * t;
    return {x - 24.0f, range.y - 4.0f, 48.0f, 28.0f};
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

double selected_preview_length_seconds(const song_select::song_entry* song) {
    const double audio_length = audio_manager::instance().get_preview_length_seconds();
    if (audio_length > 0.0) {
        return audio_length;
    }
    return song != nullptr ? static_cast<double>(song->song.meta.duration_seconds) : 0.0;
}

Rectangle centered_scaled_rect(Rectangle anchor, Rectangle target, float scale, Vector2 offset = {0.0f, 0.0f}) {
    const Vector2 center = {anchor.x + anchor.width * 0.5f + offset.x,
                            anchor.y + anchor.height * 0.5f + offset.y};
    const float width = target.width * scale;
    const float height = target.height * scale;
    return {center.x - width * 0.5f, center.y - height * 0.5f, width, height};
}

Rectangle resolve_origin_rect(Rectangle origin_rect) {
    return origin_rect.width > 0.0f && origin_rect.height > 0.0f ? origin_rect : kFallbackOriginRect;
}

layout make_layout_for_targets(float anim_t,
                               Rectangle origin_rect,
                               Rectangle song_rect,
                               Rectangle main_rect,
                               Rectangle ranking_rect,
                               Rectangle jacket_rect,
                               Rectangle meta_rect,
                               Rectangle chart_detail_rect,
                               Rectangle chart_buttons_rect,
                               Rectangle ranking_header_rect,
                               Rectangle ranking_source_local_rect,
                               Rectangle ranking_source_online_rect,
                               Rectangle ranking_list_rect) {
    const float t = tween::ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);
    const Rectangle seed_song = centered_scaled_rect(origin, song_rect, 0.68f, kSeedSongOffset);
    const Rectangle seed_main = centered_scaled_rect(origin, main_rect, 0.74f, kSeedMainOffset);
    const Rectangle seed_ranking = centered_scaled_rect(origin, ranking_rect, 0.68f, kSeedRankingOffset);
    const Rectangle seed_back = centered_scaled_rect(origin, kPlayBackButtonRect, 0.9f, kSeedBackOffset);
    const Rectangle seed_jacket = centered_scaled_rect(origin, jacket_rect, 0.82f, kSeedJacketOffset);
    const Rectangle seed_meta = centered_scaled_rect(origin, meta_rect, 0.8f, kSeedMetaOffset);
    const Rectangle seed_chart_detail = centered_scaled_rect(origin, chart_detail_rect, 0.76f, kSeedChartDetailOffset);
    const Rectangle seed_chart_buttons = centered_scaled_rect(origin, chart_buttons_rect, 0.88f, kSeedChartButtonsOffset);
    const Rectangle seed_ranking_header = centered_scaled_rect(origin, ranking_header_rect, 0.7f, kSeedRankingHeaderOffset);
    const Rectangle seed_ranking_source_local = centered_scaled_rect(origin, ranking_source_local_rect, 0.8f, kSeedRankingSourceLocalOffset);
    const Rectangle seed_ranking_source_online = centered_scaled_rect(origin, ranking_source_online_rect, 0.8f, kSeedRankingSourceOnlineOffset);
    const Rectangle seed_ranking_list = centered_scaled_rect(origin, ranking_list_rect, 0.7f, kSeedRankingListOffset);
    return {tween::lerp(seed_back, kPlayBackButtonRect, t), tween::lerp(seed_song, song_rect, t),
            tween::lerp(seed_main, main_rect, t), tween::lerp(seed_ranking, ranking_rect, t),
            tween::lerp(seed_jacket, jacket_rect, t), tween::lerp(seed_meta, meta_rect, t),
            tween::lerp(seed_chart_detail, chart_detail_rect, t), tween::lerp(seed_chart_buttons, chart_buttons_rect, t),
            tween::lerp(seed_ranking_header, ranking_header_rect, t),
            tween::lerp(seed_ranking_source_local, ranking_source_local_rect, t),
            tween::lerp(seed_ranking_source_online, ranking_source_online_rect, t),
            tween::lerp(seed_ranking_list, ranking_list_rect, t)};
}

layout make_mode_layout(float anim_t, Rectangle origin_rect, mode view_mode) {
    const bool play = view_mode == mode::play;
    return make_layout_for_targets(
        anim_t, origin_rect,
        play ? kPlaySongColumnRect : kCreateSongColumnRect,
        play ? kPlayMainColumnRect : kCreateMainColumnRect,
        play ? kPlayRankingColumnRect : kCreateRankingColumnRect,
        play ? kPlayJacketRect : kCreateJacketRect,
        play ? kPlayMetaRect : kCreateMetaRect,
        play ? kPlayChartDetailRect : kCreateChartDetailRect,
        play ? kPlayChartButtonsRect : kCreateChartButtonsRect,
        play ? kPlayRankingHeaderRect : kCreateRankingHeaderRect,
        play ? kPlayRankingSourceLocalRect : kCreateRankingSourceLocalRect,
        play ? kPlayRankingSourceOnlineRect : kCreateRankingSourceOnlineRect,
        play ? kPlayRankingListRect : kCreateRankingListRect);
}

}  // namespace
title_play_view::update_result update(song_select::state& state, title_play_view::mode view_mode, float anim_t, Rectangle origin_rect, float dt) {
    update_result result;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    const float wheel = GetMouseWheelMove();
    const layout current = make_mode_layout(anim_t, origin_rect, view_mode);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const bool has_selection = song_select::selected_song(state) != nullptr &&
                               song_select::selected_chart_for(state, filtered) != nullptr;
    const bool play_song_menu_open =
        view_mode == mode::play &&
        state.context_menu.open &&
        state.context_menu.target == song_select::context_menu_target::song;
    const bool play_chart_menu_open =
        view_mode == mode::play &&
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

    if (ui::is_clicked(current.back_rect) || IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
        return result;
    }

    if (view_mode == mode::play) {
        const std::vector<int> song_indices = song_select::filtered_song_indices(state);
        if (!song_indices.empty() &&
            std::find(song_indices.begin(), song_indices.end(), state.selected_song_index) == song_indices.end()) {
            if (song_select::apply_song_selection(state, song_indices.front(), 0)) {
                result.song_selection_changed = true;
                return result;
            }
        }
    }

    if (view_mode == mode::play && left_pressed) {
        const song_select::chart_source_filter source_values[] = {
            song_select::chart_source_filter::all,
            song_select::chart_source_filter::official,
            song_select::chart_source_filter::community,
        };
        for (int index = 0; index < 3; ++index) {
            if (CheckCollisionPointRec(mouse, play_filter_source_button_rect(current.song_column, index))) {
                if (apply_play_filter_change(state, source_values[index],
                                             state.chart_key_filter, state.chart_min_level, state.chart_max_level, result)) {
                    return result;
                }
            }
        }

        const int key_values[] = {0, 4, 5, 6, 7};
        for (int index = 0; index < 5; ++index) {
            if (CheckCollisionPointRec(mouse, play_filter_key_button_rect(current.song_column, index))) {
                if (apply_play_filter_change(state, state.chart_source, key_values[index],
                                             state.chart_min_level, state.chart_max_level, result)) {
                    return result;
                }
            }
        }

        if (CheckCollisionPointRec(mouse, play_filter_clear_button_rect(current.song_column))) {
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

    if (view_mode == mode::play && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Rectangle slider = play_filter_level_slider_rect(current.song_column);
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

    if (view_mode == mode::play) {
        const song_select::song_entry* song = song_select::selected_song(state);
        const Rectangle progress = current.meta_rect;
        const Rectangle progress_hit = {progress.x, progress.y - 12.0f, progress.width, progress.height + 24.0f};
        if (song != nullptr && left_pressed && CheckCollisionPointRec(mouse, progress_hit)) {
            state.preview_bar_dragging = true;
            state.preview_bar_resume_after_drag = audio_manager::instance().is_preview_playing();
            state.preview_bar_drag_position_seconds = audio_manager::instance().get_preview_position_seconds();
            audio_manager::instance().pause_preview();
        }
        if (song != nullptr && state.preview_bar_dragging) {
            const double preview_length = selected_preview_length_seconds(song);
            if (preview_length > 0.0) {
                const float ratio = std::clamp((mouse.x - progress.x) / progress.width, 0.0f, 1.0f);
                state.preview_bar_drag_position_seconds = preview_length * static_cast<double>(ratio);
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                return result;
            }
            audio_manager::instance().seek_preview(state.preview_bar_drag_position_seconds);
            if (state.preview_bar_resume_after_drag) {
                audio_manager::instance().play_preview(false);
            }
            state.preview_bar_dragging = false;
            state.preview_bar_resume_after_drag = false;
        }
    }

    if (!state.play_search_input.active && IsKeyPressed(KEY_ENTER) && has_selection) {
        result.play_requested = true;
        return result;
    }

    if (view_mode == mode::play) {
        const song_select::song_entry* song = song_select::selected_song(state);
        const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
        if (left_pressed && has_selection && CheckCollisionPointRec(mouse, start_button_rect(current.ranking_column))) {
            result.play_requested = true;
            return result;
        }
        if (left_pressed && CheckCollisionPointRec(mouse, preview_play_button_rect(current))) {
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
            (song->status == content_status::update || song->status == content_status::modified);
        const bool chart_reinstall_available =
            chart != nullptr &&
            (chart->status == content_status::update || chart->status == content_status::modified);
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
            .source_local_rect = current.ranking_source_local_rect,
            .source_online_rect = current.ranking_source_online_rect,
            .list_rect = current.ranking_list_rect,
        };
        if (left_pressed) {
            const auto source = title_ranking_view::hit_test_source(ranking_config, mouse);
            if (source.has_value() && source.value() != state.ranking_panel.selected_source) {
                state.ranking_panel.selected_source = *source;
                result.ranking_source_changed = true;
                return result;
            }
        }
    } else {
        const update_result create_result =
            title_create_tools_view::update(state, current, left_pressed, mouse);
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
            create_result.edit_mv_requested ||
            create_result.manage_library_requested) {
            return create_result;
        }
    }

    if (left_pressed) {
        const auto clicked_chart =
            view_mode == mode::play
                ? title_song_list_view::hit_test_chart(
                      state, current.chart_buttons_rect, state.scroll_y, state.embedded_chart_scroll_y, mouse)
                : title_song_list_view::chart_hit{-1, title_center_view::hit_test_chart(
                                                          current.chart_buttons_rect, state.chart_scroll_y, mouse,
                                                          static_cast<int>(filtered.size()))};
        if (clicked_chart.chart_index >= 0) {
            if (state.difficulty_index == clicked_chart.chart_index) {
                result.play_requested = true;
            } else {
                state.difficulty_index = clicked_chart.chart_index;
                state.chart_change_anim_t = 1.0f;
                result.chart_selection_changed = true;
            }
            return result;
        }
    }

    if (right_pressed && view_mode == mode::play && !state.songs.empty()) {
        const auto clicked_chart =
            title_song_list_view::hit_test_chart(
                state, current.chart_buttons_rect, state.scroll_y, state.embedded_chart_scroll_y, mouse);
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

    if (right_pressed && view_mode == mode::play && !state.songs.empty()) {
        const int clicked_song =
            title_song_list_view::hit_test(state, current.chart_buttons_rect, state.scroll_y, mouse);
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
            view_mode == mode::play
                ? title_song_list_view::hit_test(state, current.chart_buttons_rect, state.scroll_y, mouse)
                : title_song_list_view::hit_test(current.song_column, state.scroll_y, mouse,
                                                 static_cast<int>(state.songs.size()));
        if (clicked_song >= 0) {
            if (view_mode == mode::play && clicked_song == state.selected_song_index) {
                state.selected_song_expanded = !state.selected_song_expanded;
                if (!state.selected_song_expanded) {
                    state.embedded_chart_scroll_y = 0.0f;
                    state.embedded_chart_scroll_y_target = 0.0f;
                }
            } else if (song_select::apply_song_selection(state, clicked_song, 0)) {
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

    const Rectangle song_scroll_area = view_mode == mode::play ? current.chart_buttons_rect : current.song_column;
    const Rectangle embedded_chart_list =
        view_mode == mode::play
            ? title_song_list_view::selected_chart_list_rect(state, current.chart_buttons_rect, state.scroll_y)
            : Rectangle{};
    if (view_mode == mode::play && CheckCollisionPointRec(mouse, embedded_chart_list) && wheel != 0.0f) {
        state.embedded_chart_scroll_y_target -= wheel * kWheelScrollStep;
    } else if (CheckCollisionPointRec(mouse, song_scroll_area) && wheel != 0.0f) {
        state.scroll_y_target -= wheel * kWheelScrollStep;
    } else if (view_mode != mode::play && CheckCollisionPointRec(mouse, current.chart_buttons_rect) && wheel != 0.0f) {
        state.chart_scroll_y_target -= wheel * kWheelScrollStep;
    } else if (view_mode == mode::play && CheckCollisionPointRec(mouse, current.ranking_list_rect) && wheel != 0.0f) {
        state.ranking_panel.scroll_y_target -= wheel * kWheelScrollStep;
    }

    state.scroll_y_target = std::clamp(
        state.scroll_y_target, 0.0f,
        view_mode == mode::play
            ? title_song_list_view::max_scroll(current.chart_buttons_rect, state)
            : title_song_list_view::max_scroll(current.song_column, static_cast<int>(state.songs.size())));
    state.scroll_y = tween::damp(state.scroll_y, state.scroll_y_target, dt, 12.0f, 0.5f);

    state.chart_scroll_y_target = std::clamp(
        state.chart_scroll_y_target, 0.0f,
        view_mode == mode::play
            ? 0.0f
            : title_center_view::max_chart_scroll(current.chart_buttons_rect, static_cast<int>(filtered.size())));
    state.chart_scroll_y = tween::damp(state.chart_scroll_y, state.chart_scroll_y_target, dt, 12.0f, 0.5f);

    state.embedded_chart_scroll_y_target = std::clamp(
        state.embedded_chart_scroll_y_target, 0.0f,
        view_mode == mode::play
            ? title_song_list_view::max_embedded_chart_scroll(state, current.chart_buttons_rect, state.scroll_y)
            : 0.0f);
    state.embedded_chart_scroll_y =
        tween::damp(state.embedded_chart_scroll_y, state.embedded_chart_scroll_y_target, dt, 12.0f, 0.5f);

    if (view_mode == mode::play) {
        state.ranking_panel.scroll_y_target = std::clamp(
            state.ranking_panel.scroll_y_target, 0.0f,
            title_ranking_view::max_scroll(current.ranking_list_rect, state.ranking_panel.listing));
        state.ranking_panel.scroll_y =
            tween::damp(state.ranking_panel.scroll_y, state.ranking_panel.scroll_y_target, dt, 12.0f, 0.5f);
    }

    return result;
}


}  // namespace title_local_song_select_controller
