#include "title/center_panel_view.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "shared/content_status_badge.h"

namespace {

constexpr float kChartButtonHeight = 56.0f;
constexpr float kChartButtonGap = 7.0f;
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
constexpr float kChartStatusOffsetY = 39.0f;
constexpr float kChartNotesOffsetY = 72.0f;
constexpr float kChartNotesHeight = 24.0f;
constexpr float kChartBpmOffsetY = 96.0f;
constexpr float kChartBpmHeight = 24.0f;
constexpr float kChartAuthorOffsetY = 120.0f;
constexpr float kChartAuthorHeight = 21.0f;
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
constexpr float kStatusBadgeWidth = 108.0f;
constexpr float kStatusBadgeHeight = 24.0f;
constexpr float kChartStatusWidth = 96.0f;

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

std::string format_score(int value) {
    std::string digits = std::to_string(std::max(0, value));
    for (int insert_at = static_cast<int>(digits.size()) - 3; insert_at > 0; insert_at -= 3) {
        digits.insert(static_cast<size_t>(insert_at), ",");
    }
    return digits;
}

void draw_source_tag(Rectangle rect, content_status status, unsigned char alpha) {
    const Color color = content_status_badge::color(status);
    ui::draw_rect_f(rect, with_alpha(g_theme->row_soft, static_cast<unsigned char>(70.0f * (static_cast<float>(alpha) / 255.0f))));
    ui::draw_rect_lines(rect, 1.0f, with_alpha(color, alpha));
    ui::draw_text_in_rect(content_status_badge::label(status), 9, rect,
                          with_alpha(color, alpha), ui::text_align::center);
}

unsigned char scale_alpha(unsigned char alpha, float scale) {
    return static_cast<unsigned char>(std::clamp(static_cast<float>(alpha) * scale, 0.0f, 255.0f));
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

Rectangle song_status_badge_rect(Rectangle main_column_rect) {
    return {
        main_column_rect.x + main_column_rect.width - kHeaderPaddingX - kStatusBadgeWidth,
        main_column_rect.y + kTitleOffsetY + 8.0f,
        kStatusBadgeWidth,
        kStatusBadgeHeight,
    };
}

Rectangle chart_status_badge_rect(Rectangle chart_detail_rect) {
    return {
        chart_detail_rect.x,
        chart_detail_rect.y + kChartStatusOffsetY,
        kStatusBadgeWidth,
        kStatusBadgeHeight,
    };
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

    if (config.compact_song_header) {
        ui::draw_text_in_rect("曲情報", 16,
                              {config.main_column_rect.x + 28.0f, config.main_column_rect.y + 20.0f,
                               config.main_column_rect.width - 56.0f, 24.0f},
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        const Rectangle title_rect = {
            config.chart_detail_rect.x,
            config.chart_detail_rect.y,
            config.chart_detail_rect.width,
            42.0f,
        };
        draw_marquee_text(song->song.meta.title.c_str(), title_rect,
                          27, with_alpha(t.text, config.alpha), config.now);
        draw_marquee_text(song->song.meta.artist.c_str(),
                          {title_rect.x, title_rect.y + 34.0f, title_rect.width, 22.0f},
                          16, with_alpha(t.text_secondary, config.alpha), config.now);
        ui::draw_text_in_rect("BPM", 12,
                              {title_rect.x, title_rect.y + 70.0f, 56.0f, 18.0f},
                              with_alpha(t.accent, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%.0f", song->song.meta.base_bpm), 18,
                              {title_rect.x, title_rect.y + 90.0f, 86.0f, 24.0f},
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect("ジャンル", 12,
                              {title_rect.x + 118.0f, title_rect.y + 70.0f, 160.0f, 18.0f},
                              with_alpha(t.accent, config.alpha), ui::text_align::left);
        float tag_x = title_rect.x + 118.0f;
        const float tag_y = title_rect.y + 92.0f;
        int drawn = 0;
        const auto draw_tag = [&](const std::string& label) {
            if (label.empty() || drawn >= 3) {
                return;
            }
            const float width = std::clamp(ui::measure_text_size(label.c_str(), 12.0f).x + 20.0f, 70.0f, 132.0f);
            if (tag_x + width > title_rect.x + title_rect.width) {
                return;
            }
            const Rectangle rect = {tag_x, tag_y, width, 26.0f};
            ui::draw_rect_f(rect, with_alpha(t.row_soft, static_cast<unsigned char>(config.normal_row_alpha * 0.85f)));
            ui::draw_rect_lines(rect, 1.0f, with_alpha(t.accent, config.alpha));
            ui::draw_text_in_rect(label.c_str(), 12, rect, with_alpha(t.accent, config.alpha), ui::text_align::center);
            tag_x += width + 8.0f;
            ++drawn;
        };
        for (const std::string& label : song->song.meta.genres) {
            draw_tag(label);
        }
        if (song->song.meta.genres.empty()) {
            draw_tag(song->song.meta.genre);
        }
        ui::draw_text_in_rect("キーワード", 12,
                              {title_rect.x, title_rect.y + 120.0f, title_rect.width, 18.0f},
                              with_alpha(t.accent, config.alpha), ui::text_align::left);
        float keyword_x = title_rect.x;
        const float keyword_y = title_rect.y + 140.0f;
        int keywords = 0;
        for (const std::string& keyword : song->song.meta.keywords) {
            if (keyword.empty() || keywords >= 3) {
                continue;
            }
            const float width = std::clamp(ui::measure_text_size(keyword.c_str(), 12.0f).x + 20.0f, 76.0f, 132.0f);
            if (keyword_x + width > title_rect.x + title_rect.width) {
                break;
            }
            const Rectangle rect = {keyword_x, keyword_y, width, 26.0f};
            ui::draw_rect_f(rect, with_alpha(t.row_soft, static_cast<unsigned char>(config.normal_row_alpha * 0.65f)));
            ui::draw_rect_lines(rect, 1.0f, with_alpha(t.border_light, config.alpha));
            ui::draw_text_in_rect(keyword.c_str(), 12, rect, with_alpha(t.text_secondary, config.alpha), ui::text_align::center);
            keyword_x += width + 8.0f;
            ++keywords;
        }
        if (keywords == 0) {
            ui::draw_text_in_rect("-", 14,
                                  {keyword_x, keyword_y + 2.0f, 40.0f, 22.0f},
                                  with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        }
    } else {
        const Rectangle title_rect = {
            config.main_column_rect.x + kHeaderPaddingX,
            config.main_column_rect.y + kTitleOffsetY,
            config.main_column_rect.width - kHeaderPaddingX * 2.0f,
            kTitleHeight
        };
        const Rectangle song_title_text_rect = {
            title_rect.x,
            title_rect.y,
            title_rect.width - kStatusBadgeWidth - 8.0f,
            title_rect.height,
        };
        const Rectangle artist_rect = {
            config.main_column_rect.x + kHeaderPaddingX,
            config.main_column_rect.y + kArtistOffsetY,
            config.main_column_rect.width - kHeaderPaddingX * 2.0f,
            kArtistHeight
        };
        draw_marquee_text(song->song.meta.title.c_str(),
                          song_title_text_rect,
                          28, with_alpha(t.text, config.alpha), config.now);
        content_status_badge::draw_compound(
            song_status_badge_rect(config.main_column_rect),
            song->source_status, song->status, config.alpha, 12);
        draw_marquee_text(song->song.meta.artist.c_str(),
                          artist_rect,
                          18, with_alpha(t.text_secondary, config.alpha), config.now);
    }
    if (chart != nullptr && !config.compact_song_header) {
        const Rectangle difficulty_rect = {config.chart_detail_rect.x, config.chart_detail_rect.y + kChartDifficultyOffsetY,
                                           config.chart_detail_rect.width - 78.0f, kChartDifficultyHeight};
        ui::draw_text_in_rect(chart->meta.difficulty.c_str(),
                              18,
                              difficulty_rect,
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        draw_difficulty_level_badge(
            chart->meta.level,
            {difficulty_rect.x + difficulty_rect.width + 8.0f, difficulty_rect.y + 1.0f, 70.0f, 22.0f},
            13, config.alpha);
        content_status_badge::draw_compound(
            chart_status_badge_rect(config.chart_detail_rect),
            chart->source_status, chart->status, config.alpha, 12);
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

    if (config.compact_song_header) {
        ui::draw_text_in_rect("譜面", 16,
                              {config.chart_buttons_rect.x, config.chart_buttons_rect.y - 72.0f,
                               config.chart_buttons_rect.width, 24.0f},
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        ui::draw_rect_f({config.chart_buttons_rect.x, config.chart_buttons_rect.y - 8.0f,
                         config.chart_buttons_rect.width, 1.0f},
                        with_alpha(t.border_light, config.alpha));
        const Rectangle header = {config.chart_buttons_rect.x, config.chart_buttons_rect.y - 40.0f,
                                  config.chart_buttons_rect.width, 22.0f};
        ui::draw_text_in_rect("キー", 10, {header.x + 16.0f, header.y, 54.0f, header.height},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect("レベル", 10, {header.x + 86.0f, header.y, 80.0f, header.height},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::center);
        ui::draw_text_in_rect("難易度", 10, {header.x + 184.0f, header.y, 120.0f, header.height},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect("SOURCE", 10, {header.x + 330.0f, header.y, 76.0f, header.height},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::center);
        ui::draw_text_in_rect("BEST", 10, {header.x + 430.0f, header.y, 116.0f, header.height},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
        ui::draw_text_in_rect("GRADE", 10, {header.x + header.width - 58.0f, header.y, 44.0f, header.height},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
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
        if (config.compact_song_header) {
            const Color level_color = difficulty_level_color(item.meta.level);
            ui::draw_rect_f(row, with_alpha(selected ? config.button_selected : config.button_base, row_alpha));
            ui::draw_rect_lines(row, selected ? 1.5f : 1.0f,
                                with_alpha(selected ? t.border_active : t.border_light, config.alpha));
            ui::draw_rect_f({row.x + 2.0f, row.y + 2.0f, 3.0f, row.height - 4.0f},
                            with_alpha(level_color, scale_alpha(config.alpha, 0.72f)));
            ui::draw_text_in_rect(key_mode_label(item.meta.key_count).c_str(), 18,
                                  {row.x + 16.0f, row.y + 13.0f, 54.0f, 26.0f},
                                  with_alpha(t.text_secondary, config.alpha), ui::text_align::left);
            draw_difficulty_level_badge(item.meta.level,
                                        {row.x + 86.0f, row.y + 16.0f, 72.0f, 22.0f},
                                        12, config.alpha);
            ui::draw_text_in_rect(item.meta.difficulty.c_str(), 18,
                                  {row.x + 184.0f, row.y + 12.0f, 110.0f, 28.0f},
                                  with_alpha(t.text, config.alpha), ui::text_align::left);
            draw_source_tag({row.x + 332.0f, row.y + 17.0f, 72.0f, 20.0f},
                            item.source_status, config.alpha);
            ui::draw_text_in_rect(item.best_local_score.has_value() ? format_score(*item.best_local_score).c_str() : "--",
                                  16, {row.x + 430.0f, row.y + 14.0f, 116.0f, 28.0f},
                                  with_alpha(t.text, config.alpha), ui::text_align::right);
            if (item.best_local_rank.has_value()) {
                ui::draw_text_in_rect(rank_label(*item.best_local_rank), 19,
                                      {row.x + row.width - 58.0f, row.y + 13.0f, 42.0f, 28.0f},
                                      with_alpha(rank_color(*item.best_local_rank), config.alpha), ui::text_align::right);
            } else {
                ui::draw_text_in_rect("-", 16,
                                      {row.x + row.width - 58.0f, row.y + 14.0f, 42.0f, 28.0f},
                                      with_alpha(t.text_muted, config.alpha), ui::text_align::right);
            }
            continue;
        }
        ui::draw_rect_f(row, with_alpha(selected ? config.button_selected : config.button_base, row_alpha));
        ui::draw_rect_lines(
            row, kChartRowBorderWidth,
            with_alpha(t.border_light, static_cast<unsigned char>(130.0f * config.play_t)));
        ui::draw_text_in_rect(item.meta.difficulty.c_str(), 16,
                              {row.x + kChartTextPaddingX, row.y + kChartTitleOffsetY,
                               row.width - kChartRightReserved - kChartStatusWidth, kChartTitleHeight},
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        content_status_badge::draw_compound(
                              {row.x + row.width - kChartRightReserved - kChartStatusWidth,
                               row.y + kChartTitleOffsetY + 1.0f,
                               kChartStatusWidth, kStatusBadgeHeight},
                              item.source_status, item.status, config.alpha, 10);
        draw_difficulty_level_badge(item.meta.level,
                                    {row.x + kChartTextPaddingX, row.y + kChartLevelOffsetY - 1.0f,
                                     64.0f, 19.0f},
                                    11, config.alpha);
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
