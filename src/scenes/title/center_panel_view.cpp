#include "title/center_panel_view.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"

namespace {

constexpr float kChartButtonHeight = 81.0f;
constexpr float kChartButtonGap = 12.0f;
constexpr float kEmptyMessageOffsetY = 180.0f;
constexpr float kEmptyMessageHeight = 60.0f;
constexpr float kJacketFramePadding = 12.0f;
constexpr float kJacketInnerPadding = 3.0f;
constexpr float kJacketBorderWidth = 2.25f;
constexpr float kHeaderPaddingX = 18.0f;
constexpr float kTitleOffsetY = 18.0f;
constexpr float kTitleHeight = 45.0f;
constexpr float kArtistOffsetY = 72.0f;
constexpr float kArtistHeight = 33.0f;
constexpr float kChartDifficultyOffsetY = 6.0f;
constexpr float kChartDifficultyHeight = 30.0f;
constexpr float kChartNotesOffsetY = 48.0f;
constexpr float kChartNotesHeight = 27.0f;
constexpr float kChartBpmOffsetY = 81.0f;
constexpr float kChartBpmHeight = 27.0f;
constexpr float kChartAuthorOffsetY = 114.0f;
constexpr float kChartAuthorHeight = 24.0f;
constexpr float kClipSlack = 6.0f;
constexpr float kChartRowBorderWidth = 1.5f;
constexpr float kChartTextPaddingX = 18.0f;
constexpr float kChartRightReserved = 84.0f;
constexpr float kChartTitleOffsetY = 13.5f;
constexpr float kChartTitleHeight = 27.0f;
constexpr float kChartLevelOffsetY = 45.0f;
constexpr float kChartLevelHeight = 24.0f;
constexpr float kChartBadgeRightInset = 57.0f;
constexpr float kChartBadgeOffsetY = 15.0f;
constexpr float kChartBadgeWidth = 39.0f;
constexpr float kChartBadgeHeight = 24.0f;
constexpr float kChartRankOffsetY = 45.0f;
constexpr float kChartRankHeight = 21.0f;

const char* rank_label(rank value) {
    switch (value) {
        case rank::ss: return "SS";
        case rank::s: return "S";
        case rank::aa: return "AA";
        case rank::a: return "A";
        case rank::b: return "B";
        case rank::c: return "C";
        case rank::f: return "F";
    }
    return "?";
}

Color rank_color(rank value) {
    const auto& theme = *g_theme;
    switch (value) {
        case rank::ss: return theme.rank_ss;
        case rank::s: return theme.rank_s;
        case rank::aa: return theme.rank_aa;
        case rank::a: return theme.rank_a;
        case rank::b: return theme.rank_b;
        case rank::c: return theme.rank_c;
        case rank::f: return theme.rank_f;
    }
    return theme.text_secondary;
}

std::string key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

Color key_mode_color(int key_count) {
    const auto& theme = *g_theme;
    return key_count == 6 ? theme.rank_c : theme.rank_b;
}

std::string format_bpm_range(float min_bpm, float max_bpm) {
    if (min_bpm <= 0.0f && max_bpm <= 0.0f) {
        return "-";
    }
    if (std::fabs(max_bpm - min_bpm) < 0.05f) {
        return TextFormat("%.0f", min_bpm);
    }
    return TextFormat("%.0f-%.0f", min_bpm, max_bpm);
}

}  // namespace

namespace title_center_view {

float chart_list_content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    return static_cast<float>(count) * (kChartButtonHeight + kChartButtonGap) - kChartButtonGap;
}

float max_chart_scroll(Rectangle area, int count) {
    return std::max(0.0f, chart_list_content_height(count) - area.height);
}

Rectangle chart_button_rect(Rectangle area, int index, float scroll_y) {
    return {
        area.x,
        area.y + static_cast<float>(index) * (kChartButtonHeight + kChartButtonGap) - scroll_y,
        area.width,
        kChartButtonHeight
    };
}

int hit_test_chart(Rectangle area, float scroll_y, Vector2 point, int count) {
    if (!CheckCollisionPointRec(point, area)) {
        return -1;
    }
    for (int index = 0; index < count; ++index) {
        if (CheckCollisionPointRec(point, chart_button_rect(area, index, scroll_y))) {
            return index;
        }
    }
    return -1;
}

void draw(const song_select::state& state,
          const song_select::preview_controller& preview_controller,
          const song_select::song_entry* song,
          const song_select::chart_option* chart,
          std::span<const song_select::chart_option* const> filtered,
          const draw_config& config) {
    const auto& t = *g_theme;

    if (song == nullptr) {
        if (state.catalog_loading && !state.catalog_loaded_once && state.songs.empty()) {
            return;
        }
        ui::draw_text_in_rect("No songs found yet.", 34,
                              {config.main_column_rect.x, config.main_column_rect.y + kEmptyMessageOffsetY,
                               config.main_column_rect.width, kEmptyMessageHeight},
                              with_alpha(t.text, config.alpha));
        return;
    }

    const Rectangle jacket_frame = {
        config.jacket_rect.x - kJacketFramePadding,
        config.jacket_rect.y - kJacketFramePadding,
        config.jacket_rect.width + kJacketFramePadding * 2.0f,
        config.jacket_rect.height + kJacketFramePadding * 2.0f
    };
    const Rectangle jacket_inner = {
        config.jacket_rect.x + kJacketInnerPadding,
        config.jacket_rect.y + kJacketInnerPadding,
        config.jacket_rect.width - kJacketInnerPadding * 2.0f,
        config.jacket_rect.height - kJacketInnerPadding * 2.0f
    };

    ui::draw_rect_lines(jacket_frame, kJacketBorderWidth,
                        with_alpha(t.border_image, static_cast<unsigned char>(190.0f * config.play_t)));
    if (preview_controller.jacket_loaded()) {
        const Texture2D& jacket = preview_controller.jacket_texture();
        DrawTexturePro(jacket,
                       {0.0f, 0.0f, static_cast<float>(jacket.width), static_cast<float>(jacket.height)},
                       jacket_inner, {0.0f, 0.0f}, 0.0f,
                       with_alpha(WHITE, config.alpha));
    } else {
        ui::draw_text_in_rect("JACKET", 26, jacket_inner, with_alpha(t.text_muted, config.alpha));
    }

    const Rectangle title_rect = {
        config.main_column_rect.x + kHeaderPaddingX,
        config.main_column_rect.y + kTitleOffsetY,
        config.main_column_rect.width - kHeaderPaddingX * 2.0f,
        kTitleHeight
    };
    const Rectangle artist_rect = {
        config.main_column_rect.x + kHeaderPaddingX,
        config.main_column_rect.y + kArtistOffsetY,
        config.main_column_rect.width - kHeaderPaddingX * 2.0f,
        kArtistHeight
    };
    draw_marquee_text(song->song.meta.title.c_str(),
                      title_rect,
                      28, with_alpha(t.text, config.alpha), config.now);
    draw_marquee_text(song->song.meta.artist.c_str(),
                      artist_rect,
                      18, with_alpha(t.text_secondary, config.alpha), config.now);
    if (chart != nullptr) {
        ui::draw_text_in_rect(TextFormat("%s  Lv.%.1f", chart->meta.difficulty.c_str(), chart->meta.level),
                              18,
                              {config.chart_detail_rect.x, config.chart_detail_rect.y + kChartDifficultyOffsetY,
                               config.chart_detail_rect.width, kChartDifficultyHeight},
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%s   %d Notes", key_mode_label(chart->meta.key_count).c_str(),
                                         chart->note_count),
                              14,
                              {config.chart_detail_rect.x, config.chart_detail_rect.y + kChartNotesOffsetY,
                               config.chart_detail_rect.width, kChartNotesHeight},
                              with_alpha(t.text_secondary, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("BPM %s", format_bpm_range(chart->min_bpm, chart->max_bpm).c_str()),
                              14,
                              {config.chart_detail_rect.x, config.chart_detail_rect.y + kChartBpmOffsetY,
                               config.chart_detail_rect.width, kChartBpmHeight},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        draw_marquee_text(chart->meta.chart_author.c_str(),
                          {config.chart_detail_rect.x, config.chart_detail_rect.y + kChartAuthorOffsetY,
                           config.chart_detail_rect.width, kChartAuthorHeight},
                          14, with_alpha(t.text_muted, config.alpha), config.now);
    }

    ui::scoped_clip_rect clip(config.chart_buttons_rect);
    for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
        const song_select::chart_option& item = *filtered[static_cast<size_t>(i)];
        const Rectangle row = chart_button_rect(config.chart_buttons_rect, i, state.chart_scroll_y);
        if (row.y + row.height < config.chart_buttons_rect.y - kClipSlack ||
            row.y > config.chart_buttons_rect.y + config.chart_buttons_rect.height + kClipSlack) {
            continue;
        }

        const bool selected = i == state.difficulty_index;
        const bool hovered = ui::is_hovered(row);
        const unsigned char row_alpha = selected ? config.selected_row_alpha
            : hovered ? config.hover_row_alpha
                      : config.normal_row_alpha;
        ui::draw_rect_f(row, with_alpha(selected ? config.button_selected : config.button_base, row_alpha));
        ui::draw_rect_lines(
            row, kChartRowBorderWidth,
            with_alpha(t.border_light, static_cast<unsigned char>(130.0f * config.play_t)));
        ui::draw_text_in_rect(item.meta.difficulty.c_str(), 16,
                              {row.x + kChartTextPaddingX, row.y + kChartTitleOffsetY,
                               row.width - kChartRightReserved, kChartTitleHeight},
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("Lv.%.1f", item.meta.level), 13,
                              {row.x + kChartTextPaddingX, row.y + kChartLevelOffsetY,
                               row.width - kChartRightReserved, kChartLevelHeight},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(key_mode_label(item.meta.key_count).c_str(), 13,
                              {row.x + row.width - kChartBadgeRightInset, row.y + kChartBadgeOffsetY,
                               kChartBadgeWidth, kChartBadgeHeight},
                              with_alpha(key_mode_color(item.meta.key_count), config.alpha), ui::text_align::right);
        if (item.best_local_rank.has_value()) {
            ui::draw_text_in_rect(rank_label(*item.best_local_rank), 12,
                                  {row.x + row.width - kChartBadgeRightInset, row.y + kChartRankOffsetY,
                                   kChartBadgeWidth, kChartRankHeight},
                                  with_alpha(rank_color(*item.best_local_rank), config.alpha), ui::text_align::right);
        }
    }
}

}  // namespace title_center_view
