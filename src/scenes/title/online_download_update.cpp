#include "title/online_download_internal.h"

#include <algorithm>
#include <cmath>

#include "audio_manager.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace title_online_view {

update_result update(state& state, float anim_t, Rectangle origin_rect, float dt) {
    update_result result;
    const layout current = make_layout(anim_t, origin_rect);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const float wheel = GetMouseWheelMove();

    if (ui::is_clicked(current.back_rect)) {
        result.back_requested = true;
        return result;
    }
    if (state.detail_open && (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        state.detail_open = false;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        return result;
    }
    if (!state.detail_open && (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        result.back_requested = true;
        return result;
    }

    const auto switch_mode = [&](catalog_mode new_mode) -> bool {
        if (state.mode == new_mode) {
            return false;
        }
        state.mode = new_mode;
        state.detail_open = false;
        state.song_scroll_y = 0.0f;
        state.song_scroll_y_target = 0.0f;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
        detail::ensure_selection_valid(state);
        result.song_selection_changed = true;
        return true;
    };

    if (ui::is_clicked(current.official_tab_rect) && switch_mode(catalog_mode::official)) {
        return result;
    }
    if (ui::is_clicked(current.community_tab_rect) && switch_mode(catalog_mode::community)) {
        return result;
    }
    if (ui::is_clicked(current.owned_tab_rect) && switch_mode(catalog_mode::owned)) {
        return result;
    }

    detail::ensure_selection_valid(state);

    const Rectangle song_list_rect = current.song_grid_rect;

    if (!state.detail_open && left_pressed) {
        const int clicked_song = detail::hit_test_song_list(state, song_list_rect, mouse);
        if (clicked_song >= 0) {
            int& selected_song_index = detail::selected_song_index_ref(state);
            selected_song_index = clicked_song;
            detail::selected_chart_index_ref(state) = 0;
            state.chart_scroll_y = 0.0f;
            state.chart_scroll_y_target = 0.0f;
            state.detail_open = true;
            result.song_selection_changed = true;
            return result;
        }
    }

    if (state.detail_open && left_pressed) {
        const int clicked_chart = detail::hit_test_chart_list(state, current.chart_list_rect, mouse);
        if (clicked_chart >= 0) {
            int& selected_chart_index = detail::selected_chart_index_ref(state);
            if (selected_chart_index != clicked_chart) {
                selected_chart_index = clicked_chart;
                result.chart_selection_changed = true;
                return result;
            }
        }
    }

    if (state.detail_open) {
        if (ui::is_clicked(current.preview_play_rect)) {
            result.action = audio_manager::instance().is_preview_playing()
                ? requested_action::stop_preview
                : requested_action::restart_preview;
            return result;
        }
        if (selected_song(state) != nullptr && ui::is_clicked(current.primary_action_rect)) {
            result.action = requested_action::primary;
            return result;
        }
    }

    const bool allow_navigation_keys = !state.search_input.active;
    if (allow_navigation_keys) {
        const auto indices = detail::filtered_indices(state);
        int& selected_song_index = detail::selected_song_index_ref(state);
        int& selected_chart_index = detail::selected_chart_index_ref(state);

        if (!indices.empty() && !state.detail_open) {
            auto selected_it = std::find(indices.begin(), indices.end(), selected_song_index);
            int display_index = selected_it == indices.end() ? 0 : static_cast<int>(selected_it - indices.begin());

            int next_display_index = display_index;
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                next_display_index = std::max(0, display_index - 1);
            } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                next_display_index = std::min(static_cast<int>(indices.size()) - 1, display_index + 1);
            } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                next_display_index = std::max(0, display_index - detail::kSongGridColumns);
            } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                next_display_index = std::min(static_cast<int>(indices.size()) - 1,
                                              display_index + detail::kSongGridColumns);
            }

            if (next_display_index != display_index) {
                selected_song_index = indices[static_cast<size_t>(next_display_index)];
                selected_chart_index = 0;
                state.chart_scroll_y = 0.0f;
                state.chart_scroll_y_target = 0.0f;
                result.song_selection_changed = true;
            }

            if (IsKeyPressed(KEY_ENTER) && selected_song(state) != nullptr) {
                state.detail_open = true;
                return result;
            }
        }

        const song_entry_state* song = selected_song(state);
        if (state.detail_open && song != nullptr && !song->charts.empty()) {
            int next_chart_index = selected_chart_index;
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                next_chart_index = std::max(0, selected_chart_index - 1);
            } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                next_chart_index = std::min(static_cast<int>(song->charts.size()) - 1, selected_chart_index + 1);
            } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                next_chart_index = std::max(0, selected_chart_index - detail::kChartGridColumns);
            } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                next_chart_index = std::min(static_cast<int>(song->charts.size()) - 1,
                                            selected_chart_index + detail::kChartGridColumns);
            }
            if (next_chart_index != selected_chart_index) {
                selected_chart_index = next_chart_index;
                result.chart_selection_changed = true;
            }
        }
    }

    const int filtered_song_count = static_cast<int>(detail::filtered_indices(state).size());
    const song_entry_state* song = selected_song(state);
    const int chart_count = song != nullptr ? static_cast<int>(song->charts.size()) : 0;
    if (!state.detail_open && CheckCollisionPointRec(mouse, song_list_rect) && wheel != 0.0f) {
        state.song_scroll_y_target -= wheel * 54.0f;
    } else if (state.detail_open && CheckCollisionPointRec(mouse, current.chart_list_rect) && wheel != 0.0f) {
        state.chart_scroll_y_target -= wheel * 42.0f;
    }

    state.song_scroll_y_target = std::clamp(state.song_scroll_y_target, 0.0f,
                                            detail::max_song_scroll(song_list_rect, filtered_song_count));
    state.song_scroll_y += (state.song_scroll_y_target - state.song_scroll_y) * std::min(1.0f, dt * 12.0f);
    if (std::fabs(state.song_scroll_y - state.song_scroll_y_target) < 0.5f) {
        state.song_scroll_y = state.song_scroll_y_target;
    }

    state.chart_scroll_y_target = std::clamp(state.chart_scroll_y_target, 0.0f,
                                             detail::max_chart_scroll(current.chart_list_rect, chart_count));
    state.chart_scroll_y += (state.chart_scroll_y_target - state.chart_scroll_y) * std::min(1.0f, dt * 12.0f);
    if (std::fabs(state.chart_scroll_y - state.chart_scroll_y_target) < 0.5f) {
        state.chart_scroll_y = state.chart_scroll_y_target;
    }

    return result;
}

}  // namespace title_online_view
