#include "multiplayer_result/multiplayer_result_summary_view.h"

#include <algorithm>
#include <array>

#include "multiplayer_result/multiplayer_result_widgets.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"

namespace multiplayer_result::summary_view {
namespace {

constexpr Rectangle kJacketRect{69.0f, 127.0f, 270.0f, 270.0f};
constexpr Rectangle kSongTitleRect{kJacketRect.x, 424.0f, kJacketRect.width, 48.0f};
constexpr Rectangle kSongArtistRect{kJacketRect.x, 476.0f, kJacketRect.width, 34.0f};
constexpr Rectangle kChartBadgesRect{kJacketRect.x, 534.0f, kJacketRect.width, 32.0f};
constexpr Rectangle kSelfPlaceRect{kJacketRect.x, 610.0f, kJacketRect.width, 112.0f};
constexpr float kKeyBadgeWidth = 42.0f;
constexpr float kKeyToDifficultyGap = 14.0f;
constexpr float kDifficultyToLevelGap = 20.0f;
constexpr float kLevelBadgeWidth = 78.0f;

struct self_place_layout {
    Rectangle label;
    Rectangle avatar;
    Rectangle place;
};

struct chart_badges_layout {
    Rectangle key;
    Rectangle difficulty;
    Rectangle level;
};

struct song_text_descriptor {
    Rectangle rect;
    const char* text;
    int font_size;
    Color color;
};

constexpr self_place_layout self_place_layout_for(Rectangle panel) {
    const ui::rect_pair rows = ui::split_rows(
        ui::inset(panel, ui::edge_insets{18.0f, 20.0f, 12.0f, 20.0f}),
        24.0f);
    const ui::rect_pair body_columns = ui::split_columns(rows.second, 58.0f, 18.0f);
    return {
        rows.first,
        body_columns.first,
        {body_columns.second.x, body_columns.second.y + 4.0f, 82.0f, 54.0f},
    };
}

constexpr chart_badges_layout chart_badges_layout_for(Rectangle rect) {
    const Rectangle key{rect.x, rect.y, kKeyBadgeWidth, rect.height};
    const Rectangle level{rect.x + rect.width - kLevelBadgeWidth, rect.y, kLevelBadgeWidth, rect.height};
    const float difficulty_x = key.x + key.width + kKeyToDifficultyGap;
    return {
        key,
        {difficulty_x,
         rect.y - 1.0f,
         std::max(0.0f, level.x - difficulty_x - kDifficultyToLevelGap),
         rect.height + 2.0f},
        level,
    };
}

void draw_chart_badges(Rectangle rect, const chart_meta& chart, int key_count) {
    const Color level_color = difficulty_level_color(chart.level);
    const chart_badges_layout layout = chart_badges_layout_for(rect);
    ui::surface(layout.key,
                with_alpha(lerp_color(g_theme->section, level_color, 0.18f), 224),
                with_alpha(level_color, 184),
                1.0f);
    ui::draw_text_in_rect(TextFormat("%dK", key_count), 15, layout.key, g_theme->text, ui::text_align::center);
    draw_marquee_text(chart.difficulty.c_str(), layout.difficulty, 22, g_theme->text, GetTime());
    draw_difficulty_level_badge(chart.level, layout.level, 15, 255);
}

std::array<song_text_descriptor, 2> song_text_descriptors_for(const song_data& song) {
    return {{
        {kSongTitleRect, song.meta.title.c_str(), 32, g_theme->text},
        {kSongArtistRect, song.meta.artist.c_str(), 22, g_theme->text_secondary},
    }};
}

void draw_song_texts(const song_data& song) {
    const double now = GetTime();
    for (const song_text_descriptor& text : song_text_descriptors_for(song)) {
        draw_marquee_text(text.text, text.rect, text.font_size, text.color, now);
    }
}

}  // namespace

void draw(const song_data& song,
          const chart_meta& chart,
          int key_count,
          const play_multiplayer_score_row& self_score,
          int self_place,
          Texture2D jacket_texture,
          bool jacket_texture_loaded,
          const std::string& avatar_base_url) {
    if (jacket_texture_loaded) {
        ui::draw_texture(jacket_texture, kJacketRect);
    } else {
        ui::placeholder(kJacketRect, "", {.draw_border = false});
    }
    ui::frame(kJacketRect, g_theme->border_image, 2.0f);

    draw_song_texts(song);
    draw_chart_badges(kChartBadgesRect, chart, key_count);

    const self_place_layout self_layout = self_place_layout_for(kSelfPlaceRect);
    ui::surface(kSelfPlaceRect, with_alpha(g_theme->section, 230), g_theme->border_light, 1.5f);
    ui::draw_text_in_rect("YOUR PLACE", 18, self_layout.label, g_theme->text_muted, ui::text_align::left);
    widgets::draw_profile_image_slot(self_layout.avatar,
                                     self_score.avatar_url,
                                     self_score.display_name,
                                     avatar_base_url);
    ui::draw_text_in_rect(self_place > 0 ? TextFormat("#%d", self_place) : "--", 52,
                          self_layout.place,
                          self_place > 0 ? widgets::podium_rank_color(self_place - 1) : g_theme->text_muted,
                          ui::text_align::left);
}

}  // namespace multiplayer_result::summary_view
