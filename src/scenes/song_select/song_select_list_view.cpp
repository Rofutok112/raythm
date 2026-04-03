#include "song_select/song_select_list_view.h"

#include "scene_common.h"
#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {

std::string key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

void draw_song_row(const song_select::song_entry& song, float item_y, bool is_selected, double now) {
    const auto& theme = *g_theme;
    const Rectangle row_rect = {song_select::layout::kSongListRect.x + 14.0f, item_y - 8.0f,
                                song_select::layout::kSongListRect.width - 28.0f, 44.0f};
    const float text_x = song_select::layout::kSongListRect.x + 30.0f;
    const float list_text_max_w = song_select::layout::kSongListRect.width - 70.0f;
    const Rectangle title_clip_rect = {text_x, item_y, list_text_max_w, 24.0f};
    const Rectangle artist_clip_rect = {text_x, item_y + 22.0f, list_text_max_w, 16.0f};

    if (ui::is_hovered(row_rect, song_select::layout::kSceneLayer) || is_selected) {
        const ui::row_state row_state = ui::draw_selectable_row(row_rect, is_selected, 0.0f);
        (void)row_state;
    }

    draw_marquee_text(song.song.meta.title.c_str(), title_clip_rect,
                      24, is_selected ? theme.text : theme.text_secondary, now);
    draw_marquee_text(song.song.meta.artist.c_str(), artist_clip_rect,
                      16, theme.text_muted, now);
}

void draw_chart_rows(const song_select::state& state,
                     const std::vector<const song_select::chart_option*>& filtered,
                     float item_y) {
    const auto& theme = *g_theme;
    const float child_x = song_select::layout::kSongListRect.x + 46.0f;
    const float child_w = song_select::layout::kSongListRect.width - 92.0f;
    const float child_text_x = song_select::layout::kSongListRect.x + 58.0f;
    const float author_x = song_select::layout::kSongListRect.x + song_select::layout::kSongListRect.width - 120.0f;
    float child_y = item_y + 46.0f;
    for (int chart_index = 0; chart_index < static_cast<int>(filtered.size()); ++chart_index) {
        const song_select::chart_option& chart = *filtered[static_cast<size_t>(chart_index)];
        const bool child_selected = chart_index == state.difficulty_index;
        const Rectangle child_rect = {child_x, child_y - 6.0f, child_w, 28.0f};
        if (ui::is_hovered(child_rect, song_select::layout::kSceneLayer) || child_selected) {
            const ui::row_state child_state = ui::draw_selectable_row(child_rect, child_selected, 0.0f);
            (void)child_state;
        }
        ui::draw_text_f(TextFormat("%s %s Lv.%d", key_mode_label(chart.meta.key_count).c_str(), chart.meta.difficulty.c_str(),
                                   chart.meta.level),
                        child_text_x, child_y, 18, child_selected ? theme.text : theme.text_secondary);
        ui::draw_text_f(chart.meta.chart_author.c_str(), author_x, child_y + 1.0f, 14, theme.text_muted);
        child_y += 30.0f;
    }
}

}  // namespace

namespace song_select {

std::optional<list_hit> hit_test_song_list(const state& state, Vector2 mouse) {
    if (!CheckCollisionPointRec(mouse, layout::kSongListViewRect)) {
        return std::nullopt;
    }

    const std::vector<const chart_option*> filtered = filtered_charts_for_selected_song(state);
    float item_y = layout::kSongListViewRect.y - state.scroll_y;
    for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
        const float row_h = expanded_row_height(state, i);
        if (i == state.selected_song_index) {
            float child_y = item_y + 46.0f;
            for (int chart_index = 0; chart_index < static_cast<int>(filtered.size()); ++chart_index) {
                const Rectangle child_rect = {layout::kSongListRect.x + 46.0f, child_y - 6.0f,
                                              layout::kSongListRect.width - 92.0f, 28.0f};
                if (CheckCollisionPointRec(mouse, child_rect)) {
                    return list_hit{.song_index = i, .chart_index = chart_index};
                }
                child_y += 30.0f;
            }
        }

        const Rectangle row_rect = {layout::kSongListRect.x + 14.0f, item_y - 8.0f,
                                    layout::kSongListRect.width - 28.0f, 44.0f};
        if (CheckCollisionPointRec(mouse, row_rect)) {
            return list_hit{.song_index = i};
        }

        item_y += row_h;
    }

    return std::nullopt;
}

void draw_song_list(const state& state) {
    const auto& theme = *g_theme;
    ui::draw_text_in_rect("Songs", 28, layout::kSongListTitleRect, theme.text, ui::text_align::left);
    ui::draw_button_colored(layout::kSongListNewSongButtonRect, "NEW SONG", 14, theme.row, theme.row_hover, theme.text);

    ui::scoped_clip_rect clip_scope(layout::kSongListViewRect);

    const std::vector<const chart_option*> filtered = filtered_charts_for_selected_song(state);
    const double now = GetTime();
    float item_y = layout::kSongListViewRect.y - state.scroll_y;
    for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
        const bool is_selected = i == state.selected_song_index;
        const float row_h = expanded_row_height(state, i);

        if (item_y + row_h < layout::kSongListViewRect.y) {
            item_y += row_h;
            continue;
        }
        if (item_y > layout::kSongListViewRect.y + layout::kSongListViewRect.height) {
            break;
        }

        draw_song_row(state.songs[static_cast<size_t>(i)], item_y, is_selected, now);
        if (is_selected) {
            draw_chart_rows(state, filtered, item_y);
        }
        item_y += row_h;
    }

    ui::draw_scrollbar(layout::kSongListScrollbarTrackRect, content_height(state), state.scroll_y,
                       theme.scrollbar_track, theme.scrollbar_thumb);
}

}  // namespace song_select
