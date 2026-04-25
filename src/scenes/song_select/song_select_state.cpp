#include "song_select/song_select_state.h"

#include <algorithm>
#include <utility>

#include "song_select/song_select_layout.h"
#include "tween.h"
#include "ui_notice.h"

namespace song_select {

float scroll_offset_for_selected_song(const state& state);

const song_entry* selected_song(const state& state) {
    if (state.songs.empty() || state.selected_song_index < 0 ||
        state.selected_song_index >= static_cast<int>(state.songs.size())) {
        return nullptr;
    }

    return &state.songs[static_cast<size_t>(state.selected_song_index)];
}

std::vector<const chart_option*> filtered_charts_for_selected_song(const state& state) {
    std::vector<const chart_option*> filtered;
    const song_entry* song = selected_song(state);
    if (song == nullptr) {
        return filtered;
    }

    filtered.reserve(song->charts.size());
    for (const chart_option& chart : song->charts) {
        filtered.push_back(&chart);
    }
    return filtered;
}

const chart_option* selected_chart_for(const state& state, const std::vector<const chart_option*>& filtered) {
    if (filtered.empty()) {
        return nullptr;
    }

    const int index = std::min<int>(state.difficulty_index, static_cast<int>(filtered.size()) - 1);
    return filtered[static_cast<size_t>(index)];
}

void reset_for_enter(state& state) {
    state.catalog_loading = false;
    state.catalog_loaded_once = false;
    state.selected_song_index = 0;
    state.difficulty_index = 0;
    state.scroll_y = 0.0f;
    state.scroll_y_target = 0.0f;
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
    state.song_change_anim_t = 1.0f;
    state.chart_change_anim_t = 1.0f;
    state.scene_fade_in.restart(scene_fade::direction::in, 0.3f, 0.65f);
    state.scrollbar_dragging = false;
    state.scrollbar_drag_offset = 0.0f;
    state.context_menu = {};
    state.confirmation_dialog = {};
    state.recent_result_offset.reset();
    state.ranking_panel = {};
    state.login_dialog = {};
}

void tick_animations(state& state, float dt) {
    state.song_change_anim_t = tween::retreat(state.song_change_anim_t, dt, 4.0f);
    state.chart_change_anim_t = tween::retreat(state.chart_change_anim_t, dt, 5.0f);
    state.ranking_panel.reveal_anim += dt;
    if (state.login_dialog.open) {
        state.login_dialog.open_anim = tween::advance(state.login_dialog.open_anim, dt, 8.0f);
    } else {
        state.login_dialog.open_anim = 0.0f;
    }
    state.scene_fade_in.update(dt);
}

void apply_catalog(state& state, catalog_data catalog,
                   const std::string& preferred_song_id,
                   const std::string& preferred_chart_id) {
    state.songs = std::move(catalog.songs);
    state.load_errors = std::move(catalog.load_errors);
    state.catalog_loading = false;
    state.catalog_loaded_once = true;
    state.selected_song_index = 0;
    state.difficulty_index = 0;
    state.scroll_y = 0.0f;
    state.scroll_y_target = 0.0f;
    state.chart_scroll_y = 0.0f;
    state.chart_scroll_y_target = 0.0f;
    state.scrollbar_dragging = false;
    state.scrollbar_drag_offset = 0.0f;
    state.context_menu = {};
    state.confirmation_dialog = {};
    state.ranking_panel.scroll_y = 0.0f;
    state.ranking_panel.scroll_y_target = 0.0f;
    state.ranking_panel.reveal_anim = 0.0f;
    state.ranking_panel.source_dropdown_open = false;
    state.ranking_panel.scrollbar_dragging = false;
    state.ranking_panel.scrollbar_drag_offset = 0.0f;

    if (!preferred_song_id.empty()) {
        for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
            if (state.songs[static_cast<size_t>(i)].song.meta.song_id == preferred_song_id) {
                state.selected_song_index = i;
                break;
            }
        }
    }

    if (!state.songs.empty() && !preferred_chart_id.empty()) {
        const auto& charts = state.songs[static_cast<size_t>(state.selected_song_index)].charts;
        for (int i = 0; i < static_cast<int>(charts.size()); ++i) {
            if (charts[static_cast<size_t>(i)].meta.chart_id == preferred_chart_id) {
                state.difficulty_index = i;
                break;
            }
        }
    }

    if (!state.songs.empty()) {
        state.song_change_anim_t = 1.0f;
        state.chart_change_anim_t = 1.0f;
        const float restored_scroll = scroll_offset_for_selected_song(state);
        state.scroll_y = restored_scroll;
        state.scroll_y_target = restored_scroll;
    }
}

bool apply_song_selection(state& state, int song_index, int chart_index) {
    if (state.songs.empty()) {
        return false;
    }

    const int clamped_song_index = std::clamp(song_index, 0, static_cast<int>(state.songs.size()) - 1);
    const bool song_changed = clamped_song_index != state.selected_song_index;
    const int previous_chart_index = state.difficulty_index;
    state.selected_song_index = clamped_song_index;

    const auto filtered = filtered_charts_for_selected_song(state);
    if (filtered.empty()) {
        state.difficulty_index = 0;
    } else {
        state.difficulty_index = std::clamp(chart_index, 0, static_cast<int>(filtered.size()) - 1);
    }

    if (song_changed) {
        state.song_change_anim_t = 1.0f;
        state.chart_scroll_y = 0.0f;
        state.chart_scroll_y_target = 0.0f;
    }
    if (song_changed || previous_chart_index != state.difficulty_index) {
        state.chart_change_anim_t = 1.0f;
    }
    return song_changed;
}

void open_song_context_menu(state& state, int song_index, Rectangle rect) {
    state.context_menu.open = true;
    state.context_menu.target = context_menu_target::song;
    state.context_menu.section = context_menu_section::root;
    state.context_menu.song_index = song_index;
    state.context_menu.chart_index = -1;
    state.context_menu.rect = rect;
}

void open_chart_context_menu(state& state, int song_index, int chart_index, Rectangle rect) {
    state.context_menu.open = true;
    state.context_menu.target = context_menu_target::chart;
    state.context_menu.section = context_menu_section::root;
    state.context_menu.song_index = song_index;
    state.context_menu.chart_index = chart_index;
    state.context_menu.rect = rect;
}

void open_list_background_context_menu(state& state, Rectangle rect) {
    state.context_menu.open = true;
    state.context_menu.target = context_menu_target::list_background;
    state.context_menu.section = context_menu_section::root;
    state.context_menu.song_index = state.selected_song_index;
    state.context_menu.chart_index = state.difficulty_index;
    state.context_menu.rect = rect;
}

void close_context_menu(state& state) {
    state.context_menu = {};
}

void queue_status_message(state& state, std::string message, bool is_error) {
    (void)state;
    ui::notify(std::move(message), is_error ? ui::notice_tone::error : ui::notice_tone::success);
}

float expanded_row_height(const state& state, int song_index) {
    if (song_index == state.selected_song_index) {
        return layout::kRowHeight + 14.0f +
               static_cast<float>(filtered_charts_for_selected_song(state).size()) * 30.0f;
    }
    return layout::kRowHeight;
}

float song_list_content_top() {
    return layout::kSongListTopPadding;
}

float song_list_first_item_y(const state& state) {
    return layout::kSongListViewRect.y + song_list_content_top() - state.scroll_y;
}

float content_height(const state& state) {
    float total = song_list_content_top();
    for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
        total += expanded_row_height(state, i);
    }
    return total;
}

float scroll_offset_for_selected_song(const state& state) {
    if (state.songs.empty() || state.selected_song_index < 0 ||
        state.selected_song_index >= static_cast<int>(state.songs.size())) {
        return 0.0f;
    }

    float row_top = song_list_content_top();
    for (int i = 0; i < state.selected_song_index; ++i) {
        row_top += expanded_row_height(state, i);
    }

    const float row_bottom = row_top + expanded_row_height(state, state.selected_song_index);
    const float view_height = layout::kSongListViewRect.height;
    const float max_scroll = std::max(0.0f, content_height(state) - view_height);

    float scroll = std::clamp(row_top - 12.0f, 0.0f, max_scroll);
    if (row_bottom + 12.0f > scroll + view_height) {
        scroll = std::clamp(row_bottom + 12.0f - view_height, 0.0f, max_scroll);
    }
    return scroll;
}

std::string fallback_song_id_after_song_delete(const state& state, int song_index) {
    if (state.songs.size() <= 1) {
        return "";
    }
    if (song_index + 1 < static_cast<int>(state.songs.size())) {
        return state.songs[static_cast<size_t>(song_index + 1)].song.meta.song_id;
    }
    if (song_index > 0) {
        return state.songs[static_cast<size_t>(song_index - 1)].song.meta.song_id;
    }
    return "";
}

std::string fallback_chart_id_after_chart_delete(const state& state, int song_index, int chart_index) {
    if (song_index < 0 || song_index >= static_cast<int>(state.songs.size())) {
        return "";
    }

    const auto& charts = state.songs[static_cast<size_t>(song_index)].charts;
    if (charts.size() <= 1) {
        return "";
    }
    if (chart_index + 1 < static_cast<int>(charts.size())) {
        return charts[static_cast<size_t>(chart_index + 1)].meta.chart_id;
    }
    if (chart_index > 0) {
        return charts[static_cast<size_t>(chart_index - 1)].meta.chart_id;
    }
    return "";
}

}  // namespace song_select
