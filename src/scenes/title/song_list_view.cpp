#include "title/song_list_view.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "scene_common.h"
#include "shared/content_status_badge.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_layout.h"
#include "ui_scroll.h"
#include "ui_tooltip.h"
#include "ui/icons/raythm_icons.h"

namespace {

constexpr float kSongRowHeight = 96.0f;
constexpr float kExpandedSongRowHeight = 344.0f;
constexpr float kSongRowGap = 10.0f;
constexpr float kScrollPadding = 36.0f;
constexpr float kInitialRowOffsetY = 18.0f;
constexpr float kSongCountOffsetY = 36.0f;
constexpr float kSongCountHeight = 27.0f;
constexpr float kClipSlack = 6.0f;
constexpr float kRowBorderWidth = 1.35f;
constexpr float kClassicRowBorderWidth = 1.5f;
constexpr float kTextPaddingX = 21.0f;
constexpr float kTitleOffsetY = 15.0f;
constexpr float kTitleHeight = 36.0f;
constexpr float kArtistOffsetY = 60.0f;
constexpr float kArtistHeight = 27.0f;
constexpr float kStatusBadgeWidth = 96.0f;
constexpr float kStatusBadgeHeight = 24.0f;
constexpr float kStatusBadgeInset = 15.0f;
constexpr float kClassicArtistDurationReserveWidth = 62.0f;
constexpr float kClassicDurationWidth = 58.0f;
constexpr float kClassicDurationHeight = 18.0f;
constexpr float kClassicDurationGap = 12.0f;
constexpr float kJacketSize = 72.0f;
constexpr float kExpandedJacketInsetX = 18.0f;
constexpr float kExpandedJacketInsetY = 14.0f;
constexpr float kExpandedTextGap = 20.0f;
constexpr float kExpandedRightColumnWidth = 104.0f;
constexpr float kExpandedRightInset = 26.0f;
constexpr float kExpandedTitleOffsetY = 15.0f;
constexpr float kExpandedTitleHeight = 24.0f;
constexpr float kExpandedArtistOffsetY = 41.0f;
constexpr float kExpandedArtistHeight = 18.0f;
constexpr float kExpandedTagOffsetY = 62.0f;
constexpr float kExpandedTagGap = 8.0f;
constexpr float kExpandedTagRightPadding = 16.0f;
constexpr float kExpandedStatusRightInset = 36.0f;
constexpr float kExpandedStatusWidth = 88.0f;
constexpr float kChartHeaderHeight = 24.0f;
constexpr float kChartHeaderGap = 5.0f;
constexpr float kChartRowHeight = 33.0f;
constexpr float kChartRowGap = 4.0f;
constexpr float kEmbeddedChartScrollPadding = 8.0f;
constexpr float kTagHeight = 23.0f;
constexpr float kTagFontSize = 11.0f;
constexpr float kChartMinDrawableHeight = 70.0f;
constexpr float kModifiedStatusWidth = 22.0f;

struct chart_columns {
    Rectangle key;
    Rectangle level;
    Rectangle difficulty;
    Rectangle creator;
    Rectangle notes;
    Rectangle status;
    Rectangle rank;
};

struct song_list_header_layout {
    Rectangle full;
    Rectangle title;
    Rectangle count;
};

struct compact_song_row_layout {
    Rectangle title;
    Rectangle artist;
    Rectangle duration;
    Rectangle status_badge;
};

struct expanded_song_row_layout {
    Rectangle jacket;
    Rectangle title;
    Rectangle artist;
    Rectangle tag_lane;
    Rectangle bpm;
    Rectangle plays;
    Rectangle status_tags;
};

struct chart_list_layout {
    Rectangle area;
    Rectangle header_row;
    Rectangle viewport;
};

struct song_row_entry {
    int visible_index = -1;
    int song_index = -1;
    Rectangle rect{};
};

struct song_row_view {
    const song_select::song_entry* song = nullptr;
    song_row_entry entry{};
    bool selected = false;
};

song_list_header_layout make_song_list_header_layout(Rectangle column) {
    const Rectangle full = {
        column.x,
        column.y - kSongCountOffsetY,
        column.width,
        kSongCountHeight,
    };
    const ui::rect_pair columns = ui::split_columns(full, full.width * 0.5f);
    return {full, columns.first, columns.second};
}

compact_song_row_layout make_compact_song_row_layout(Rectangle row, float artist_reserved_width) {
    const Rectangle badge = {
        row.x + row.width - kStatusBadgeWidth - kStatusBadgeInset,
        row.y + kStatusBadgeInset,
        kStatusBadgeWidth,
        kStatusBadgeHeight,
    };
    const float text_width = row.width - kTextPaddingX * 2.0f - kStatusBadgeWidth;
    return {
        {row.x + kTextPaddingX, row.y + kTitleOffsetY, text_width, kTitleHeight},
        {row.x + kTextPaddingX, row.y + kArtistOffsetY, text_width - artist_reserved_width, kArtistHeight},
        {badge.x - kClassicDurationWidth - kClassicDurationGap,
         row.y + kArtistOffsetY + 2.0f,
         kClassicDurationWidth,
         kClassicDurationHeight},
        badge,
    };
}

expanded_song_row_layout make_expanded_song_row_layout(Rectangle row) {
    const Rectangle jacket = {
        row.x + kExpandedJacketInsetX,
        row.y + kExpandedJacketInsetY,
        kJacketSize,
        kJacketSize,
    };
    const float text_x = jacket.x + jacket.width + kExpandedTextGap;
    const float right_x = row.x + row.width - kExpandedRightColumnWidth - kExpandedRightInset;
    const float text_width = std::max(120.0f, right_x - text_x - kExpandedTextGap);
    const float tag_y = row.y + kExpandedTagOffsetY;
    return {
        jacket,
        {text_x, row.y + kExpandedTitleOffsetY, text_width, kExpandedTitleHeight},
        {text_x, row.y + kExpandedArtistOffsetY, text_width, kExpandedArtistHeight},
        {text_x, tag_y, right_x - kExpandedTagRightPadding - text_x, kTagHeight},
        {right_x, row.y + kExpandedTitleOffsetY, kExpandedRightColumnWidth, 18.0f},
        {right_x, row.y + kExpandedArtistOffsetY, kExpandedRightColumnWidth, 18.0f},
        {row.x + row.width - kExpandedStatusRightInset - kExpandedStatusWidth,
         tag_y,
         kExpandedStatusWidth,
         kTagHeight},
    };
}

Rectangle chart_header_row_rect(Rectangle charts) {
    return {charts.x, charts.y - 3.0f, charts.width, kChartRowHeight};
}

chart_columns make_chart_columns(Rectangle row) {
    return {
        {row.x + 14.0f, row.y + 7.0f, 44.0f, 18.0f},
        {row.x + 68.0f, row.y + 5.0f, 70.0f, 22.0f},
        {row.x + 152.0f, row.y + 7.0f, 118.0f, 18.0f},
        {row.x + 288.0f, row.y + 7.0f, 112.0f, 18.0f},
        {row.x + row.width - 214.0f, row.y + 7.0f, 56.0f, 18.0f},
        {row.x + row.width - 128.0f, row.y + 6.0f, 76.0f, 20.0f},
        {row.x + row.width - 54.0f, row.y + 6.0f, 40.0f, 20.0f},
    };
}

chart_columns make_chart_header_columns(Rectangle charts) {
    return make_chart_columns(chart_header_row_rect(charts));
}

float row_height(const song_select::state& state, int index) {
    (void)state;
    (void)index;
    return kSongRowHeight;
}

template <typename VisitRow>
void for_each_song_row(const song_select::state& state, Rectangle area, float scroll_y, VisitRow visit_row) {
    const std::vector<int> indices = song_select::filtered_song_indices(state);
    float row_y = area.y + kInitialRowOffsetY - scroll_y;
    for (int visible = 0; visible < static_cast<int>(indices.size()); ++visible) {
        const int song_index = indices[static_cast<size_t>(visible)];
        const float height = row_height(state, song_index);
        const song_row_entry entry{
            .visible_index = visible,
            .song_index = song_index,
            .rect = {area.x, row_y, area.width, height},
        };
        if (!visit_row(entry)) {
            break;
        }
        row_y += height + kSongRowGap;
    }
}

std::vector<std::string> genre_labels(const song_meta& meta) {
    if (!meta.genres.empty()) {
        return meta.genres;
    }
    if (!meta.genre.empty()) {
        return {meta.genre};
    }
    return {};
}

Color tag_color_for_label(const std::string& label) {
    constexpr Color kPalette[] = {
        {147, 94, 226, 255},
        {38, 167, 216, 255},
        {214, 143, 43, 255},
        {132, 204, 45, 255},
        {216, 78, 133, 255},
        {62, 126, 220, 255},
        {39, 181, 154, 255},
        {218, 91, 61, 255},
        {190, 181, 48, 255},
        {162, 103, 231, 255},
        {65, 190, 96, 255},
        {212, 94, 172, 255},
    };
    unsigned int hash = 2166136261u;
    for (unsigned char ch : label) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<unsigned char>(ch - 'A' + 'a');
        }
        hash ^= ch;
        hash *= 16777619u;
    }
    return kPalette[hash % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

std::string format_duration_label(float seconds) {
    if (seconds <= 0.0f) {
        return "";
    }
    const int total_seconds = static_cast<int>(std::round(seconds));
    return TextFormat("%d:%02d", total_seconds / 60, total_seconds % 60);
}

std::string format_count_label(int count) {
    if (count >= 1000000) {
        return TextFormat("%.1fm", static_cast<float>(count) / 1000000.0f);
    }
    if (count >= 1000) {
        return TextFormat("%.1fk", static_cast<float>(count) / 1000.0f);
    }
    return TextFormat("%d", count);
}

std::string key_mode_label(int key_count) {
    return TextFormat("%dK", key_count > 0 ? key_count : 4);
}

Color key_mode_color(int key_count) {
    const auto& theme = *g_theme;
    if (key_count == 5) return theme.rank_b;
    if (key_count == 6) return theme.rank_c;
    if (key_count == 7) return theme.rank_a;
    return theme.rank_b;
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

Rectangle chart_area_rect(Rectangle row) {
    return {row.x + 18.0f, row.y + 126.0f, row.width - 36.0f, row.height - 144.0f};
}

chart_list_layout make_chart_list_layout_for_area(Rectangle area) {
    return {
        area,
        chart_header_row_rect(area),
        {
            area.x,
            area.y + kChartHeaderHeight + kChartHeaderGap,
            area.width,
            area.height - kChartHeaderHeight - kChartHeaderGap,
        },
    };
}

chart_list_layout make_chart_list_layout(Rectangle row) {
    return make_chart_list_layout_for_area(chart_area_rect(row));
}

constexpr Rectangle fixed_song_rows_viewport(Rectangle area) {
    return ui::vertical_viewport_after_top_inset(area, kInitialRowOffsetY);
}

song_row_entry fixed_song_row_entry_for(Rectangle area, int visible_index, int song_index, float scroll_y) {
    return {
        .visible_index = visible_index,
        .song_index = song_index,
        .rect = ui::vertical_list_row_rect(
            fixed_song_rows_viewport(area),
            visible_index,
            kSongRowHeight,
            kSongRowGap,
            scroll_y),
    };
}

song_row_view song_row_view_for(const song_select::state& state, const song_row_entry& entry) {
    return {
        .song = entry.song_index >= 0 && entry.song_index < static_cast<int>(state.songs.size())
            ? &state.songs[static_cast<size_t>(entry.song_index)]
            : nullptr,
        .entry = entry,
        .selected = entry.song_index == state.selected_song_index,
    };
}

Rectangle chart_list_viewport_rect(Rectangle chart_area) {
    return make_chart_list_layout_for_area(chart_area).viewport;
}

Rectangle chart_row_rect(Rectangle chart_area, int index, float scroll_y) {
    const Rectangle viewport = chart_list_viewport_rect(chart_area);
    return ui::vertical_list_row_rect(viewport, index, kChartRowHeight, kChartRowGap, scroll_y);
}

bool rect_visible_in(Rectangle viewport, Rectangle rect) {
    return ui::rect_visible_in_viewport(rect, viewport, kClipSlack);
}

void draw_tag(Rectangle rect, const std::string& label, unsigned char alpha) {
    const Color color = tag_color_for_label(label);
    ui::surface(rect,
                with_alpha(g_theme->row_soft, static_cast<unsigned char>(70.0f * (static_cast<float>(alpha) / 255.0f))),
                with_alpha(color, alpha),
                1.0f);
    ui::draw_text_in_rect(label.c_str(), static_cast<int>(kTagFontSize), rect, with_alpha(color, alpha), ui::text_align::center);
}

void draw_status_tag(Rectangle rect, content_status status, unsigned char alpha, int font_size = 11) {
    const Color color = content_status_badge::color(status);
    if (status == content_status::modified) {
        const Rectangle icon_rect = ui::inscribed_square(rect, 0.5f);
        raythm_icons::draw_triangle_alert(icon_rect, with_alpha(color, alpha), 2.6f);
        ui::enqueue_hover_tooltip(icon_rect, "変更されています", alpha);
        return;
    }

    ui::surface(rect,
                with_alpha(g_theme->row_soft, static_cast<unsigned char>(70.0f * (static_cast<float>(alpha) / 255.0f))),
                with_alpha(color, alpha),
                1.0f);
    ui::draw_text_in_rect(content_status_badge::label(status), font_size, rect,
                          with_alpha(color, alpha), ui::text_align::center);
}

void draw_lock_icon(Rectangle rect, unsigned char alpha) {
    raythm_icons::draw_lock(rect, with_alpha(g_theme->slow, alpha), 2.4f);
}

void draw_locked_row_overlay(Rectangle row, unsigned char alpha) {
    ui::backdrop(row, with_alpha(g_theme->bg, static_cast<unsigned char>((static_cast<int>(alpha) * 120) / 255)));
    draw_lock_icon(ui::centered_square(row, 24.0f), alpha);
}

float status_tag_gap(content_status status) {
    return status == content_status::modified ? 4.0f : 7.0f;
}

float status_tag_width(content_status status, int font_size) {
    if (status == content_status::modified) {
        return kModifiedStatusWidth;
    }
    return std::clamp(ui::measure_text_size(content_status_badge::label(status), static_cast<float>(font_size)).x + 18.0f,
                      48.0f, 92.0f);
}

void draw_status_tags(Rectangle anchor, content_status source_status, content_status status, unsigned char alpha, int font_size = 11) {
    const float status_width = status_tag_width(status, font_size);
    const Rectangle status_rect = {
        anchor.x + anchor.width - status_width,
        anchor.y,
        status_width,
        anchor.height,
    };
    if (content_status_badge::should_show_source(source_status, status)) {
        const float source_width = status_tag_width(source_status, font_size);
        draw_status_tag({status_rect.x - source_width - status_tag_gap(status), anchor.y, source_width, anchor.height},
                        source_status, alpha, font_size);
    }
    draw_status_tag(status_rect, status, alpha, font_size);
}

void draw_status_tags_fit(Rectangle bounds,
                          content_status source_status,
                          content_status status,
                          unsigned char alpha,
                          int font_size = 11) {
    const float status_width = status_tag_width(status, font_size);
    const Rectangle status_rect = {
        bounds.x + bounds.width - status_width,
        bounds.y,
        status_width,
        bounds.height,
    };
    if (content_status_badge::should_show_source(source_status, status)) {
        const float source_width = status_tag_width(source_status, font_size);
        draw_status_tag({status_rect.x - source_width - status_tag_gap(status), bounds.y, source_width, bounds.height},
                        source_status, alpha, font_size);
    }
    draw_status_tag(status_rect, status, alpha, font_size);
}

void draw_song_list_header(Rectangle column, int song_count, bool split_title, unsigned char alpha) {
    const auto& t = *g_theme;
    const song_list_header_layout layout = make_song_list_header_layout(column);
    if (!split_title) {
        ui::draw_text_in_rect(TextFormat("%d songs", song_count), 16,
                              layout.full,
                              with_alpha(t.text_muted, alpha), ui::text_align::left);
        return;
    }

    ui::draw_text_in_rect("ALL SONGS", 14,
                          layout.title,
                          with_alpha(t.text, alpha), ui::text_align::left);
    ui::draw_text_in_rect(TextFormat("%d songs", song_count), 12,
                          layout.count,
                          with_alpha(t.text_muted, alpha), ui::text_align::right);
}

void draw_compact_song_row(const song_select::song_entry& song,
                           Rectangle row,
                           bool selected,
                           const title_song_list_view::draw_config& config) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(row);
    const unsigned char row_alpha = selected ? config.selected_row_alpha
        : hovered ? config.hover_row_alpha
                  : config.normal_row_alpha;

    ui::surface(row,
                with_alpha(selected ? config.button_selected : config.button_base, row_alpha),
                with_alpha(t.border_light, static_cast<unsigned char>(130.0f * config.play_t)),
                kClassicRowBorderWidth);

    const std::string duration_label = format_duration_label(song.song.meta.duration_seconds);
    const float artist_reserved_width = duration_label.empty() ? 0.0f : kClassicArtistDurationReserveWidth;
    const compact_song_row_layout layout = make_compact_song_row_layout(row, artist_reserved_width);
    draw_marquee_text(song.song.meta.title.c_str(),
                      layout.title,
                      20, with_alpha(t.text, config.alpha), config.now);
    draw_marquee_text(song.song.meta.artist.c_str(),
                      layout.artist,
                      14, with_alpha(t.text_muted, config.alpha), config.now);
    if (!duration_label.empty()) {
        ui::draw_text_in_rect(duration_label.c_str(),
                              13,
                              layout.duration,
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
    }
    content_status_badge::draw_compound(layout.status_badge, song.source_status, song.status, config.alpha, 12);
}

void draw_compact_song_row(const song_row_view& row, const title_song_list_view::draw_config& config) {
    if (row.song == nullptr) {
        return;
    }
    draw_compact_song_row(*row.song, row.entry.rect, row.selected, config);
}

void draw_song_jacket(const song_select::state& state,
                      const song_select::song_entry& song,
                      Rectangle jacket_rect,
                      unsigned char row_alpha,
                      const title_song_list_view::draw_config& config) {
    const auto& t = *g_theme;
    if (state.jackets) {
        if (const Texture2D* jacket = state.jackets->get(song.song)) {
            ui::draw_texture(*jacket, jacket_rect, with_alpha(WHITE, config.alpha));
        } else {
            ui::placeholder(jacket_rect, "JACKET", {
                .font_size = 10,
                .draw_border = false,
                .fill = with_alpha(t.bg_alt, row_alpha),
                .text_color = with_alpha(t.text_muted, config.alpha),
                .custom_colors = true,
            });
        }
    } else {
        ui::placeholder(jacket_rect, "JACKET", {
            .font_size = 10,
            .draw_border = false,
            .fill = with_alpha(t.bg_alt, row_alpha),
            .text_color = with_alpha(t.text_muted, config.alpha),
            .custom_colors = true,
        });
    }
    ui::frame(jacket_rect, with_alpha(t.border_image, config.alpha), 1.0f);
}

void draw_expanded_song_row(const song_select::state& state,
                            const song_select::song_entry& song,
                            Rectangle row,
                            bool selected,
                            const title_song_list_view::draw_config& config) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(row);
    const unsigned char row_alpha = selected ? config.selected_row_alpha
        : hovered ? config.hover_row_alpha
                  : config.normal_row_alpha;

    ui::surface(row,
                with_alpha(selected ? config.button_selected : config.button_base, row_alpha),
                with_alpha(selected ? t.border_active : t.border_light,
                           static_cast<unsigned char>(selected ? config.alpha : 130.0f * config.play_t)),
                selected ? 1.8f : kRowBorderWidth);

    const expanded_song_row_layout layout = make_expanded_song_row_layout(row);
    draw_song_jacket(state, song, layout.jacket, row_alpha, config);
    draw_marquee_text(song.song.meta.title.c_str(),
                      layout.title,
                      selected ? 18 : 17, with_alpha(t.text, config.alpha), config.now);
    draw_marquee_text(song.song.meta.artist.c_str(),
                      layout.artist,
                      12, with_alpha(t.text_secondary, config.alpha), config.now);

    ui::wrapped_layout_cursor tags =
        ui::wrapped_cursor(layout.tag_lane, kExpandedTagGap, layout.tag_lane.height);
    int tags_drawn = 0;
    for (const std::string& label : genre_labels(song.song.meta)) {
        if (label.empty() || tags_drawn >= 2) {
            continue;
        }
        const float width = std::clamp(ui::measure_text_size(label.c_str(), kTagFontSize).x + 20.0f, 66.0f, 124.0f);
        if (width > layout.tag_lane.width) {
            break;
        }
        const std::optional<Rectangle> tag_rect = tags.next(width, layout.tag_lane.height);
        if (!tag_rect.has_value()) {
            break;
        }
        draw_tag(*tag_rect, label, config.alpha);
        ++tags_drawn;
    }

    ui::draw_text_in_rect(TextFormat("BPM %.0f", song.song.meta.base_bpm), 11,
                          layout.bpm,
                          with_alpha(t.text_secondary, config.alpha), ui::text_align::right);
    if (song.song.meta.has_play_count) {
        ui::draw_text_in_rect((std::string("plays ") + format_count_label(song.song.meta.play_count)).c_str(), 11,
                              layout.plays,
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
    }
    draw_status_tags(layout.status_tags,
                     song.source_status, song.status, config.alpha, 11);
}

void draw_expanded_song_row(const song_select::state& state,
                            const song_row_view& row,
                            const title_song_list_view::draw_config& config) {
    if (row.song == nullptr) {
        return;
    }
    draw_expanded_song_row(state, *row.song, row.entry.rect, row.selected, config);
}

void draw_chart_header(const chart_list_layout& layout, unsigned char alpha) {
    const auto& t = *g_theme;
    const chart_columns header_columns = make_chart_columns(layout.header_row);
    ui::draw_text_in_rect("KEYS", 10,
                          header_columns.key,
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    ui::draw_text_in_rect("LV", 10, header_columns.level,
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    ui::draw_text_in_rect("DIFF", 10, header_columns.difficulty,
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    ui::draw_text_in_rect("CREATOR", 10, header_columns.creator,
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    ui::draw_text_in_rect("NOTES", 10, header_columns.notes,
                          with_alpha(t.text_muted, alpha), ui::text_align::right);
    ui::draw_text_in_rect("SOURCE", 10, header_columns.status,
                          with_alpha(t.text_muted, alpha), ui::text_align::center);
    ui::draw_text_in_rect("RANK", 10, header_columns.rank,
                          with_alpha(t.text_muted, alpha), ui::text_align::right);
}

void draw_chart_row(const song_select::song_entry& song,
                    const song_select::chart_option& chart,
                    Rectangle row,
                    bool selected,
                    const title_song_list_view::draw_config& config) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(row);
    const bool locked = content_is_play_locked(song.song.meta, chart.meta);
    const unsigned char row_alpha = selected ? config.selected_row_alpha
        : hovered ? config.hover_row_alpha
                  : config.normal_row_alpha;
    const Color row_fill = locked
        ? lerp_color(config.button_base, t.bg, selected ? 0.48f : 0.36f)
        : (selected ? config.button_selected : config.button_base);
    ui::surface(row,
                with_alpha(row_fill, row_alpha),
                with_alpha(locked ? t.slow : selected ? t.border_active : t.border_light,
                           locked ? static_cast<unsigned char>((static_cast<int>(config.alpha) * 180) / 255)
                                  : config.alpha),
                1.0f);

    const chart_columns columns = make_chart_columns(row);
    const Color key_color = key_mode_color(chart.meta.key_count);
    ui::draw_text_in_rect(key_mode_label(chart.meta.key_count).c_str(), 13,
                          columns.key,
                          with_alpha(key_color, config.alpha), ui::text_align::left);
    draw_difficulty_level_badge(chart.meta.level, columns.level, 12, config.alpha);
    ui::draw_text_in_rect(chart.meta.difficulty.c_str(), 13,
                          columns.difficulty,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    ui::draw_text_in_rect(chart.meta.chart_author.empty() ? "-" : chart.meta.chart_author.c_str(), 12,
                          columns.creator,
                          with_alpha(t.text_secondary, config.alpha), ui::text_align::left);
    ui::draw_text_in_rect(chart.note_count > 0 ? TextFormat("%d", chart.note_count) : "-",
                          12,
                          columns.notes,
                          with_alpha(t.text_secondary, config.alpha), ui::text_align::right);
    draw_status_tags_fit(columns.status,
                         chart.source_status,
                         chart.status,
                         config.alpha,
                         9);
    if (chart.best_local_rank.has_value()) {
        ui::draw_text_in_rect(rank_label(*chart.best_local_rank), 13,
                              columns.rank,
                              with_alpha(rank_color(*chart.best_local_rank), config.alpha), ui::text_align::right);
    } else {
        ui::draw_text_in_rect("-", 12,
                              columns.rank,
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
    }
    if (locked) {
        draw_locked_row_overlay(row, config.alpha);
    }
}

void draw_embedded_chart_list(const song_select::state& state,
                              const song_select::song_entry& song,
                              Rectangle row,
                              const title_song_list_view::draw_config& config) {
    if (state.selected_song_expand_t <= 0.02f) {
        return;
    }

    const float chart_t = tween::ease_out_cubic(state.selected_song_expand_t);
    const chart_list_layout chart_layout = make_chart_list_layout(row);
    if (chart_layout.area.height < kChartMinDrawableHeight) {
        return;
    }
    const unsigned char chart_alpha =
        static_cast<unsigned char>(static_cast<float>(config.alpha) * chart_t);
    const auto& t = *g_theme;
    ui::surface(chart_layout.area,
                with_alpha(t.panel, static_cast<unsigned char>(52.0f * chart_t * (static_cast<float>(config.alpha) / 255.0f))),
                with_alpha(t.border_light, chart_alpha),
                1.0f);
    draw_chart_header(chart_layout, chart_alpha);

    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    ui::scoped_clip_rect chart_clip(chart_layout.viewport);
    title_song_list_view::draw_config chart_config = config;
    chart_config.alpha = chart_alpha;
    const ui::index_range visible_charts = ui::vertical_list_visible_range(
        filtered.size(), chart_layout.viewport, kChartRowHeight, kChartRowGap, config.embedded_chart_scroll_y);
    for (int chart_index = visible_charts.begin; chart_index < visible_charts.end; ++chart_index) {
        const Rectangle chart_row = chart_row_rect(chart_layout.area, chart_index, config.embedded_chart_scroll_y);
        draw_chart_row(song,
                       *filtered[static_cast<size_t>(chart_index)],
                       chart_row,
                       chart_index == state.difficulty_index,
                       chart_config);
    }
}

}  // namespace

namespace title_song_list_view {

float content_height(int count) {
    return ui::vertical_list_content_height(count, kSongRowHeight, kSongRowGap);
}

float content_height(const song_select::state& state) {
    float total = 0.0f;
    for_each_song_row(state, {}, 0.0f, [&](const song_row_entry& entry) {
        total = entry.rect.y + entry.rect.height;
        return true;
    });
    return total;
}

float max_scroll(Rectangle area, int count) {
    return ui::max_scroll_offset(content_height(count), area, kScrollPadding);
}

float max_scroll(Rectangle area, const song_select::state& state) {
    return ui::max_scroll_offset(title_song_list_view::content_height(state), area, kScrollPadding);
}

Rectangle row_rect(Rectangle area, int index, float scroll_y) {
    return fixed_song_row_entry_for(area, index, index, scroll_y).rect;
}

Rectangle row_rect(const song_select::state& state, Rectangle area, int index, float scroll_y) {
    Rectangle result{};
    for_each_song_row(state, area, scroll_y, [&](const song_row_entry& entry) {
        if (entry.song_index == index) {
            result = entry.rect;
            return false;
        }
        return true;
    });
    return result;
}

int hit_test(Rectangle area, float scroll_y, Vector2 point, int count) {
    if (!ui::contains_point(area, point)) {
        return -1;
    }
    const Rectangle rows_viewport = fixed_song_rows_viewport(area);
    const int index = ui::vertical_list_index_at_y(point.y, rows_viewport, kSongRowHeight, kSongRowGap, scroll_y);
    if (index >= 0 && index < count && ui::contains_point(row_rect(area, index, scroll_y), point)) {
        return index;
    }
    return -1;
}

int hit_test(const song_select::state& state, Rectangle area, float scroll_y, Vector2 point) {
    if (!ui::contains_point(area, point)) {
        return -1;
    }
    int result = -1;
    for_each_song_row(state, area, scroll_y, [&](const song_row_entry& entry) {
        if (ui::contains_point(entry.rect, point)) {
            result = entry.song_index;
            return false;
        }
        return true;
    });
    return result;
}

Rectangle selected_chart_list_rect(const song_select::state& state, Rectangle area, float scroll_y) {
    if (state.songs.empty()) {
        return {};
    }
    const int song_index = state.selected_song_index;
    if (song_index < 0 || song_index >= static_cast<int>(state.songs.size())) {
        return {};
    }
    if (!state.selected_song_expanded || state.selected_song_expand_t <= 0.02f) {
        return {};
    }
    const Rectangle charts = chart_area_rect(row_rect(state, area, song_index, scroll_y));
    if (charts.height < kChartMinDrawableHeight) {
        return {};
    }
    return chart_list_viewport_rect(charts);
}

float max_embedded_chart_scroll(const song_select::state& state, Rectangle area, float scroll_y) {
    if (state.songs.empty()) {
        return 0.0f;
    }
    const Rectangle viewport = selected_chart_list_rect(state, area, scroll_y);
    if (viewport.width <= 0.0f || viewport.height <= 0.0f) {
        return 0.0f;
    }
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    if (filtered.empty()) {
        return 0.0f;
    }
    const float content = ui::vertical_list_content_height(filtered.size(), kChartRowHeight, kChartRowGap);
    return ui::max_scroll_offset(content, viewport, kEmbeddedChartScrollPadding);
}

chart_hit hit_test_chart(const song_select::state& state,
                         Rectangle area,
                         float scroll_y,
                         float chart_scroll_y,
                         Vector2 point) {
    if (state.songs.empty() || !state.selected_song_expanded ||
        state.selected_song_expand_t <= 0.05f || !ui::contains_point(area, point)) {
        return {};
    }
    const int song_index = state.selected_song_index;
    if (song_index < 0 || song_index >= static_cast<int>(state.songs.size())) {
        return {};
    }
    const Rectangle selected_row = row_rect(state, area, song_index, scroll_y);
    if (!ui::contains_point(selected_row, point)) {
        return {};
    }
    const Rectangle charts = chart_area_rect(selected_row);
    if (charts.height < kChartMinDrawableHeight) {
        return {};
    }
    const Rectangle viewport = chart_list_viewport_rect(charts);
    if (!ui::contains_point(viewport, point)) {
        return {};
    }
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const int index = ui::vertical_list_index_at_y(point.y, viewport, kChartRowHeight, kChartRowGap, chart_scroll_y);
    if (index >= 0 && index < static_cast<int>(filtered.size())) {
        return {song_index, index};
    }
    return {};
}

void draw(const song_select::state& state, const draw_config& config) {
    const bool hide_unloaded_content =
        state.catalog_loading && !state.catalog_loaded_once && state.songs.empty();
    if (hide_unloaded_content) {
        return;
    }
    if (state.jackets) {
        state.jackets->poll();
    }

    if (!config.expanded_cards) {
        const std::vector<int> indices = song_select::filtered_song_indices(state);
        draw_song_list_header(config.column_rect, static_cast<int>(indices.size()), false, config.alpha);

        ui::scoped_clip_rect clip(config.column_rect);
        const ui::index_range visible_rows = ui::vertical_list_visible_range(
            indices.size(),
            fixed_song_rows_viewport(config.column_rect),
            config.column_rect,
            kSongRowHeight,
            kSongRowGap,
            state.scroll_y,
            kClipSlack);
        for (int visible = visible_rows.begin; visible < visible_rows.end; ++visible) {
            const int i = indices[static_cast<size_t>(visible)];
            const song_row_view row = song_row_view_for(
                state,
                fixed_song_row_entry_for(config.column_rect, visible, i, state.scroll_y));
            draw_compact_song_row(row, config);
        }
        return;
    }

    const std::vector<int> indices = song_select::filtered_song_indices(state);
    if (config.show_header) {
        draw_song_list_header(config.column_rect, static_cast<int>(indices.size()), true, config.alpha);
    }

    ui::scoped_clip_rect clip(config.column_rect);
    for_each_song_row(state, config.column_rect, state.scroll_y, [&](const song_row_entry& entry) {
        const song_row_view row = song_row_view_for(state, entry);
        if (row.song == nullptr || row.entry.rect.width <= 0.0f || row.entry.rect.height <= 0.0f) {
            return true;
        }
        if (!rect_visible_in(config.column_rect, row.entry.rect)) {
            return true;
        }

        draw_expanded_song_row(state, row, config);
        if (row.selected) {
            draw_embedded_chart_list(state, *row.song, row.entry.rect, config);
        }
        return true;
    });
}

}  // namespace title_song_list_view
