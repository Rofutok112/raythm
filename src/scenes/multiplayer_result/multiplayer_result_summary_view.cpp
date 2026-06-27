#include "multiplayer_result/multiplayer_result_summary_view.h"

#include <cctype>

#include "scene_common.h"
#include "shared/avatar_texture_cache.h"
#include "theme.h"
#include "ui_draw.h"

namespace multiplayer_result::summary_view {
namespace {

constexpr Rectangle kJacketRect{69.0f, 127.0f, 270.0f, 270.0f};
constexpr Rectangle kSongTitleRect{kJacketRect.x, 424.0f, kJacketRect.width, 48.0f};
constexpr Rectangle kSongArtistRect{kJacketRect.x, 476.0f, kJacketRect.width, 34.0f};
constexpr Rectangle kChartBadgesRect{kJacketRect.x, 534.0f, kJacketRect.width, 32.0f};
constexpr Rectangle kSelfPlaceRect{kJacketRect.x, 610.0f, kJacketRect.width, 112.0f};

struct self_place_layout {
    Rectangle label;
    Rectangle avatar;
    Rectangle place;
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

Color rank_color(int rank_index) {
    if (rank_index == 0) {
        return g_theme->all_perfect;
    }
    if (rank_index == 1) {
        return g_theme->slow;
    }
    if (rank_index == 2) {
        return g_theme->fast;
    }
    return g_theme->text_muted;
}

std::string avatar_label_for(const std::string& name) {
    std::string result;
    result.reserve(2);
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (result.size() == 2) {
                break;
            }
        }
    }
    return result.empty() ? "?" : result;
}

void draw_profile_image_slot(Rectangle rect,
                             const std::string& avatar_url,
                             const std::string& display_name,
                             const std::string& base_url) {
    avatar_texture_cache::draw_avatar(rect,
                                      avatar_url,
                                      avatar_label_for(display_name),
                                      with_alpha(g_theme->section, 235),
                                      g_theme->text_secondary,
                                      13,
                                      base_url);
}

void draw_chart_badges(Rectangle rect, const chart_meta& chart, int key_count) {
    const Color level_color = difficulty_level_color(chart.level);
    const Rectangle key_chip{rect.x, rect.y, 42.0f, 32.0f};
    const Rectangle difficulty_rect{key_chip.x + key_chip.width + 14.0f, rect.y - 1.0f, 116.0f, 34.0f};
    const Rectangle level_rect{rect.x + rect.width - 78.0f, rect.y, 78.0f, 32.0f};
    ui::surface(key_chip,
                with_alpha(lerp_color(g_theme->section, level_color, 0.18f), 224),
                with_alpha(level_color, 184),
                1.0f);
    ui::draw_text_in_rect(TextFormat("%dK", key_count), 15, key_chip, g_theme->text, ui::text_align::center);
    draw_marquee_text(chart.difficulty.c_str(), difficulty_rect, 22, g_theme->text, GetTime());
    draw_difficulty_level_badge(chart.level, level_rect, 15, 255);
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

    draw_marquee_text(song.meta.title.c_str(), kSongTitleRect, 32, g_theme->text, GetTime());
    draw_marquee_text(song.meta.artist.c_str(), kSongArtistRect, 22, g_theme->text_secondary, GetTime());

    draw_chart_badges(kChartBadgesRect, chart, key_count);

    const self_place_layout self_layout = self_place_layout_for(kSelfPlaceRect);
    ui::surface(kSelfPlaceRect, with_alpha(g_theme->section, 230), g_theme->border_light, 1.5f);
    ui::draw_text_in_rect("YOUR PLACE", 18, self_layout.label, g_theme->text_muted, ui::text_align::left);
    draw_profile_image_slot(self_layout.avatar,
                            self_score.avatar_url,
                            self_score.display_name,
                            avatar_base_url);
    ui::draw_text_in_rect(self_place > 0 ? TextFormat("#%d", self_place) : "--", 52,
                          self_layout.place,
                          self_place > 0 ? rank_color(self_place - 1) : g_theme->text_muted,
                          ui::text_align::left);
}

}  // namespace multiplayer_result::summary_view
