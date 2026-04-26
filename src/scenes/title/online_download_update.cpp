#include "title/online_download_internal.h"

#include <algorithm>
#include <cmath>

#include "audio_manager.h"
#include "tween.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace title_online_view {
namespace {

void reset_chart_scroll(state& state) {
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
}

void reset_browse_scrolls(state& state) {
    state.song_scroll_y = 0.0f;
    state.song_scroll_y_target = 0.0f;
    reset_chart_scroll(state);
}

bool handle_back_or_close_input(state& state, update_result& result) {
    if (state.detail_open && (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        state.detail_open = false;
        reset_chart_scroll(state);
        return true;
    }
    if (!state.detail_open && (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        result.back_requested = true;
        return true;
    }
    return false;
}

bool switch_mode(state& state, catalog_mode new_mode, update_result& result) {
    if (state.mode == new_mode) {
        return false;
    }

    state.mode = new_mode;
    state.detail_open = false;
    reset_browse_scrolls(state);
    detail::ensure_selection_valid(state);
    result.song_selection_changed = true;
    return true;
}

bool handle_song_click(state& state,
                       Rectangle song_list_rect,
                       Vector2 mouse,
                       bool left_pressed,
                       update_result& result) {
    if (state.detail_open || !left_pressed) {
        return false;
    }

    const int clicked_song = detail::hit_test_song_list(state, song_list_rect, mouse);
    if (clicked_song < 0) {
        return false;
    }

    int& selected_song_index = detail::selected_song_index_ref(state);
    if (selected_song_index != clicked_song) {
        selected_song_index = clicked_song;
        detail::selected_chart_index_ref(state) = 0;
        reset_chart_scroll(state);
        result.song_selection_changed = true;
    } else {
        state.detail_open = true;
        request_charts_for_selected_song(state);
    }
    return true;
}

bool handle_chart_click(state& state,
                        Rectangle chart_list_rect,
                        Vector2 mouse,
                        bool left_pressed,
                        update_result& result) {
    if (!state.detail_open || !left_pressed) {
        return false;
    }

    const song_entry_state* song = selected_song(state);
    if (song == nullptr || !CheckCollisionPointRec(mouse, chart_list_rect)) {
        return false;
    }

    if (!state.download_in_progress) {
        for (int index = 0; index < static_cast<int>(song->charts.size()); ++index) {
            const chart_entry_state& chart = song->charts[static_cast<size_t>(index)];
            const Rectangle card = detail::chart_row_rect(chart_list_rect, index, state.chart_scroll_y);
            if (!detail::can_download_chart(*song, chart) ||
                !CheckCollisionPointRec(mouse, detail::chart_download_icon_rect(card))) {
                continue;
            }

            int& selected_chart_index = detail::selected_chart_index_ref(state);
            if (selected_chart_index != index) {
                selected_chart_index = index;
                result.chart_selection_changed = true;
            }
            result.action = requested_action::download_chart;
            return true;
        }
    }

    const int clicked_chart = detail::hit_test_chart_list(state, chart_list_rect, mouse);
    if (clicked_chart < 0) {
        return false;
    }

    int& selected_chart_index = detail::selected_chart_index_ref(state);
    if (selected_chart_index != clicked_chart) {
        selected_chart_index = clicked_chart;
        result.chart_selection_changed = true;
    }
    return true;
}

bool handle_detail_actions(state& state,
                           const layout& current,
                           Vector2 mouse,
                           bool left_pressed,
                           update_result& result) {
    if (!state.detail_open) {
        return false;
    }

    const song_entry_state* song = selected_song(state);
    if (song != nullptr && (!song->charts_loaded || song->charts_has_more)) {
        request_charts_for_selected_song(state);
    }

    if (song != nullptr && left_pressed && CheckCollisionPointRec(mouse, current.preview_bar_rect)) {
        const double preview_length = detail::preview_display_length_seconds(*song);
        if (preview_length > 0.0 && audio_manager::instance().is_preview_loaded()) {
            const float ratio = std::clamp((mouse.x - current.preview_bar_rect.x) / current.preview_bar_rect.width,
                                           0.0f, 1.0f);
            audio_manager::instance().seek_preview(preview_length * static_cast<double>(ratio));
        }
        return true;
    }

    if (ui::is_clicked(current.preview_play_rect)) {
        result.action = audio_manager::instance().is_preview_playing()
            ? requested_action::stop_preview
            : requested_action::restart_preview;
        return true;
    }

    const bool download_ready = song != nullptr &&
        (song->charts_loaded || (song->installed && !song->update_available));
    if (song != nullptr && !state.download_in_progress && download_ready &&
        ui::is_clicked(current.primary_action_rect)) {
        result.action = needs_download(*song)
            ? requested_action::primary
            : requested_action::open_local;
        return true;
    }

    return false;
}

void handle_keyboard_navigation(state& state, update_result& result) {
    if (state.search_input.active) {
        return;
    }

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
            reset_chart_scroll(state);
            result.song_selection_changed = true;
        }

        if (IsKeyPressed(KEY_ENTER) && selected_song(state) != nullptr) {
            state.detail_open = true;
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

void update_scroll_positions(state& state,
                             Rectangle song_list_rect,
                             Rectangle chart_list_rect,
                             Vector2 mouse,
                             float wheel,
                             float dt) {
    const int filtered_song_count = static_cast<int>(detail::filtered_indices(state).size());
    const song_entry_state* song = selected_song(state);
    const int chart_count = song != nullptr ? static_cast<int>(song->charts.size()) : 0;

    if (!state.detail_open && CheckCollisionPointRec(mouse, song_list_rect) && wheel != 0.0f) {
        state.song_scroll_y_target -= wheel * 54.0f;
    } else if (state.detail_open && CheckCollisionPointRec(mouse, chart_list_rect) && wheel != 0.0f) {
        state.chart_scroll_y_target -= wheel * 42.0f;
    }

    state.song_scroll_y_target = std::clamp(state.song_scroll_y_target, 0.0f,
                                            detail::max_song_scroll(song_list_rect, filtered_song_count));
    state.song_scroll_y = tween::damp(state.song_scroll_y, state.song_scroll_y_target, dt, 12.0f, 0.5f);
    if (!state.detail_open &&
        state.song_scroll_y_target >= std::max(0.0f, detail::max_song_scroll(song_list_rect, filtered_song_count) - 120.0f)) {
        request_next_song_page(state, state.mode);
    }

    state.chart_scroll_y_target = std::clamp(state.chart_scroll_y_target, 0.0f,
                                             detail::max_chart_scroll(chart_list_rect, chart_count));
    state.chart_scroll_y = tween::damp(state.chart_scroll_y, state.chart_scroll_y_target, dt, 12.0f, 0.5f);
    if (state.detail_open && song != nullptr && song->charts_has_more &&
        state.chart_scroll_y_target >= std::max(0.0f, detail::max_chart_scroll(chart_list_rect, chart_count) - 80.0f)) {
        request_charts_for_selected_song(state);
    }
}

}  // namespace

update_result update(state& state, float anim_t, Rectangle origin_rect, float dt) {
    update_result result;
    const layout current = make_layout(anim_t, origin_rect);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const float wheel = GetMouseWheelMove();
    const float detail_target = state.detail_open ? 1.0f : 0.0f;
    const float detail_lerp_speed = state.detail_open ? 6.5f : 10.0f;
    state.detail_transition = tween::damp(state.detail_transition, detail_target, dt, detail_lerp_speed, 0.002f);

    if (ui::is_clicked(current.back_rect)) {
        result.back_requested = true;
        return result;
    }

    if (handle_back_or_close_input(state, result)) {
        return result;
    }

    if (ui::is_clicked(current.official_tab_rect) && switch_mode(state, catalog_mode::official, result)) {
        return result;
    }
    if (ui::is_clicked(current.community_tab_rect) && switch_mode(state, catalog_mode::community, result)) {
        return result;
    }
    if (ui::is_clicked(current.owned_tab_rect) && switch_mode(state, catalog_mode::owned, result)) {
        return result;
    }

    detail::ensure_selection_valid(state);

    const Rectangle song_list_rect = current.song_grid_rect;

    if (handle_song_click(state, song_list_rect, mouse, left_pressed, result)) {
        return result;
    }

    if (handle_chart_click(state, current.chart_list_rect, mouse, left_pressed, result)) {
        return result;
    }

    if (handle_detail_actions(state, current, mouse, left_pressed, result)) {
        return result;
    }

    handle_keyboard_navigation(state, result);
    update_scroll_positions(state, song_list_rect, current.chart_list_rect, mouse, wheel, dt);

    return result;
}

}  // namespace title_online_view
