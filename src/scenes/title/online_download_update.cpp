#include "title/online_download_internal.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "content_lifecycle.h"
#include "services/content_sync_service.h"
#include "song_select/song_select_level_filter.h"
#include "title/online_catalog_data_controller.h"
#include "title/online_download_preview_controller.h"
#include "tween.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_notice.h"
#include "ui_scroll.h"
#include "virtual_screen.h"

namespace title_online_view {
namespace {

constexpr float kChartFilterMinLevel = song_select::level_filter::kMinLevel;
constexpr float kChartFilterMaxLevel = song_select::level_filter::kMaxLevel;

void reset_chart_scroll(state& state) {
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
    state.preview_bar_dragging = false;
    state.preview_bar_resume_after_drag = false;
    state.preview_bar_drag_position_seconds = 0.0;
}

void reset_browse_scrolls(state& state) {
    state.song_scroll_y = 0.0f;
    state.song_scroll_y_target = 0.0f;
    reset_chart_scroll(state);
}

bool handle_back_or_close_input(state& state, update_result& result) {
    if (state.detail_open && ui::is_cancel_pressed()) {
        state.detail_open = false;
        state.preview_bar_dragging = false;
        state.preview_bar_resume_after_drag = false;
        state.preview_bar_drag_position_seconds = 0.0;
        reset_chart_scroll(state);
        return true;
    }
    if (!state.detail_open && ui::is_cancel_pressed()) {
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

void clear_chart_filters(state& state) {
    state.chart_search_input.value.clear();
    state.chart_search_input.cursor = 0;
    state.min_level_input.value.clear();
    state.min_level_input.cursor = 0;
    state.max_level_input.value.clear();
    state.max_level_input.cursor = 0;
    state.chart_level_filter_dragging = false;
    state.chart_level_filter_dragging_min = false;
    state.chart_source = chart_source_filter::all;
    state.chart_key_filter = 0;
    state.chart_download_filter = 0;
    reset_chart_scroll(state);
}

struct chart_filter_interaction {
    bool clear = false;
    std::optional<chart_source_filter> source;
    std::optional<int> key_filter;
    std::optional<int> download_filter;
};

chart_filter_interaction chart_source_or_clear_interaction_for(Rectangle filter_panel) {
    for (int index = 0; index < static_cast<int>(detail::kChartSourceFilters.size()); ++index) {
        const detail::chart_source_filter_option& option =
            detail::kChartSourceFilters[static_cast<size_t>(index)];
        if (ui::is_clicked(detail::chart_source_button_rect(filter_panel, index))) {
            return {.source = option.value};
        }
    }
    if (ui::is_clicked(detail::chart_clear_button_rect(filter_panel))) {
        return {.clear = true};
    }
    return {};
}

chart_filter_interaction chart_key_or_status_interaction_for(Rectangle filter_panel) {
    for (int index = 0; index < static_cast<int>(detail::kChartKeyFilters.size()); ++index) {
        const detail::chart_key_filter_option& option =
            detail::kChartKeyFilters[static_cast<size_t>(index)];
        if (ui::is_clicked(detail::chart_key_button_rect(filter_panel, index))) {
            return {.key_filter = option.value};
        }
    }
    for (int index = 0; index < static_cast<int>(detail::kChartStatusFilters.size()); ++index) {
        const detail::chart_status_filter_option& option =
            detail::kChartStatusFilters[static_cast<size_t>(index)];
        if (ui::is_clicked(detail::chart_status_button_rect(filter_panel, index))) {
            return {.download_filter = option.value};
        }
    }
    return {};
}

bool apply_chart_filter_interaction(state& state, const chart_filter_interaction& interaction) {
    if (interaction.clear) {
        clear_chart_filters(state);
        return true;
    }

    bool handled = false;
    if (interaction.source.has_value()) {
        state.chart_source = *interaction.source;
        handled = true;
    }
    if (interaction.key_filter.has_value()) {
        state.chart_key_filter = *interaction.key_filter;
        handled = true;
    }
    if (interaction.download_filter.has_value()) {
        state.chart_download_filter = *interaction.download_filter;
        handled = true;
    }
    if (handled) {
        reset_chart_scroll(state);
    }
    return handled;
}

float chart_level_value(const std::string& value, float fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        size_t parsed = 0;
        const float result = std::stof(value, &parsed);
        if (parsed == value.size()) {
            return std::clamp(result, kChartFilterMinLevel, kChartFilterMaxLevel);
        }
    } catch (...) {
    }
    return fallback;
}

void set_level_input(ui::text_input_state& input, float value, float default_value) {
    const float clamped = std::clamp(value, kChartFilterMinLevel, kChartFilterMaxLevel);
    if (std::fabs(clamped - default_value) <= 0.05f) {
        input.value.clear();
    } else if (clamped >= kChartFilterMaxLevel - 0.05f) {
        input.value = TextFormat("%.0f", kChartFilterMaxLevel);
    } else {
        input.value = TextFormat("%.1f", clamped);
    }
    input.cursor = ui::utf8_codepoint_count(input.value);
    input.has_selection = false;
    input.selection_anchor = input.cursor;
}

bool update_level_range_from_slider(state& state, Rectangle chart_list, Vector2 mouse) {
    const Rectangle slider = detail::chart_level_slider_rect(chart_list);
    if (ui::is_mouse_button_released()) {
        state.chart_level_filter_dragging = false;
    }
    float min_level = chart_level_value(state.min_level_input.value, kChartFilterMinLevel);
    float max_level = chart_level_value(state.max_level_input.value, kChartFilterMaxLevel);
    if (min_level > max_level) {
        std::swap(min_level, max_level);
    }

    const Rectangle min_chip = song_select::level_filter::chip_rect(slider, min_level);
    const Rectangle max_chip = song_select::level_filter::chip_rect(slider, max_level);
    if (ui::is_mouse_button_pressed()) {
        if (ui::contains_point(max_chip, mouse)) {
            state.chart_level_filter_dragging = true;
            state.chart_level_filter_dragging_min = false;
        } else if (ui::contains_point(min_chip, mouse)) {
            state.chart_level_filter_dragging = true;
            state.chart_level_filter_dragging_min = true;
        }
    }
    if (!ui::is_mouse_button_down() || !state.chart_level_filter_dragging) {
        return false;
    }

    const float value = song_select::level_filter::level_from_t(
        std::clamp((mouse.x - slider.x) / slider.width, 0.0f, 1.0f));
    if (state.chart_level_filter_dragging_min) {
        min_level = std::min(value, max_level);
    } else {
        max_level = std::max(value, min_level);
    }
    set_level_input(state.min_level_input, min_level, kChartFilterMinLevel);
    set_level_input(state.max_level_input, max_level, kChartFilterMaxLevel);
    reset_chart_scroll(state);
    return true;
}

bool switch_discovery_view(state& state, discovery_view view, update_result& result) {
    if (state.view == view) {
        return false;
    }
    state.view = view;
    state.detail_open = false;
    detail::rebuild_visible_discovery_songs(state);
    detail::ensure_selection_valid(state);
    result.song_selection_changed = true;
    return true;
}

bool switch_source_filter(state& state,
                          online_catalog::data_controller& data_controller,
                          source_filter source,
                          update_result& result) {
    if (state.source == source) {
        return false;
    }
    state.source = source;
    state.view = discovery_view::overview;
    state.detail_open = false;
    reset_browse_scrolls(state);
    reload_catalog(state, data_controller);
    result.song_selection_changed = true;
    return true;
}

bool handle_sidebar_clicks(state& state,
                           online_catalog::data_controller& data_controller,
                           const layout& current,
                           update_result& result) {
    if (state.detail_open || !ui::is_mouse_button_released()) {
        return false;
    }

    for (int index = 0; index < static_cast<int>(detail::kDiscoveryViews.size()); ++index) {
        if (ui::is_clicked(detail::sidebar_button_rect(current.sidebar_rect, index))) {
            return switch_discovery_view(state, detail::kDiscoveryViews[static_cast<size_t>(index)], result);
        }
    }

    for (int index = 0; index < static_cast<int>(detail::kSourceFilters.size()); ++index) {
        if (ui::is_clicked(detail::source_button_rect(current.sidebar_rect, index))) {
            return switch_source_filter(state, data_controller, detail::kSourceFilters[static_cast<size_t>(index)], result);
        }
    }
    return false;
}

bool handle_song_click(state& state,
                       online_catalog::data_controller& data_controller,
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
        request_charts_for_selected_song(state, data_controller);
    }
    return true;
}

bool handle_overview_shelf_paging(state& state,
                                  Rectangle song_list_rect,
                                  Vector2 mouse,
                                  bool left_pressed,
                                  update_result& result) {
    if (state.detail_open || !left_pressed || !detail::uses_overview_shelves(state)) {
        return false;
    }

    const std::vector<detail::overview_shelf_row> rows = detail::overview_shelf_rows(state);
    for (int shelf_row = 0; shelf_row < static_cast<int>(rows.size()); ++shelf_row) {
        const detail::overview_shelf_row& row = rows[static_cast<size_t>(shelf_row)];
        if (row.total_count <= detail::kSongGridColumns) {
            continue;
        }

        const float max_offset = static_cast<float>(std::max(0, row.total_count - detail::kSongGridColumns));
        const auto target_it = state.overview_shelf_scroll_x_target.find(row.key);
        const float target = target_it == state.overview_shelf_scroll_x_target.end() ? row.scroll_x : target_it->second;

        if (ui::contains_point(detail::overview_shelf_prev_button_rect(
                                   song_list_rect, shelf_row, state.song_scroll_y),
                               mouse)) {
            state.overview_shelf_scroll_x_target[row.key] = std::max(0.0f, target - 1.0f);
            result.song_selection_changed = true;
            return true;
        }

        if (ui::contains_point(detail::overview_shelf_next_button_rect(
                                   song_list_rect, shelf_row, state.song_scroll_y),
                               mouse)) {
            state.overview_shelf_scroll_x_target[row.key] = std::min(max_offset, target + 1.0f);
            result.song_selection_changed = true;
            return true;
        }
    }
    return false;
}

void update_overview_shelf_scrolls(state& state, float dt) {
    if (!detail::uses_overview_shelves(state)) {
        return;
    }

    for (const detail::overview_shelf_row& row : detail::overview_shelf_rows(state)) {
        const float max_offset = static_cast<float>(std::max(0, row.total_count - detail::kSongGridColumns));
        float& target = state.overview_shelf_scroll_x_target[row.key];
        target = std::clamp(target, 0.0f, max_offset);
        float& current = state.overview_shelf_scroll_x[row.key];
        current = tween::damp(current, target, dt, 14.0f, 0.001f);
    }
}

bool chart_list_accepts_click(const state& state,
                              const song_entry_state* song,
                              Rectangle chart_list_rect,
                              Vector2 mouse,
                              bool left_pressed) {
    return state.detail_open &&
           left_pressed &&
           song != nullptr &&
           ui::contains_point(chart_list_rect, mouse);
}

bool clicked_downloadable_chart_icon(const song_entry_state& song,
                                     const chart_entry_state& chart,
                                     Rectangle chart_card,
                                     Vector2 mouse) {
    return detail::can_download_chart(song, chart) &&
           ui::contains_point(detail::chart_download_icon_rect(chart_card), mouse);
}

bool handle_chart_click(state& state,
                        Rectangle chart_list_rect,
                        Vector2 mouse,
                        bool left_pressed,
                        update_result& result) {
    const song_entry_state* song = selected_song(state);
    if (!chart_list_accepts_click(state, song, chart_list_rect, mouse, left_pressed)) {
        return false;
    }

    if (!state.download_in_progress) {
        const auto chart_indices = detail::filtered_chart_indices(state);
        const ui::index_range visible_charts =
            detail::visible_chart_range(chart_list_rect, static_cast<int>(chart_indices.size()), state.chart_scroll_y);
        for (int display_index = visible_charts.begin; display_index < visible_charts.end; ++display_index) {
            const int index = chart_indices[static_cast<size_t>(display_index)];
            const chart_entry_state& chart = song->charts[static_cast<size_t>(index)];
            const Rectangle card = detail::chart_row_rect(chart_list_rect, display_index, state.chart_scroll_y);
            if (!clicked_downloadable_chart_icon(*song, chart, card, mouse)) {
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
                            online_catalog::data_controller& data_controller,
                            const layout& current,
                            title_audio_controller& audio_controller,
                            Vector2 mouse,
                            bool left_pressed,
                            update_result& result) {
    if (!state.detail_open) {
        return false;
    }

    const song_entry_state* song = selected_song(state);
    if (song != nullptr && (!song->charts_loaded || song->charts_has_more)) {
        request_charts_for_selected_song(state, data_controller);
    }

    if (preview_controller::update_scrub(state, song, audio_controller, current.preview_bar_rect, mouse, left_pressed)) {
        return true;
    }

    if (ui::is_clicked(current.preview_play_rect)) {
        result.action = preview_controller::toggle_playback_action(song, audio_controller);
        return true;
    }

    const Rectangle filter_panel = current.detail_left_rect;
    if (apply_chart_filter_interaction(state, chart_source_or_clear_interaction_for(filter_panel))) {
        return true;
    }
    if (update_level_range_from_slider(state, filter_panel, mouse)) {
        return true;
    }
    if (apply_chart_filter_interaction(state, chart_key_or_status_interaction_for(filter_panel))) {
        return true;
    }

    if (song != nullptr && !state.download_in_progress && ui::is_clicked(current.primary_action_rect)) {
        const chart_entry_state* chart = selected_chart(state);
        if (song->song.online_identity.has_value() &&
            !content_lifecycle::lifecycle_is_active(song->song.online_identity->lifecycle_status)) {
            const std::string label = content_lifecycle::display_label(
                song->song.online_identity->review_status,
                song->song.online_identity->lifecycle_status);
            ui::notify(label.empty() ? "This song is not available." : label,
                       ui::notice_tone::error, 2.4f);
        } else if (needs_download(*song)) {
            result.action = requested_action::primary;
        } else if (chart != nullptr &&
                   chart->installed &&
                   (chart->update_available || content_sync_service::is_modified(chart->chart.sync_state))) {
            result.action = requested_action::download_chart;
        } else if (content_unlock_is_locked(song->song.song.meta.extra.unlock) ||
                   (chart != nullptr && content_unlock_is_locked(chart->chart.meta.extra.unlock))) {
            const std::string reason = chart != nullptr
                ? content_play_lock_reason(song->song.song.meta, chart->chart.meta)
                : content_unlock_reason_or_default(song->song.song.meta.extra.unlock);
            ui::notify(reason, ui::notice_tone::error, 2.4f);
        } else {
            result.action = requested_action::open_local;
        }
        return true;
    }

    return false;
}

bool handle_preview_panel_actions(state& state,
                                  online_catalog::data_controller& data_controller,
                                  const layout& current,
                                  title_audio_controller& audio_controller,
                                  Vector2 mouse,
                                  bool left_pressed,
                                  update_result& result) {
    if (state.detail_open) {
        return false;
    }
    const song_entry_state* song = selected_song(state);
    if (song == nullptr) {
        return false;
    }

    const Rectangle bar = detail::preview_panel_progress_rect(current.preview_panel_rect);
    if (preview_controller::update_scrub(state, song, audio_controller, bar, mouse, left_pressed)) {
        return true;
    }

    if (ui::is_clicked(detail::preview_panel_play_button_rect(current.preview_panel_rect))) {
        result.action = preview_controller::toggle_playback_action(song, audio_controller);
        return true;
    }
    if (ui::is_clicked(detail::preview_panel_prev_button_rect(current.preview_panel_rect)) ||
        ui::is_clicked(detail::preview_panel_next_button_rect(current.preview_panel_rect))) {
        const auto indices = detail::filtered_indices(state);
        int& selected_song_index = detail::selected_song_index_ref(state);
        const auto selected_it = std::find(indices.begin(), indices.end(), selected_song_index);
        if (!indices.empty() && selected_it != indices.end()) {
            const int display_index = static_cast<int>(selected_it - indices.begin());
            const int delta = ui::is_clicked(detail::preview_panel_next_button_rect(current.preview_panel_rect)) ? 1 : -1;
            const int next_display_index =
                std::clamp(display_index + delta, 0, static_cast<int>(indices.size()) - 1);
            if (next_display_index != display_index) {
                selected_song_index = indices[static_cast<size_t>(next_display_index)];
                detail::selected_chart_index_ref(state) = 0;
                result.song_selection_changed = true;
            }
        }
        return true;
    }
    if (ui::is_clicked(detail::preview_panel_open_button_rect(current.preview_panel_rect))) {
        state.detail_open = true;
        request_charts_for_selected_song(state, data_controller);
        return true;
    }
    return false;
}

void handle_keyboard_navigation(state& state,
                                online_catalog::data_controller& data_controller,
                                update_result& result) {
    if (state.search_input.active || state.chart_search_input.active ||
        state.min_level_input.active || state.max_level_input.active) {
        return;
    }

    const auto indices = detail::filtered_indices(state);
    int& selected_song_index = detail::selected_song_index_ref(state);
    int& selected_chart_index = detail::selected_chart_index_ref(state);

    if (!indices.empty() && !state.detail_open) {
        auto selected_it = std::find(indices.begin(), indices.end(), selected_song_index);
        int display_index = selected_it == indices.end() ? 0 : static_cast<int>(selected_it - indices.begin());

        int next_display_index = display_index;
        if (ui::is_left_pressed()) {
            next_display_index = std::max(0, display_index - 1);
        } else if (ui::is_right_pressed()) {
            next_display_index = std::min(static_cast<int>(indices.size()) - 1, display_index + 1);
        } else if (ui::is_up_pressed()) {
            next_display_index = std::max(0, display_index - detail::kSongGridColumns);
        } else if (ui::is_down_pressed()) {
            next_display_index = std::min(static_cast<int>(indices.size()) - 1,
                                          display_index + detail::kSongGridColumns);
        }

        if (next_display_index != display_index) {
            selected_song_index = indices[static_cast<size_t>(next_display_index)];
            selected_chart_index = 0;
            reset_chart_scroll(state);
            result.song_selection_changed = true;
        }

        if (ui::is_enter_pressed() && selected_song(state) != nullptr) {
            state.detail_open = true;
            request_charts_for_selected_song(state, data_controller);
        }
    }

    const song_entry_state* song = selected_song(state);
    const auto chart_indices = detail::filtered_chart_indices(state);
    if (state.detail_open && song != nullptr && !chart_indices.empty()) {
        auto selected_it = std::find(chart_indices.begin(), chart_indices.end(), selected_chart_index);
        int display_index = selected_it == chart_indices.end() ? 0 : static_cast<int>(selected_it - chart_indices.begin());

        int next_display_index = display_index;
        if (ui::is_left_pressed()) {
            next_display_index = std::max(0, display_index - 1);
        } else if (ui::is_right_pressed()) {
            next_display_index = std::min(static_cast<int>(chart_indices.size()) - 1, display_index + 1);
        } else if (ui::is_up_pressed()) {
            next_display_index = std::max(0, display_index - detail::kChartGridColumns);
        } else if (ui::is_down_pressed()) {
            next_display_index = std::min(static_cast<int>(chart_indices.size()) - 1,
                                          display_index + detail::kChartGridColumns);
        }
        if (next_display_index != display_index) {
            selected_chart_index = chart_indices[static_cast<size_t>(next_display_index)];
            result.chart_selection_changed = true;
        }
    }
}

void update_scroll_positions(state& state,
                             online_catalog::data_controller& data_controller,
                             Rectangle song_list_rect,
                             Rectangle chart_list_rect,
                             Vector2 mouse,
                             float wheel,
                             float dt) {
    const int filtered_song_count = static_cast<int>(detail::filtered_indices(state).size());
    const song_entry_state* song = selected_song(state);
    const int chart_count = static_cast<int>(detail::filtered_chart_indices(state).size());

    if (!state.detail_open && ui::contains_point(song_list_rect, mouse) && wheel != 0.0f) {
        state.song_scroll_y_target =
            ui::wheel_scrolled_target(state.song_scroll_y_target, wheel, 54.0f);
    } else if (state.detail_open && ui::contains_point(chart_list_rect, mouse) && wheel != 0.0f) {
        state.chart_scroll_y_target =
            ui::wheel_scrolled_target(state.chart_scroll_y_target, wheel, 42.0f);
    }

    state.song_scroll_y_target = ui::clamp_scroll_offset(
        state.song_scroll_y_target,
        detail::max_song_scroll(state, song_list_rect, filtered_song_count));
    state.song_scroll_y = tween::damp(state.song_scroll_y, state.song_scroll_y_target, dt, 12.0f, 0.5f);
    if (!state.detail_open &&
        state.song_scroll_y_target >= std::max(0.0f, detail::max_song_scroll(state, song_list_rect, filtered_song_count) - 120.0f)) {
        request_next_song_page(state, data_controller, state.mode);
    }

    state.chart_scroll_y_target = ui::clamp_scroll_offset(
        state.chart_scroll_y_target,
        detail::max_chart_scroll(chart_list_rect, chart_count));
    state.chart_scroll_y = tween::damp(state.chart_scroll_y, state.chart_scroll_y_target, dt, 12.0f, 0.5f);
    if (state.detail_open && song != nullptr && song->charts_has_more &&
        state.chart_scroll_y_target >= std::max(0.0f, detail::max_chart_scroll(chart_list_rect, chart_count) - 80.0f)) {
        request_charts_for_selected_song(state, data_controller);
    }
}

}  // namespace

update_result update(state& state,
                     online_catalog::data_controller& data_controller,
                     title_audio_controller& audio_controller,
                     float anim_t,
                     Rectangle origin_rect,
                     float dt) {
    update_result result;
    const layout current = make_layout(anim_t, origin_rect);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = ui::is_mouse_button_pressed();
    const float wheel = ui::mouse_wheel_move();
    state.jackets.poll();
    const float detail_target = state.detail_open ? 1.0f : 0.0f;
    const float detail_lerp_speed = state.detail_open ? 6.5f : 10.0f;
    state.detail_transition = tween::damp(state.detail_transition, detail_target, dt, detail_lerp_speed, 0.002f);
    update_overview_shelf_scrolls(state, dt);

    if (state.detail_open && ui::is_clicked(current.back_rect)) {
        state.detail_open = false;
        state.preview_bar_dragging = false;
        state.preview_bar_resume_after_drag = false;
        state.preview_bar_drag_position_seconds = 0.0;
        reset_chart_scroll(state);
        return result;
    }

    if (ui::is_clicked(current.back_rect)) {
        result.back_requested = true;
        return result;
    }

    if (handle_back_or_close_input(state, result)) {
        return result;
    }

    if (handle_sidebar_clicks(state, data_controller, current, result)) {
        return result;
    }

    detail::ensure_selection_valid(state);
    const Rectangle song_list_rect = current.song_grid_rect;

    if (handle_overview_shelf_paging(state, song_list_rect, mouse, left_pressed, result)) {
        return result;
    }

    if (handle_song_click(state, data_controller, song_list_rect, mouse, left_pressed, result)) {
        return result;
    }

    if (handle_chart_click(state, current.chart_list_rect, mouse, left_pressed, result)) {
        return result;
    }

    if (handle_detail_actions(state, data_controller, current, audio_controller, mouse, left_pressed, result)) {
        return result;
    }

    if (handle_preview_panel_actions(state, data_controller, current, audio_controller, mouse, left_pressed, result)) {
        return result;
    }

    handle_keyboard_navigation(state, data_controller, result);
    update_scroll_positions(state, data_controller, song_list_rect, current.chart_list_rect, mouse, wheel, dt);

    return result;
}

}  // namespace title_online_view
