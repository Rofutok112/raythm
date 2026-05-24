#include "title/song_list_view.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "scene_common.h"
#include "shared/content_status_badge.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
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
constexpr float kJacketSize = 72.0f;
constexpr float kChartHeaderHeight = 24.0f;
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
    Rectangle row = {charts.x, charts.y - 3.0f, charts.width, kChartRowHeight};
    return make_chart_columns(row);
}

float row_height(const song_select::state& state, int index) {
    (void)state;
    (void)index;
    return kSongRowHeight;
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

Rectangle chart_list_viewport_rect(Rectangle chart_area) {
    return {
        chart_area.x,
        chart_area.y + kChartHeaderHeight + 5.0f,
        chart_area.width,
        chart_area.height - kChartHeaderHeight - 5.0f,
    };
}

Rectangle chart_row_rect(Rectangle chart_area, int index, float scroll_y) {
    const Rectangle viewport = chart_list_viewport_rect(chart_area);
    return {
        viewport.x,
        viewport.y + static_cast<float>(index) * (kChartRowHeight + kChartRowGap) - scroll_y,
        viewport.width,
        kChartRowHeight,
    };
}

void draw_tag(Rectangle rect, const std::string& label, unsigned char alpha) {
    const Color color = tag_color_for_label(label);
    ui::draw_rect_f(rect, with_alpha(g_theme->row_soft, static_cast<unsigned char>(70.0f * (static_cast<float>(alpha) / 255.0f))));
    ui::draw_rect_lines(rect, 1.0f, with_alpha(color, alpha));
    ui::draw_text_in_rect(label.c_str(), static_cast<int>(kTagFontSize), rect, with_alpha(color, alpha), ui::text_align::center);
}

void draw_status_tag(Rectangle rect, content_status status, unsigned char alpha, int font_size = 11) {
    const Color color = content_status_badge::color(status);
    if (status == content_status::modified) {
        const float icon_size = std::max(1.0f, std::min(rect.width, rect.height) - 1.0f);
        const Rectangle icon_rect = {
            rect.x + (rect.width - icon_size) * 0.5f,
            rect.y + (rect.height - icon_size) * 0.5f,
            icon_size,
            icon_size,
        };
        raythm_icons::draw_triangle_alert(icon_rect, with_alpha(color, alpha), 2.6f);
        ui::enqueue_hover_tooltip(icon_rect, "変更されています", alpha);
        return;
    }

    ui::draw_rect_f(rect, with_alpha(g_theme->row_soft, static_cast<unsigned char>(70.0f * (static_cast<float>(alpha) / 255.0f))));
    ui::draw_rect_lines(rect, 1.0f, with_alpha(color, alpha));
    ui::draw_text_in_rect(content_status_badge::label(status), font_size, rect,
                          with_alpha(color, alpha), ui::text_align::center);
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

void draw_status_tags_fit(Rectangle bounds, content_status source_status, content_status status, unsigned char alpha, int font_size = 11) {
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

void draw_chart_row(const song_select::chart_option& chart,
                    Rectangle row,
                    bool selected,
                    const title_song_list_view::draw_config& config) {
    const auto& t = *g_theme;
    const bool hovered = ui::is_hovered(row);
    const unsigned char row_alpha = selected ? config.selected_row_alpha
        : hovered ? config.hover_row_alpha
                  : config.normal_row_alpha;
    ui::draw_rect_f(row, with_alpha(selected ? config.button_selected : config.button_base, row_alpha));
    ui::draw_rect_lines(row, 1.0f, with_alpha(selected ? t.border_active : t.border_light, config.alpha));

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
    draw_status_tags_fit(columns.status, chart.source_status, chart.status, config.alpha, 9);
    if (chart.best_local_rank.has_value()) {
        ui::draw_text_in_rect(rank_label(*chart.best_local_rank), 13,
                              columns.rank,
                              with_alpha(rank_color(*chart.best_local_rank), config.alpha), ui::text_align::right);
    } else {
        ui::draw_text_in_rect("-", 12,
                              columns.rank,
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
    }
}

}  // namespace

namespace title_song_list_view {

float content_height(int count) {
    if (count <= 0) {
        return 0.0f;
    }
    return static_cast<float>(count) * (kSongRowHeight + kSongRowGap) - kSongRowGap;
}

float content_height(const song_select::state& state) {
    const std::vector<int> indices = song_select::filtered_song_indices(state);
    if (indices.empty()) {
        return 0.0f;
    }
    float total = kInitialRowOffsetY;
    for (int visible = 0; visible < static_cast<int>(indices.size()); ++visible) {
        total += row_height(state, indices[static_cast<size_t>(visible)]);
        if (visible + 1 < static_cast<int>(indices.size())) {
            total += kSongRowGap;
        }
    }
    return total;
}

float max_scroll(Rectangle area, int count) {
    return std::max(0.0f, content_height(count) - area.height + kScrollPadding);
}

float max_scroll(Rectangle area, const song_select::state& state) {
    return std::max(0.0f, title_song_list_view::content_height(state) - area.height + kScrollPadding);
}

Rectangle row_rect(Rectangle area, int index, float scroll_y) {
    return {
        area.x,
        area.y + kInitialRowOffsetY + static_cast<float>(index) * (kSongRowHeight + kSongRowGap) - scroll_y,
        area.width,
        kSongRowHeight
    };
}

Rectangle row_rect(const song_select::state& state, Rectangle area, int index, float scroll_y) {
    const std::vector<int> indices = song_select::filtered_song_indices(state);
    float y = area.y + kInitialRowOffsetY - scroll_y;
    for (int visible = 0; visible < static_cast<int>(indices.size()); ++visible) {
        const int song_index = indices[static_cast<size_t>(visible)];
        if (song_index == index) {
            return {area.x, y, area.width, row_height(state, song_index)};
        }
        y += row_height(state, song_index) + kSongRowGap;
    }
    return {};
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

int hit_test(const song_select::state& state, Rectangle area, float scroll_y, Vector2 point) {
    if (!CheckCollisionPointRec(point, area)) {
        return -1;
    }
    const std::vector<int> indices = song_select::filtered_song_indices(state);
    for (const int song_index : indices) {
        if (CheckCollisionPointRec(point, row_rect(state, area, song_index, scroll_y))) {
            return song_index;
        }
    }
    return -1;
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
    const float content =
        static_cast<float>(filtered.size()) * (kChartRowHeight + kChartRowGap) - kChartRowGap;
    return std::max(0.0f, content - viewport.height + kEmbeddedChartScrollPadding);
}

chart_hit hit_test_chart(const song_select::state& state,
                         Rectangle area,
                         float scroll_y,
                         float chart_scroll_y,
                         Vector2 point) {
    if (state.songs.empty() || !state.selected_song_expanded ||
        state.selected_song_expand_t <= 0.05f || !CheckCollisionPointRec(point, area)) {
        return {};
    }
    const int song_index = state.selected_song_index;
    if (song_index < 0 || song_index >= static_cast<int>(state.songs.size())) {
        return {};
    }
    const Rectangle selected_row = row_rect(state, area, song_index, scroll_y);
    if (!CheckCollisionPointRec(point, selected_row)) {
        return {};
    }
    const Rectangle charts = chart_area_rect(selected_row);
    if (charts.height < kChartMinDrawableHeight) {
        return {};
    }
    const Rectangle viewport = chart_list_viewport_rect(charts);
    if (!CheckCollisionPointRec(point, viewport)) {
        return {};
    }
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    for (int index = 0; index < static_cast<int>(filtered.size()); ++index) {
        if (CheckCollisionPointRec(point, chart_row_rect(charts, index, chart_scroll_y))) {
            return {song_index, index};
        }
    }
    return {};
}

void draw(const song_select::state& state, const draw_config& config) {
    const auto& t = *g_theme;
    const bool hide_unloaded_content =
        state.catalog_loading && !state.catalog_loaded_once && state.songs.empty();
    if (hide_unloaded_content) {
        return;
    }
    if (state.jackets) {
        state.jackets->poll();
    }

    if (!config.expanded_cards) {
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
                row, kClassicRowBorderWidth,
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
        return;
    }

    const std::vector<int> indices = song_select::filtered_song_indices(state);
    if (config.show_header) {
        ui::draw_text_in_rect("ALL SONGS", 14,
                              {config.column_rect.x, config.column_rect.y - kSongCountOffsetY,
                               config.column_rect.width * 0.5f, kSongCountHeight},
                              with_alpha(t.text, config.alpha), ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%d songs", static_cast<int>(indices.size())), 12,
                              {config.column_rect.x + config.column_rect.width * 0.5f,
                               config.column_rect.y - kSongCountOffsetY,
                               config.column_rect.width * 0.5f, kSongCountHeight},
                              with_alpha(t.text_muted, config.alpha), ui::text_align::right);
    }

    ui::scoped_clip_rect clip(config.column_rect);
    for (const int i : indices) {
        const song_select::song_entry& song = state.songs[static_cast<size_t>(i)];
        const Rectangle row = row_rect(state, config.column_rect, i, state.scroll_y);
        if (row.width <= 0.0f || row.height <= 0.0f) {
            continue;
        }
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
            row, selected ? 1.8f : kRowBorderWidth,
            with_alpha(selected ? t.border_active : t.border_light,
                       static_cast<unsigned char>(selected ? config.alpha : 130.0f * config.play_t)));

        const Rectangle jacket_rect = {row.x + 18.0f, row.y + 14.0f, kJacketSize, kJacketSize};
        if (state.jackets) {
            if (const Texture2D* jacket = state.jackets->get(song.song)) {
                DrawTexturePro(*jacket,
                               {0.0f, 0.0f, static_cast<float>(jacket->width), static_cast<float>(jacket->height)},
                               jacket_rect, {0.0f, 0.0f}, 0.0f, with_alpha(WHITE, config.alpha));
            } else {
                ui::draw_rect_f(jacket_rect, with_alpha(t.bg_alt, row_alpha));
                ui::draw_text_in_rect("JACKET", 10, jacket_rect, with_alpha(t.text_muted, config.alpha));
            }
        } else {
            ui::draw_rect_f(jacket_rect, with_alpha(t.bg_alt, row_alpha));
            ui::draw_text_in_rect("JACKET", 10, jacket_rect, with_alpha(t.text_muted, config.alpha));
        }
        ui::draw_rect_lines(jacket_rect, 1.0f, with_alpha(t.border_image, config.alpha));

        const float text_x = jacket_rect.x + jacket_rect.width + 20.0f;
        const float right_x = row.x + row.width - 130.0f;
        const float title_y = row.y + 15.0f;
        const float artist_y = row.y + 41.0f;
        const float tag_y = row.y + 62.0f;
        draw_marquee_text(song.song.meta.title.c_str(),
                          {text_x, title_y, std::max(120.0f, right_x - text_x - 20.0f), 24.0f},
                          selected ? 18 : 17, with_alpha(t.text, config.alpha), config.now);
        draw_marquee_text(song.song.meta.artist.c_str(),
                          {text_x, artist_y, std::max(120.0f, right_x - text_x - 20.0f), 18.0f},
                          12, with_alpha(t.text_secondary, config.alpha), config.now);

        float tag_x = text_x;
        int tags_drawn = 0;
        for (const std::string& label : genre_labels(song.song.meta)) {
            if (label.empty() || tags_drawn >= 2) {
                continue;
            }
            const float width = std::clamp(ui::measure_text_size(label.c_str(), kTagFontSize).x + 20.0f, 66.0f, 124.0f);
            if (tag_x + width > right_x - 16.0f) {
                break;
            }
            draw_tag({tag_x, tag_y, width, kTagHeight}, label, config.alpha);
            tag_x += width + 8.0f;
            ++tags_drawn;
        }

        ui::draw_text_in_rect(TextFormat("BPM %.0f", song.song.meta.base_bpm), 11,
                              {right_x, title_y, 104.0f, 18.0f},
                              with_alpha(t.text_secondary, config.alpha), ui::text_align::right);
        if (song.song.meta.has_play_count) {
            ui::draw_text_in_rect((std::string("plays ") + format_count_label(song.song.meta.play_count)).c_str(), 11,
                                  {right_x, artist_y, 104.0f, 18.0f},
                                  with_alpha(t.text_muted, config.alpha), ui::text_align::right);
        }
        draw_status_tags({row.x + row.width - 124.0f, tag_y, 88.0f, kTagHeight},
                         song.source_status, song.status, config.alpha, 11);

        if (!selected || state.selected_song_expand_t <= 0.02f) {
            continue;
        }

        const float chart_t = tween::ease_out_cubic(state.selected_song_expand_t);
        const Rectangle charts = chart_area_rect(row);
        if (charts.height < kChartMinDrawableHeight) {
            continue;
        }
        const unsigned char chart_alpha =
            static_cast<unsigned char>(static_cast<float>(config.alpha) * chart_t);
        ui::draw_rect_f(charts, with_alpha(t.panel, static_cast<unsigned char>(52.0f * chart_t * (static_cast<float>(config.alpha) / 255.0f))));
        ui::draw_rect_lines(charts, 1.0f, with_alpha(t.border_light, chart_alpha));
        const chart_columns header_columns = make_chart_header_columns(charts);
        ui::draw_text_in_rect("KEYS", 10,
                              header_columns.key,
                              with_alpha(t.text_muted, chart_alpha), ui::text_align::left);
        ui::draw_text_in_rect("LV", 10, header_columns.level,
                              with_alpha(t.text_muted, chart_alpha), ui::text_align::left);
        ui::draw_text_in_rect("DIFF", 10, header_columns.difficulty,
                              with_alpha(t.text_muted, chart_alpha), ui::text_align::left);
        ui::draw_text_in_rect("CREATOR", 10, header_columns.creator,
                              with_alpha(t.text_muted, chart_alpha), ui::text_align::left);
        ui::draw_text_in_rect("NOTES", 10, header_columns.notes,
                              with_alpha(t.text_muted, chart_alpha), ui::text_align::right);
        ui::draw_text_in_rect("SOURCE", 10, header_columns.status,
                              with_alpha(t.text_muted, chart_alpha), ui::text_align::center);
        ui::draw_text_in_rect("RANK", 10, header_columns.rank,
                              with_alpha(t.text_muted, chart_alpha), ui::text_align::right);

        const auto filtered = song_select::filtered_charts_for_selected_song(state);
        const Rectangle viewport = chart_list_viewport_rect(charts);
        ui::scoped_clip_rect chart_clip(viewport);
        title_song_list_view::draw_config chart_config = config;
        chart_config.alpha = chart_alpha;
        for (int chart_index = 0; chart_index < static_cast<int>(filtered.size()); ++chart_index) {
            const Rectangle chart_row = chart_row_rect(charts, chart_index, config.embedded_chart_scroll_y);
            if (chart_row.y + chart_row.height < viewport.y - kClipSlack ||
                chart_row.y > viewport.y + viewport.height + kClipSlack) {
                continue;
            }
            draw_chart_row(*filtered[static_cast<size_t>(chart_index)],
                           chart_row,
                           chart_index == state.difficulty_index,
                           chart_config);
        }
    }
}

}  // namespace title_song_list_view
