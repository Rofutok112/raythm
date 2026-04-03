#include "song_select/song_select_state.h"

#include <algorithm>
#include <utility>

#include "song_select/song_select_layout.h"

namespace song_select {

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
    state.selected_song_index = 0;
    state.difficulty_index = 0;
    state.scroll_y = 0.0f;
    state.scroll_y_target = 0.0f;
    state.song_change_anim_t = 1.0f;
    state.scene_fade_in_t = 1.0f;
    state.scrollbar_dragging = false;
    state.scrollbar_drag_offset = 0.0f;
    state.context_menu = {};
    state.confirmation_dialog = {};
    state.status_message.clear();
    state.status_message_is_error = false;
}

void tick_animations(state& state, float dt) {
    state.song_change_anim_t = std::max(0.0f, state.song_change_anim_t - dt * 4.0f);
    state.scene_fade_in_t = std::max(0.0f, state.scene_fade_in_t - dt / 0.3f);
}

void apply_catalog(state& state, catalog_data catalog,
                   const std::string& preferred_song_id,
                   const std::string& preferred_chart_id) {
    state.songs = std::move(catalog.songs);
    state.load_errors = std::move(catalog.load_errors);
    state.selected_song_index = 0;
    state.difficulty_index = 0;
    state.scroll_y = 0.0f;
    state.scroll_y_target = 0.0f;
    state.scrollbar_dragging = false;
    state.scrollbar_drag_offset = 0.0f;
    state.context_menu = {};
    state.confirmation_dialog = {};

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
    }
}

bool apply_song_selection(state& state, int song_index, int chart_index) {
    if (state.songs.empty()) {
        return false;
    }

    const int clamped_song_index = std::clamp(song_index, 0, static_cast<int>(state.songs.size()) - 1);
    const bool song_changed = clamped_song_index != state.selected_song_index;
    state.selected_song_index = clamped_song_index;

    const auto filtered = filtered_charts_for_selected_song(state);
    if (filtered.empty()) {
        state.difficulty_index = 0;
    } else {
        state.difficulty_index = std::clamp(chart_index, 0, static_cast<int>(filtered.size()) - 1);
    }

    if (song_changed) {
        state.song_change_anim_t = 1.0f;
    }
    return song_changed;
}

void open_song_context_menu(state& state, int song_index, Rectangle rect) {
    state.context_menu.open = true;
    state.context_menu.target = context_menu_target::song;
    state.context_menu.song_index = song_index;
    state.context_menu.chart_index = -1;
    state.context_menu.rect = rect;
}

void open_chart_context_menu(state& state, int song_index, int chart_index, Rectangle rect) {
    state.context_menu.open = true;
    state.context_menu.target = context_menu_target::chart;
    state.context_menu.song_index = song_index;
    state.context_menu.chart_index = chart_index;
    state.context_menu.rect = rect;
}

void close_context_menu(state& state) {
    state.context_menu = {};
}

void queue_status_message(state& state, std::string message, bool is_error) {
    state.status_message = std::move(message);
    state.status_message_is_error = is_error;
}

float expanded_row_height(const state& state, int song_index) {
    if (song_index == state.selected_song_index) {
        return layout::kRowHeight + 14.0f +
               static_cast<float>(filtered_charts_for_selected_song(state).size()) * 30.0f;
    }
    return layout::kRowHeight;
}

float content_height(const state& state) {
    float total = 0.0f;
    for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
        total += expanded_row_height(state, i);
    }
    return total;
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
