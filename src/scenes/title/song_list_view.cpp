#include "title/song_list_view.h"

#include <algorithm>

#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {

constexpr float kSongRowHeight = 68.0f;
constexpr float kSongRowGap = 10.0f;

}  // namespace

namespace title_song_list_view {

float content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    return static_cast<float>(count) * (kSongRowHeight + kSongRowGap) - kSongRowGap;
}

float max_scroll(Rectangle area, int count) {
    return std::max(0.0f, content_height(count) - area.height + 24.0f);
}

Rectangle row_rect(Rectangle area, int index, float scroll_y) {
    return {
        area.x,
        area.y + 12.0f + static_cast<float>(index) * (kSongRowHeight + kSongRowGap) - scroll_y,
        area.width,
        kSongRowHeight
    };
}

int hit_test(Rectangle area, float scroll_y, Vector2 point, int count) {
    if (!CheckCollisionPointRec(point, area)) {
        return -1;
    }
    for (int index = 0; index < count; ++index) {
        if (CheckCollisionPointRec(point, row_rect(area, index, scroll_y))) {
            return index;
        }
    }
    return -1;
}

void draw(const song_select::state& state, const draw_config& config) {
    const auto& t = *g_theme;

    ui::draw_text_in_rect(TextFormat("%d songs", static_cast<int>(state.songs.size())), 16,
                          {config.column_rect.x, config.column_rect.y - 24.0f, config.column_rect.width, 18.0f},
                          with_alpha(t.text_muted, config.alpha), ui::text_align::left);

    ui::scoped_clip_rect clip(config.column_rect);
    for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
        const song_select::song_entry& song = state.songs[static_cast<size_t>(i)];
        const Rectangle row = row_rect(config.column_rect, i, state.scroll_y);
        if (row.y + row.height < config.column_rect.y - 4.0f ||
            row.y > config.column_rect.y + config.column_rect.height + 4.0f) {
            continue;
        }

        const bool selected = i == state.selected_song_index;
        const bool hovered = ui::is_hovered(row);
        const unsigned char row_alpha = selected ? config.selected_row_alpha
            : hovered ? config.hover_row_alpha
                      : config.normal_row_alpha;

        DrawRectangleRec(row, with_alpha(selected ? config.button_selected : config.button_base, row_alpha));
        DrawRectangleLinesEx(
            row, 1.0f,
            with_alpha(t.border_light, static_cast<unsigned char>(130.0f * config.play_t)));
        draw_marquee_text(song.song.meta.title.c_str(),
                          {row.x + 14.0f, row.y + 10.0f, row.width - 26.0f, 22.0f},
                          20, with_alpha(t.text, config.alpha), config.now);
        draw_marquee_text(song.song.meta.artist.c_str(),
                          {row.x + 14.0f, row.y + 38.0f, row.width - 26.0f, 16.0f},
                          14, with_alpha(t.text_muted, config.alpha), config.now);
    }
}

}  // namespace title_song_list_view
