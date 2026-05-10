#include "title/song_list_view.h"

#include <algorithm>
#include <cmath>

#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "shared/content_status_badge.h"

namespace {

constexpr float kSongRowHeight = 102.0f;
constexpr float kSongRowGap = 15.0f;
constexpr float kScrollPadding = 36.0f;
constexpr float kInitialRowOffsetY = 18.0f;
constexpr float kSongCountOffsetY = 36.0f;
constexpr float kSongCountHeight = 27.0f;
constexpr float kClipSlack = 6.0f;
constexpr float kRowBorderWidth = 1.5f;
constexpr float kTextPaddingX = 21.0f;
constexpr float kTitleOffsetY = 15.0f;
constexpr float kTitleHeight = 36.0f;
constexpr float kArtistOffsetY = 60.0f;
constexpr float kArtistHeight = 27.0f;
constexpr float kStatusBadgeWidth = 96.0f;
constexpr float kStatusBadgeHeight = 24.0f;
constexpr float kStatusBadgeInset = 15.0f;

std::string format_duration_label(float seconds) {
    if (seconds <= 0.0f) {
        return "";
    }
    const int total_seconds = static_cast<int>(std::round(seconds));
    return TextFormat("%d:%02d", total_seconds / 60, total_seconds % 60);
}

}  // namespace

namespace title_song_list_view {

float content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    return static_cast<float>(count) * (kSongRowHeight + kSongRowGap) - kSongRowGap;
}

float max_scroll(Rectangle area, int count) {
    return std::max(0.0f, content_height(count) - area.height + kScrollPadding);
}

Rectangle row_rect(Rectangle area, int index, float scroll_y) {
    return {
        area.x,
        area.y + kInitialRowOffsetY + static_cast<float>(index) * (kSongRowHeight + kSongRowGap) - scroll_y,
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
    const bool hide_unloaded_content =
        state.catalog_loading && !state.catalog_loaded_once && state.songs.empty();
    if (hide_unloaded_content) {
        return;
    }

    ui::draw_text_in_rect(TextFormat("%d songs", static_cast<int>(state.songs.size())), 16,
                          {config.column_rect.x, config.column_rect.y - kSongCountOffsetY,
                           config.column_rect.width, kSongCountHeight},
                          with_alpha(t.text_muted, config.alpha), ui::text_align::left);

    ui::scoped_clip_rect clip(config.column_rect);
    for (int i = 0; i < static_cast<int>(state.songs.size()); ++i) {
        const song_select::song_entry& song = state.songs[static_cast<size_t>(i)];
        const Rectangle row = row_rect(config.column_rect, i, state.scroll_y);
        if (row.y + row.height < config.column_rect.y - kClipSlack ||
            row.y > config.column_rect.y + config.column_rect.height + kClipSlack) {
            continue;
        }

        const bool selected = i == state.selected_song_index;
        const bool hovered = ui::is_hovered(row);
        const unsigned char row_alpha = selected ? config.selected_row_alpha
            : hovered ? config.hover_row_alpha
                      : config.normal_row_alpha;

        ui::draw_rect_f(row, with_alpha(selected ? config.button_selected : config.button_base, row_alpha));
        ui::draw_rect_lines(
            row, kRowBorderWidth,
            with_alpha(t.border_light, static_cast<unsigned char>(130.0f * config.play_t)));
        const Rectangle badge_rect = {
            row.x + row.width - kStatusBadgeWidth - kStatusBadgeInset,
            row.y + kStatusBadgeInset,
            kStatusBadgeWidth,
            kStatusBadgeHeight,
        };
        draw_marquee_text(song.song.meta.title.c_str(),
                          {row.x + kTextPaddingX, row.y + kTitleOffsetY,
                           row.width - kTextPaddingX * 2.0f - kStatusBadgeWidth, kTitleHeight},
                          20, with_alpha(t.text, config.alpha), config.now);
        const std::string duration_label = format_duration_label(song.song.meta.duration_seconds);
        const float artist_reserved_width = duration_label.empty() ? 0.0f : 62.0f;
        draw_marquee_text(song.song.meta.artist.c_str(),
                          {row.x + kTextPaddingX, row.y + kArtistOffsetY,
                           row.width - kTextPaddingX * 2.0f - kStatusBadgeWidth - artist_reserved_width, kArtistHeight},
                          14, with_alpha(t.text_muted, config.alpha), config.now);
        if (!duration_label.empty()) {
            ui::draw_text_in_rect(duration_label.c_str(),
                                  13,
                                  {badge_rect.x - 70.0f, row.y + kArtistOffsetY + 2.0f, 58.0f, 18.0f},
                                  with_alpha(t.text_muted, config.alpha), ui::text_align::right);
        }
        content_status_badge::draw_compound(badge_rect, song.source_status, song.status, config.alpha, 12);
    }
}

}  // namespace title_song_list_view
