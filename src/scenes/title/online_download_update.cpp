#include "title/online_download_internal.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "content_lifecycle.h"
#include "services/content_sync_service.h"
#include "title/online_catalog_data_controller.h"
#include "title/online_download_preview_controller.h"
#include "tween.h"
#include "ui_draw.h"
#include "ui_notice.h"
#include "virtual_screen.h"

namespace title_online_view {
namespace {

constexpr float kChartLevelWidth = 220.0f;
constexpr float kChartKeyButtonWidth = 44.0f;
constexpr float kChartKeyButtonStep = 50.0f;
constexpr float kChartFilterMinLevel = 0.0f;
constexpr float kChartFilterUsefulMaxLevel = 15.0f;
constexpr float kChartFilterMaxLevel = 99.0f;
constexpr float kChartFilterUsefulTrack = 0.97f;

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
    return std::round(level * 10.0f) / 10.0f;
}

Rectangle level_filter_chip_rect(Rectangle range, float level) {
    const float x = range.x + range.width * level_filter_t(level);
    return {x - 24.0f, range.y - 4.0f, 48.0f, 28.0f};
}

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
    if (state.detail_open && (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        state.detail_open = false;
        state.preview_bar_dragging = false;
        state.preview_bar_resume_after_drag = false;
        state.preview_bar_drag_position_seconds = 0.0;
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

Rectangle preview_open_button_rect(Rectangle panel) {
    constexpr float kPreviewPanelInset = 24.0f;
    constexpr float kPreviewOpenButtonBottom = 28.0f;
    constexpr float kPreviewOpenButtonHeight = 58.0f;
    return {
        panel.x + kPreviewPanelInset,
        panel.y + panel.height - kPreviewOpenButtonBottom - kPreviewOpenButtonHeight,
        panel.width - kPreviewPanelInset * 2.0f,
        kPreviewOpenButtonHeight,
    };
}

Rectangle preview_play_button_rect(Rectangle panel) {
    constexpr float kPreviewPlayY = 400.0f;
    constexpr float kPreviewPlayWidth = 116.0f;
    constexpr float kPreviewPlayHeight = 54.0f;
    return {
        panel.x + panel.width * 0.5f - kPreviewPlayWidth * 0.5f,
        panel.y + kPreviewPlayY,
        kPreviewPlayWidth,
        kPreviewPlayHeight,
    };
}

Rectangle preview_prev_button_rect(Rectangle panel) {
    constexpr float kPreviewPlayY = 400.0f;
    constexpr float kPreviewButtonWidth = 90.0f;
    constexpr float kPreviewButtonHeight = 54.0f;
    constexpr float kPreviewButtonGap = 8.0f;
    return {
        panel.x + panel.width * 0.5f - 58.0f - kPreviewButtonGap - kPreviewButtonWidth,
        panel.y + kPreviewPlayY,
        kPreviewButtonWidth,
        kPreviewButtonHeight,
    };
}

Rectangle preview_next_button_rect(Rectangle panel) {
    constexpr float kPreviewPlayY = 400.0f;
    constexpr float kPreviewButtonWidth = 90.0f;
    constexpr float kPreviewButtonHeight = 54.0f;
    constexpr float kPreviewButtonGap = 8.0f;
    return {
        panel.x + panel.width * 0.5f + 58.0f + kPreviewButtonGap,
        panel.y + kPreviewPlayY,
        kPreviewButtonWidth,
        kPreviewButtonHeight,
    };
}

Rectangle preview_bar_rect(Rectangle panel) {
    constexpr float kPreviewPanelInset = 24.0f;
    constexpr float kPreviewBarY = 468.0f;
    constexpr float kPreviewBarHeight = 12.0f;
    return {
        panel.x + kPreviewPanelInset,
        panel.y + kPreviewBarY,
        panel.width - kPreviewPanelInset * 2.0f,
        kPreviewBarHeight,
    };
}

Rectangle chart_filter_button_rect(Rectangle chart_list, int index) {
    return {
        chart_list.x + static_cast<float>(index) * 74.0f,
        chart_list.y - 78.0f,
        66.0f,
        28.0f,
    };
}

Rectangle chart_source_button_rect(Rectangle chart_list, int index) {
    const float button_width = (chart_list.width - 52.0f) * 0.5f;
    const float x = chart_list.x + 20.0f + static_cast<float>(index % 2) * (button_width + 12.0f);
    const float y = chart_list.y + 124.0f + static_cast<float>(index / 2) * 42.0f;
    return {
        x,
        y,
        button_width,
        36.0f,
    };
}

Rectangle chart_key_button_rect(Rectangle chart_list, int index) {
    const float group_width = kChartKeyButtonWidth + kChartKeyButtonStep * 4.0f;
    return {
        chart_list.x + (chart_list.width - group_width) * 0.5f + static_cast<float>(index) * kChartKeyButtonStep,
        chart_list.y + 470.0f,
        kChartKeyButtonWidth,
        30.0f,
    };
}

Rectangle chart_status_button_rect(Rectangle chart_list, int index) {
    return {
        chart_list.x + 20.0f + static_cast<float>(index) * ((chart_list.width - 52.0f) / 3.0f + 6.0f),
        chart_list.y + 262.0f,
        (chart_list.width - 52.0f) / 3.0f,
        36.0f,
    };
}

Rectangle chart_clear_button_rect(Rectangle chart_list) {
    return {
        chart_list.x + 20.0f,
        chart_list.y + chart_list.height - 64.0f,
        chart_list.width - 40.0f,
        42.0f,
    };
}

Rectangle chart_level_min_input_rect(Rectangle chart_list) {
    const float group_x = chart_list.x + (chart_list.width - 188.0f) * 0.5f;
    return {group_x, chart_list.y + 358.0f, 66.0f, 30.0f};
}

Rectangle chart_level_max_input_rect(Rectangle chart_list) {
    const float group_x = chart_list.x + (chart_list.width - 188.0f) * 0.5f;
    return {group_x + 122.0f, chart_list.y + 358.0f, 66.0f, 30.0f};
}

Rectangle chart_level_slider_rect(Rectangle chart_list) {
    return {chart_list.x + (chart_list.width - kChartLevelWidth) * 0.5f, chart_list.y + 372.0f, kChartLevelWidth, 24.0f};
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
    const Rectangle slider = chart_level_slider_rect(chart_list);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        state.chart_level_filter_dragging = false;
    }
    float min_level = chart_level_value(state.min_level_input.value, kChartFilterMinLevel);
    float max_level = chart_level_value(state.max_level_input.value, kChartFilterMaxLevel);
    if (min_level > max_level) {
        std::swap(min_level, max_level);
    }

    const Rectangle min_chip = level_filter_chip_rect(slider, min_level);
    const Rectangle max_chip = level_filter_chip_rect(slider, max_level);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, max_chip)) {
            state.chart_level_filter_dragging = true;
            state.chart_level_filter_dragging_min = false;
        } else if (CheckCollisionPointRec(mouse, min_chip)) {
            state.chart_level_filter_dragging = true;
            state.chart_level_filter_dragging_min = true;
        }
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) || !state.chart_level_filter_dragging) {
        return false;
    }

    const float value = level_from_filter_t(std::clamp((mouse.x - slider.x) / slider.width, 0.0f, 1.0f));
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
    if (state.detail_open || !IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        return false;
    }

    if (ui::is_clicked(detail::sidebar_button_rect(current.sidebar_rect, 0))) {
        return switch_discovery_view(state, discovery_view::overview, result);
    }
    if (ui::is_clicked(detail::sidebar_button_rect(current.sidebar_rect, 1))) {
        return switch_discovery_view(state, discovery_view::new_arrivals, result);
    }
    if (ui::is_clicked(detail::sidebar_button_rect(current.sidebar_rect, 2))) {
        return switch_discovery_view(state, discovery_view::rising, result);
    }
    if (ui::is_clicked(detail::sidebar_button_rect(current.sidebar_rect, 3))) {
        return switch_discovery_view(state, discovery_view::hidden_gems, result);
    }
    if (ui::is_clicked(detail::sidebar_button_rect(current.sidebar_rect, 4))) {
        return switch_discovery_view(state, discovery_view::recommended, result);
    }
    if (ui::is_clicked(detail::sidebar_button_rect(current.sidebar_rect, 5))) {
        return switch_discovery_view(state, discovery_view::needs_charts, result);
    }

    if (ui::is_clicked(detail::source_button_rect(current.sidebar_rect, 0))) {
        return switch_source_filter(state, data_controller, source_filter::all, result);
    }
    if (ui::is_clicked(detail::source_button_rect(current.sidebar_rect, 1))) {
        return switch_source_filter(state, data_controller, source_filter::official, result);
    }
    if (ui::is_clicked(detail::source_button_rect(current.sidebar_rect, 2))) {
        return switch_source_filter(state, data_controller, source_filter::community, result);
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

        if (CheckCollisionPointRec(mouse, detail::overview_shelf_prev_button_rect(
                                              song_list_rect, shelf_row, state.song_scroll_y))) {
            state.overview_shelf_scroll_x_target[row.key] = std::max(0.0f, target - 1.0f);
            result.song_selection_changed = true;
            return true;
        }

        if (CheckCollisionPointRec(mouse, detail::overview_shelf_next_button_rect(
                                              song_list_rect, shelf_row, state.song_scroll_y))) {
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
        const auto chart_indices = detail::filtered_chart_indices(state);
        for (int display_index = 0; display_index < static_cast<int>(chart_indices.size()); ++display_index) {
            const int index = chart_indices[static_cast<size_t>(display_index)];
            const chart_entry_state& chart = song->charts[static_cast<size_t>(index)];
            const Rectangle card = detail::chart_row_rect(chart_list_rect, display_index, state.chart_scroll_y);
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
        result.action = preview_controller::toggle_playback_action(audio_controller);
        return true;
    }

    const Rectangle filter_panel = current.detail_left_rect;
    if (ui::is_clicked(chart_source_button_rect(filter_panel, 0))) {
        state.chart_source = chart_source_filter::all;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_source_button_rect(filter_panel, 1))) {
        state.chart_source = chart_source_filter::official;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_source_button_rect(filter_panel, 2))) {
        state.chart_source = chart_source_filter::community;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_source_button_rect(filter_panel, 3))) {
        state.chart_source = chart_source_filter::mine;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_clear_button_rect(filter_panel))) {
        clear_chart_filters(state);
        return true;
    }
    if (update_level_range_from_slider(state, filter_panel, mouse)) {
        return true;
    }

    if (ui::is_clicked(chart_key_button_rect(filter_panel, 0))) {
        state.chart_key_filter = 0;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_key_button_rect(filter_panel, 1))) {
        state.chart_key_filter = 4;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_key_button_rect(filter_panel, 2))) {
        state.chart_key_filter = 5;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_key_button_rect(filter_panel, 3))) {
        state.chart_key_filter = 6;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_key_button_rect(filter_panel, 4))) {
        state.chart_key_filter = 7;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_status_button_rect(filter_panel, 0))) {
        state.chart_download_filter = 0;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_status_button_rect(filter_panel, 1))) {
        state.chart_download_filter = 1;
        reset_chart_scroll(state);
        return true;
    }
    if (ui::is_clicked(chart_status_button_rect(filter_panel, 2))) {
        state.chart_download_filter = 2;
        reset_chart_scroll(state);
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

    const Rectangle bar = preview_bar_rect(current.preview_panel_rect);
    if (preview_controller::update_scrub(state, song, audio_controller, bar, mouse, left_pressed)) {
        return true;
    }

    if (ui::is_clicked(preview_play_button_rect(current.preview_panel_rect))) {
        result.action = preview_controller::toggle_playback_action(audio_controller);
        return true;
    }
    if (ui::is_clicked(preview_prev_button_rect(current.preview_panel_rect)) ||
        ui::is_clicked(preview_next_button_rect(current.preview_panel_rect))) {
        const auto indices = detail::filtered_indices(state);
        int& selected_song_index = detail::selected_song_index_ref(state);
        const auto selected_it = std::find(indices.begin(), indices.end(), selected_song_index);
        if (!indices.empty() && selected_it != indices.end()) {
            const int display_index = static_cast<int>(selected_it - indices.begin());
            const int delta = ui::is_clicked(preview_next_button_rect(current.preview_panel_rect)) ? 1 : -1;
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
    if (ui::is_clicked(preview_open_button_rect(current.preview_panel_rect))) {
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
            request_charts_for_selected_song(state, data_controller);
        }
    }

    const song_entry_state* song = selected_song(state);
    const auto chart_indices = detail::filtered_chart_indices(state);
    if (state.detail_open && song != nullptr && !chart_indices.empty()) {
        auto selected_it = std::find(chart_indices.begin(), chart_indices.end(), selected_chart_index);
        int display_index = selected_it == chart_indices.end() ? 0 : static_cast<int>(selected_it - chart_indices.begin());

        int next_display_index = display_index;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            next_display_index = std::max(0, display_index - 1);
        } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            next_display_index = std::min(static_cast<int>(chart_indices.size()) - 1, display_index + 1);
        } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            next_display_index = std::max(0, display_index - detail::kChartGridColumns);
        } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
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

    if (!state.detail_open && CheckCollisionPointRec(mouse, song_list_rect) && wheel != 0.0f) {
        state.song_scroll_y_target -= wheel * 54.0f;
    } else if (state.detail_open && CheckCollisionPointRec(mouse, chart_list_rect) && wheel != 0.0f) {
        state.chart_scroll_y_target -= wheel * 42.0f;
    }

    state.song_scroll_y_target = std::clamp(state.song_scroll_y_target, 0.0f,
                                            detail::max_song_scroll(state, song_list_rect, filtered_song_count));
    state.song_scroll_y = tween::damp(state.song_scroll_y, state.song_scroll_y_target, dt, 12.0f, 0.5f);
    if (!state.detail_open &&
        state.song_scroll_y_target >= std::max(0.0f, detail::max_song_scroll(state, song_list_rect, filtered_song_count) - 120.0f)) {
        request_next_song_page(state, data_controller, state.mode);
    }

    state.chart_scroll_y_target = std::clamp(state.chart_scroll_y_target, 0.0f,
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
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const float wheel = GetMouseWheelMove();
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
