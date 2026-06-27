#include "title/center_panel_view.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_layout.h"
#include "ui_tooltip.h"
#include "ui/icons/raythm_icons.h"
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

struct jacket_layout {
    Rectangle frame;
    Rectangle inner;
};

struct compact_song_info_layout {
    Rectangle heading;
    Rectangle title;
    Rectangle artist;
    Rectangle bpm_label;
    Rectangle bpm_value;
    Rectangle genre_label;
    Rectangle genre_lane;
    Rectangle keyword_label;
    Rectangle keyword_lane;
};

struct normal_song_header_layout {
    Rectangle title;
    Rectangle title_text;
    Rectangle status_badge;
    Rectangle artist;
};

struct chart_detail_layout {
    Rectangle difficulty;
    Rectangle level_badge;
    Rectangle status_badge;
    Rectangle status_icon;
    Rectangle notes;
    Rectangle bpm;
    Rectangle author;
    Rectangle lock;
};

struct compact_chart_header_layout {
    Rectangle title;
    Rectangle divider;
    Rectangle row;
    Rectangle key;
    Rectangle level;
    Rectangle difficulty;
    Rectangle source;
    Rectangle best;
    Rectangle grade;
};

struct compact_chart_row_layout {
    Rectangle accent;
    Rectangle key;
    Rectangle level_badge;
    Rectangle difficulty;
    Rectangle source;
    Rectangle status_icon;
    Rectangle best_score;
    Rectangle best_rank;
    Rectangle missing_rank;
};

struct classic_chart_row_layout {
    Rectangle difficulty;
    Rectangle status;
    Rectangle status_icon;
    Rectangle level_badge;
    Rectangle key_mode;
    Rectangle rank;
};

jacket_layout make_jacket_layout(Rectangle jacket_rect) {
    return {
        {
            jacket_rect.x - kJacketFramePadding,
            jacket_rect.y - kJacketFramePadding,
            jacket_rect.width + kJacketFramePadding * 2.0f,
            jacket_rect.height + kJacketFramePadding * 2.0f,
        },
        ui::inset(jacket_rect, kJacketInnerPadding),
    };
}

Rectangle make_song_status_badge_rect(Rectangle main_column_rect) {
    return {
        main_column_rect.x + main_column_rect.width - kHeaderPaddingX - kStatusBadgeWidth,
        main_column_rect.y + kTitleOffsetY + 8.0f,
        kStatusBadgeWidth,
        kStatusBadgeHeight,
    };
}

Rectangle make_chart_status_badge_rect(Rectangle chart_detail_rect) {
    return {
        chart_detail_rect.x,
        chart_detail_rect.y + kChartStatusOffsetY,
        kStatusBadgeWidth,
        kStatusBadgeHeight,
    };
}

compact_song_info_layout make_compact_song_info_layout(Rectangle main_column_rect, Rectangle chart_detail_rect) {
    const Rectangle title = {
        chart_detail_rect.x,
        chart_detail_rect.y,
        chart_detail_rect.width,
        42.0f,
    };
    return {
        {
            main_column_rect.x + 28.0f,
            main_column_rect.y + 20.0f,
            main_column_rect.width - 56.0f,
            24.0f,
        },
        title,
        {title.x, title.y + 34.0f, title.width, 22.0f},
        {title.x, title.y + 70.0f, 56.0f, 18.0f},
        {title.x, title.y + 90.0f, 86.0f, 24.0f},
        {title.x + 118.0f, title.y + 70.0f, 160.0f, 18.0f},
        {title.x + 118.0f, title.y + 92.0f, title.width - 118.0f, 26.0f},
        {title.x, title.y + 120.0f, title.width, 18.0f},
        {title.x, title.y + 140.0f, title.width, 26.0f},
    };
}

normal_song_header_layout make_normal_song_header_layout(Rectangle main_column_rect) {
    const Rectangle title = {
        main_column_rect.x + kHeaderPaddingX,
        main_column_rect.y + kTitleOffsetY,
        main_column_rect.width - kHeaderPaddingX * 2.0f,
        kTitleHeight,
    };
    return {
        title,
        {title.x, title.y, title.width - kStatusBadgeWidth - 8.0f, title.height},
        make_song_status_badge_rect(main_column_rect),
        {
            main_column_rect.x + kHeaderPaddingX,
            main_column_rect.y + kArtistOffsetY,
            main_column_rect.width - kHeaderPaddingX * 2.0f,
            kArtistHeight,
        },
    };
}

chart_detail_layout make_chart_detail_layout(Rectangle chart_detail_rect) {
    const Rectangle difficulty = {
        chart_detail_rect.x,
        chart_detail_rect.y + kChartDifficultyOffsetY,
        chart_detail_rect.width - 78.0f,
        kChartDifficultyHeight,
    };
    const Rectangle status = make_chart_status_badge_rect(chart_detail_rect);
    return {
        difficulty,
        {difficulty.x + difficulty.width + 8.0f, difficulty.y + 1.0f, 70.0f, 22.0f},
        status,
        {chart_detail_rect.x + kStatusBadgeWidth + 10.0f,
         chart_detail_rect.y + kChartStatusOffsetY + 2.0f,
         20.0f,
         20.0f},
        {chart_detail_rect.x, chart_detail_rect.y + kChartNotesOffsetY,
         chart_detail_rect.width, kChartNotesHeight},
        {chart_detail_rect.x, chart_detail_rect.y + kChartBpmOffsetY,
         chart_detail_rect.width, kChartBpmHeight},
        {chart_detail_rect.x, chart_detail_rect.y + kChartAuthorOffsetY,
         chart_detail_rect.width, kChartAuthorHeight},
        {chart_detail_rect.x, chart_detail_rect.y,
         chart_detail_rect.width, kChartAuthorOffsetY + kChartAuthorHeight},
    };
}

compact_chart_header_layout make_compact_chart_header_layout(Rectangle chart_buttons_rect) {
    const Rectangle header = {
        chart_buttons_rect.x,
        chart_buttons_rect.y - 40.0f,
        chart_buttons_rect.width,
        22.0f,
    };
    return {
        {chart_buttons_rect.x, chart_buttons_rect.y - 72.0f, chart_buttons_rect.width, 24.0f},
        {chart_buttons_rect.x, chart_buttons_rect.y - 8.0f, chart_buttons_rect.width, 1.0f},
        header,
        {header.x + 16.0f, header.y, 54.0f, header.height},
        {header.x + 86.0f, header.y, 80.0f, header.height},
        {header.x + 184.0f, header.y, 120.0f, header.height},
        {header.x + 330.0f, header.y, 76.0f, header.height},
        {header.x + 430.0f, header.y, 116.0f, header.height},
        {header.x + header.width - 58.0f, header.y, 44.0f, header.height},
    };
}

compact_chart_row_layout make_compact_chart_row_layout(Rectangle row) {
    return {
        {row.x + 2.0f, row.y + 2.0f, 3.0f, row.height - 4.0f},
        {row.x + 16.0f, row.y + 13.0f, 54.0f, 26.0f},
        {row.x + 86.0f, row.y + 16.0f, 72.0f, 22.0f},
        {row.x + 184.0f, row.y + 12.0f, 110.0f, 28.0f},
        {row.x + 332.0f, row.y + 17.0f, 72.0f, 20.0f},
        {row.x + 410.0f, row.y + 18.0f, 18.0f, 18.0f},
        {row.x + 430.0f, row.y + 14.0f, 116.0f, 28.0f},
        {row.x + row.width - 58.0f, row.y + 13.0f, 42.0f, 28.0f},
        {row.x + row.width - 58.0f, row.y + 14.0f, 42.0f, 28.0f},
    };
}

classic_chart_row_layout make_classic_chart_row_layout(Rectangle row) {
    const Rectangle status = {
        row.x + row.width - kChartRightReserved - kChartStatusWidth,
        row.y + kChartTitleOffsetY + 1.0f,
        kChartStatusWidth,
        kStatusBadgeHeight,
    };
    return {
        {row.x + kChartTextPaddingX, row.y + kChartTitleOffsetY,
         row.width - kChartRightReserved - kChartStatusWidth, kChartTitleHeight},
        status,
        {status.x + status.width + 8.0f, status.y + 3.0f, 18.0f, 18.0f},
        {row.x + kChartTextPaddingX, row.y + kChartLevelOffsetY - 1.0f, 64.0f, 19.0f},
        {row.x + row.width - kChartBadgeRightInset, row.y + kChartBadgeOffsetY,
         kChartBadgeWidth, kChartBadgeHeight},
        {row.x + row.width - kChartBadgeRightInset, row.y + kChartRankOffsetY,
         kChartBadgeWidth, kChartRankHeight},
    };
}

bool rect_visible_in(Rectangle viewport, Rectangle rect) {
    return rect.y + rect.height >= viewport.y - kClipSlack &&
           rect.y <= viewport.y + viewport.height + kClipSlack;
}

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
    ui::surface(rect,
                with_alpha(g_theme->row_soft, static_cast<unsigned char>(70.0f * (static_cast<float>(alpha) / 255.0f))),
                with_alpha(color, alpha),
                1.0f);
    ui::draw_text_in_rect(content_status_badge::label(status), 9, rect,
                          with_alpha(color, alpha), ui::text_align::center);
}

void draw_status_side_icons(Rectangle first_icon_rect,
                            bool modified,
                            unsigned char alpha) {
    if (modified) {
        raythm_icons::draw_triangle_alert(first_icon_rect, with_alpha(g_theme->slow, alpha), 2.4f);
        ui::enqueue_hover_tooltip(first_icon_rect, "変更されています", alpha);
    }
}

unsigned char scale_alpha(unsigned char alpha, float scale) {
    return static_cast<unsigned char>(std::clamp(static_cast<float>(alpha) * scale, 0.0f, 255.0f));
}

Rectangle centered_square(Rectangle rect, float size) {
    return {
        rect.x + (rect.width - size) * 0.5f,
        rect.y + (rect.height - size) * 0.5f,
        size,
        size,
    };
}

void draw_locked_overlay(Rectangle rect, unsigned char alpha, float icon_size) {
    ui::backdrop(rect, with_alpha(g_theme->bg, scale_alpha(alpha, 0.48f)));
    raythm_icons::draw_lock(centered_square(rect, icon_size), with_alpha(g_theme->slow, alpha), 3.0f);
}

void draw_preview_jacket(const title_preview_snapshot& preview,
                         const title_center_view::draw_config& config) {
    const auto& t = *g_theme;
    const jacket_layout layout = make_jacket_layout(config.jacket_rect);
    ui::frame(layout.frame,
              with_alpha(t.border_image, static_cast<unsigned char>(190.0f * config.play_t)),
              kJacketBorderWidth);
    if (preview.jacket.status == song_select::jacket_loader::load_status::ready &&
        preview.jacket_texture != nullptr) {
        const Texture2D& jacket = *preview.jacket_texture;
        ui::draw_texture(jacket, layout.inner, with_alpha(WHITE, config.alpha));
    } else if (preview.jacket.status == song_select::jacket_loader::load_status::loading) {
        ui::draw_text_in_rect("読み込み中", 22, layout.inner, with_alpha(t.text_muted, config.alpha));
    } else {
        ui::draw_text_in_rect("JACKET", 26, layout.inner, with_alpha(t.text_muted, config.alpha));
    }
}

void draw_compact_song_chip(Rectangle rect,
                            const char* label,
                            Color fill,
                            Color border,
                            Color text) {
    ui::surface(rect, fill, border, 1.0f);
    ui::draw_text_in_rect(label, 12, rect, text, ui::text_align::center);
}

void draw_compact_song_info(const song_select::song_entry& song,
                            const title_center_view::draw_config& config) {
    const auto& t = *g_theme;
    const compact_song_info_layout layout =
        make_compact_song_info_layout(config.main_column_rect, config.chart_detail_rect);
    ui::draw_text_in_rect("曲情報", 16,
                          layout.heading,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    draw_marquee_text(song.song.meta.title.c_str(), layout.title,
                      27, with_alpha(t.text, config.alpha), config.now);
    draw_marquee_text(song.song.meta.artist.c_str(),
                      layout.artist,
                      16, with_alpha(t.text_secondary, config.alpha), config.now);
    ui::draw_text_in_rect("BPM", 12,
                          layout.bpm_label,
                          with_alpha(t.accent, config.alpha), ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("%.0f", song.song.meta.base_bpm), 18,
                          layout.bpm_value,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    ui::draw_text_in_rect("ジャンル", 12,
                          layout.genre_label,
                          with_alpha(t.accent, config.alpha), ui::text_align::left);

    float tag_x = layout.genre_lane.x;
    int drawn = 0;
    const auto draw_genre_tag = [&](const std::string& label) {
        if (label.empty() || drawn >= 3) {
            return;
        }
        const float width = std::clamp(ui::measure_text_size(label.c_str(), 12.0f).x + 20.0f, 70.0f, 132.0f);
        if (tag_x + width > layout.genre_lane.x + layout.genre_lane.width) {
            return;
        }
        draw_compact_song_chip({tag_x, layout.genre_lane.y, width, layout.genre_lane.height},
                               label.c_str(),
                               with_alpha(t.row_soft, static_cast<unsigned char>(config.normal_row_alpha * 0.85f)),
                               with_alpha(t.accent, config.alpha),
                               with_alpha(t.accent, config.alpha));
        tag_x += width + 8.0f;
        ++drawn;
    };
    for (const std::string& label : song.song.meta.genres) {
        draw_genre_tag(label);
    }
    if (song.song.meta.genres.empty()) {
        draw_genre_tag(song.song.meta.genre);
    }

    ui::draw_text_in_rect("キーワード", 12,
                          layout.keyword_label,
                          with_alpha(t.accent, config.alpha), ui::text_align::left);
    float keyword_x = layout.keyword_lane.x;
    int keywords = 0;
    for (const std::string& keyword : song.song.meta.keywords) {
        if (keyword.empty() || keywords >= 3) {
            continue;
        }
        const float width = std::clamp(ui::measure_text_size(keyword.c_str(), 12.0f).x + 20.0f, 76.0f, 132.0f);
        if (keyword_x + width > layout.keyword_lane.x + layout.keyword_lane.width) {
            break;
        }
        draw_compact_song_chip({keyword_x, layout.keyword_lane.y, width, layout.keyword_lane.height},
                               keyword.c_str(),
                               with_alpha(t.row_soft, static_cast<unsigned char>(config.normal_row_alpha * 0.65f)),
                               with_alpha(t.border_light, config.alpha),
                               with_alpha(t.text_secondary, config.alpha));
        keyword_x += width + 8.0f;
        ++keywords;
    }
    if (keywords == 0) {
        ui::draw_text_in_rect("-", 14,
                              {keyword_x, layout.keyword_lane.y + 2.0f, 40.0f, 22.0f},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::left);
    }
}

void draw_normal_song_header(const song_select::song_entry& song,
                             const title_center_view::draw_config& config) {
    const auto& t = *g_theme;
    const normal_song_header_layout layout = make_normal_song_header_layout(config.main_column_rect);
    draw_marquee_text(song.song.meta.title.c_str(),
                      layout.title_text,
                      28, with_alpha(t.text, config.alpha), config.now);
    content_status_badge::draw_compound(
        layout.status_badge,
        song.source_status, song.status, config.alpha, 12);
    draw_marquee_text(song.song.meta.artist.c_str(),
                      layout.artist,
                      18, with_alpha(t.text_secondary, config.alpha), config.now);
}

void draw_chart_detail(const song_select::song_entry& song,
                       const song_select::chart_option& chart,
                       const title_center_view::draw_config& config) {
    const auto& t = *g_theme;
    const chart_detail_layout layout = make_chart_detail_layout(config.chart_detail_rect);
    ui::draw_text_in_rect(chart.meta.difficulty.c_str(),
                          18,
                          layout.difficulty,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    draw_difficulty_level_badge(chart.meta.level, layout.level_badge, 13, config.alpha);
    content_status_badge::draw_compound(
        layout.status_badge,
        chart.source_status, chart.status, config.alpha, 12);
    draw_status_side_icons(layout.status_icon,
                           chart.status == content_status::modified,
                           config.alpha);
    ui::draw_text_in_rect(TextFormat("%s   %d Notes", key_mode_label(chart.meta.key_count).c_str(),
                                     chart.note_count),
                          14,
                          layout.notes,
                          with_alpha(t.text_secondary, config.alpha), ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("BPM %s", format_bpm_range(chart.min_bpm, chart.max_bpm).c_str()),
                          14,
                          layout.bpm,
                          with_alpha(t.text_muted, config.alpha), ui::text_align::left);
    draw_marquee_text(chart.meta.chart_author.c_str(),
                      layout.author,
                      14, with_alpha(t.text_muted, config.alpha), config.now);
    if (content_is_play_locked(song.song.meta, chart.meta)) {
        draw_locked_overlay(layout.lock, config.alpha, 34.0f);
    }
}

void draw_compact_chart_header(Rectangle chart_buttons_rect, unsigned char alpha) {
    const auto& t = *g_theme;
    const compact_chart_header_layout layout = make_compact_chart_header_layout(chart_buttons_rect);
    ui::draw_text_in_rect("譜面", 16,
                          layout.title,
                          with_alpha(t.text, alpha), ui::text_align::left);
    ui::divider(layout.divider, with_alpha(t.border_light, alpha));
    ui::draw_text_in_rect("キー", 10, layout.key,
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    ui::draw_text_in_rect("レベル", 10, layout.level,
                          with_alpha(t.text_muted, alpha), ui::text_align::center);
    ui::draw_text_in_rect("難易度", 10, layout.difficulty,
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    ui::draw_text_in_rect("SOURCE", 10, layout.source,
                          with_alpha(t.text_muted, alpha), ui::text_align::center);
    ui::draw_text_in_rect("BEST", 10, layout.best,
                          with_alpha(t.text_muted, alpha), ui::text_align::right);
    ui::draw_text_in_rect("GRADE", 10, layout.grade,
                          with_alpha(t.text_muted, alpha), ui::text_align::right);
}

void draw_compact_chart_row(const song_select::chart_option& item,
                            Rectangle row,
                            bool selected,
                            bool locked,
                            unsigned char row_alpha,
                            const title_center_view::draw_config& config) {
    const auto& t = *g_theme;
    const compact_chart_row_layout layout = make_compact_chart_row_layout(row);
    const Color level_color = difficulty_level_color(item.meta.level);
    const Color row_fill = locked
        ? lerp_color(config.button_base, t.bg, selected ? 0.48f : 0.36f)
        : (selected ? config.button_selected : config.button_base);
    ui::surface(row,
                with_alpha(row_fill, row_alpha),
                with_alpha(locked ? t.slow : selected ? t.border_active : t.border_light,
                           locked ? scale_alpha(config.alpha, 0.72f) : config.alpha),
                selected ? 1.5f : 1.0f);
    ui::accent_bar(layout.accent, with_alpha(level_color, scale_alpha(config.alpha, 0.72f)));
    ui::draw_text_in_rect(key_mode_label(item.meta.key_count).c_str(), 18,
                          layout.key,
                          with_alpha(t.text_secondary, config.alpha), ui::text_align::left);
    draw_difficulty_level_badge(item.meta.level, layout.level_badge, 12, config.alpha);
    ui::draw_text_in_rect(item.meta.difficulty.c_str(), 18,
                          layout.difficulty,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    draw_source_tag(layout.source, item.source_status, config.alpha);
    draw_status_side_icons(layout.status_icon,
                           item.status == content_status::modified,
                           config.alpha);
    const std::string score_label = item.best_local_score.has_value()
        ? format_score(*item.best_local_score)
        : "--";
    ui::draw_text_in_rect(score_label.c_str(),
                          16, layout.best_score,
                          with_alpha(t.text, config.alpha), ui::text_align::right);
    if (item.best_local_rank.has_value()) {
        ui::draw_text_in_rect(rank_label(*item.best_local_rank), 19,
                              layout.best_rank,
                              with_alpha(rank_color(*item.best_local_rank), config.alpha), ui::text_align::right);
    } else {
        ui::draw_text_in_rect("-", 16,
                              layout.missing_rank,
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
    }
    if (locked) {
        draw_locked_overlay(row, config.alpha, 26.0f);
    }
}

void draw_classic_chart_row(const song_select::chart_option& item,
                            Rectangle row,
                            bool selected,
                            bool locked,
                            unsigned char row_alpha,
                            const title_center_view::draw_config& config) {
    const auto& t = *g_theme;
    const classic_chart_row_layout layout = make_classic_chart_row_layout(row);
    const Color row_fill = locked
        ? lerp_color(config.button_base, t.bg, selected ? 0.48f : 0.36f)
        : (selected ? config.button_selected : config.button_base);
    ui::surface(row,
                with_alpha(row_fill, row_alpha),
                with_alpha(locked ? t.slow : t.border_light,
                           static_cast<unsigned char>((locked ? 170.0f : 130.0f) * config.play_t)),
                kChartRowBorderWidth);
    ui::draw_text_in_rect(item.meta.difficulty.c_str(), 16,
                          layout.difficulty,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    content_status_badge::draw_compound(
        layout.status,
        item.source_status, item.status, config.alpha, 10);
    draw_status_side_icons(layout.status_icon,
                           item.status == content_status::modified,
                           config.alpha);
    draw_difficulty_level_badge(item.meta.level, layout.level_badge, 11, config.alpha);
    ui::draw_text_in_rect(key_mode_label(item.meta.key_count).c_str(), 13,
                          layout.key_mode,
                          with_alpha(key_mode_color(item.meta.key_count), config.alpha), ui::text_align::right);
    if (item.best_local_rank.has_value()) {
        ui::draw_text_in_rect(rank_label(*item.best_local_rank), 12,
                              layout.rank,
                              with_alpha(rank_color(*item.best_local_rank), config.alpha), ui::text_align::right);
    }
    if (locked) {
        draw_locked_overlay(row, config.alpha, 26.0f);
    }
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
    if (!ui::contains_point(area, point)) {
        return -1;
    }
    for (int index = 0; index < count; ++index) {
        if (ui::contains_point(chart_button_rect(area, index, scroll_y), point)) {
            return index;
        }
    }
    return -1;
}

Rectangle song_status_badge_rect(Rectangle main_column_rect) {
    return make_song_status_badge_rect(main_column_rect);
}

Rectangle chart_status_badge_rect(Rectangle chart_detail_rect) {
    return make_chart_status_badge_rect(chart_detail_rect);
}

void draw(const song_select::state& state,
          const title_preview_snapshot& preview,
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

    draw_preview_jacket(preview, config);

    if (config.compact_song_header) {
        draw_compact_song_info(*song, config);
    } else {
        draw_normal_song_header(*song, config);
    }
    if (chart != nullptr && !config.compact_song_header) {
        draw_chart_detail(*song, *chart, config);
    }

    if (config.compact_song_header) {
        draw_compact_chart_header(config.chart_buttons_rect, config.alpha);
    }

    ui::scoped_clip_rect clip(config.chart_buttons_rect);
    for (int i = 0; i < static_cast<int>(filtered.size()); ++i) {
        const song_select::chart_option& item = *filtered[static_cast<size_t>(i)];
        const Rectangle row = chart_button_rect(config.chart_buttons_rect, i, state.chart_scroll_y);
        if (!rect_visible_in(config.chart_buttons_rect, row)) {
            continue;
        }

        const bool selected = i == state.difficulty_index;
        const bool hovered = ui::is_hovered(row);
        const bool locked = content_is_play_locked(song->song.meta, item.meta);
        const unsigned char row_alpha = selected ? config.selected_row_alpha
            : hovered ? config.hover_row_alpha
                      : config.normal_row_alpha;
        if (config.compact_song_header) {
            draw_compact_chart_row(item, row, selected, locked, row_alpha, config);
            continue;
        }
        draw_classic_chart_row(item, row, selected, locked, row_alpha, config);
    }
}

}  // namespace title_center_view
