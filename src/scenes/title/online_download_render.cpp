#include "title/online_download_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "content_lifecycle.h"
#include "localization/localization.h"
#include "scene_common.h"
#include "services/content_sync_service.h"
#include "song_select/song_select_filter_widget.h"
#include "song_select/song_select_level_filter_widget.h"
#include "tween.h"
#include "title/preview_transport_widget.h"
#include "title/title_layout.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_text_input.h"
#include "ui/icons/raythm_icons.h"
#include "virtual_screen.h"

namespace title_online_view {
namespace {

constexpr float kChartFilterMinLevel = song_select::level_filter::kMinLevel;
constexpr float kChartFilterMaxLevel = song_select::level_filter::kMaxLevel;
constexpr float kRankingPanelInsetX = 28.0f;
constexpr float kRankingHeaderY = 452.0f;
constexpr float kRankingTableY = 486.0f;
constexpr float kRankingTableHeight = 244.0f;
constexpr float kRankingRowTop = 34.0f;
constexpr float kRankingRowHeight = 27.0f;
constexpr float kRankingRowStep = 30.0f;
constexpr float kDetailTitleGap = 24.0f;
constexpr float kDetailTitleRightInset = 30.0f;
constexpr float kDetailTitleTopOffset = 4.0f;
constexpr float kDetailTitleHeight = 38.0f;
constexpr float kDetailArtistTopOffset = 40.0f;
constexpr float kDetailArtistHeight = 24.0f;
constexpr float kDetailChartTopOffset = 82.0f;
constexpr float kDetailGenreTopOffset = 118.0f;
constexpr float kDetailKeywordTopOffset = 170.0f;
constexpr float kDetailTagHeadingHeight = 16.0f;
constexpr float kDetailTagTopGap = 20.0f;
constexpr float kDetailTagHeight = 26.0f;
constexpr float kDetailProgressTimeTopGap = 14.0f;
constexpr float kDetailDownloadProgressTopOffset = 18.0f;
constexpr float kDetailDownloadProgressHeight = 8.0f;
constexpr float kDetailLockReasonTopOffset = 24.0f;
constexpr float kDetailLockReasonHeight = 18.0f;

struct compact_tag_row_layout {
    Rectangle heading = {};
    Rectangle tag_area = {};
};

struct detail_preview_layout {
    Rectangle title = {};
    Rectangle artist = {};
    Rectangle chart_summary = {};
    Rectangle chart_level_badge = {};
    Rectangle chart_author = {};
    compact_tag_row_layout genres = {};
    compact_tag_row_layout keywords = {};
    Rectangle preview_time = {};
    Rectangle download_progress = {};
    Rectangle lock_reason = {};
};

struct preview_ranking_layout {
    Rectangle header = {};
    Rectangle table = {};
    Rectangle placement_header = {};
    Rectangle player_header = {};
    Rectangle score_header = {};
    Rectangle accuracy_header = {};
    Rectangle clear_header = {};
    Rectangle empty_message = {};
};

struct preview_ranking_row_layout {
    Rectangle row = {};
    Rectangle placement = {};
    Rectangle player = {};
    Rectangle score = {};
    Rectangle accuracy = {};
    Rectangle clear = {};
};

compact_tag_row_layout compact_tag_row_layout_for(Rectangle title_rect, float y) {
    return {
        .heading = {title_rect.x, y, title_rect.width, kDetailTagHeadingHeight},
        .tag_area = {title_rect.x, y + kDetailTagTopGap, title_rect.width, kDetailTagHeight},
    };
}

detail_preview_layout detail_preview_layout_for(Rectangle hero_jacket_rect,
                                                Rectangle preview_panel,
                                                Rectangle preview_bar,
                                                Rectangle primary_action) {
    const Rectangle title = {
        hero_jacket_rect.x + hero_jacket_rect.width + kDetailTitleGap,
        hero_jacket_rect.y + kDetailTitleTopOffset,
        preview_panel.x + preview_panel.width - hero_jacket_rect.x - hero_jacket_rect.width -
            kDetailTitleGap - kDetailTitleRightInset,
        kDetailTitleHeight,
    };
    return {
        .title = title,
        .artist = {title.x, title.y + kDetailArtistTopOffset, title.width, kDetailArtistHeight},
        .chart_summary = {title.x, title.y + kDetailChartTopOffset, title.width * 0.42f, 24.0f},
        .chart_level_badge = {title.x + title.width * 0.43f, title.y + kDetailChartTopOffset, 70.0f, 21.0f},
        .chart_author = {title.x + title.width * 0.58f, title.y + kDetailChartTopOffset,
                         title.width * 0.42f, 24.0f},
        .genres = compact_tag_row_layout_for(title, title.y + kDetailGenreTopOffset),
        .keywords = compact_tag_row_layout_for(title, title.y + kDetailKeywordTopOffset),
        .preview_time = {preview_bar.x, preview_bar.y + kDetailProgressTimeTopGap, preview_bar.width, 16.0f},
        .download_progress = {primary_action.x,
                              primary_action.y - kDetailDownloadProgressTopOffset,
                              primary_action.width,
                              kDetailDownloadProgressHeight},
        .lock_reason = {primary_action.x,
                        primary_action.y - kDetailLockReasonTopOffset,
                        primary_action.width,
                        kDetailLockReasonHeight},
    };
}

preview_ranking_layout preview_ranking_layout_for(Rectangle preview_panel) {
    const Rectangle table = {
        preview_panel.x + kRankingPanelInsetX,
        preview_panel.y + kRankingTableY,
        preview_panel.width - kRankingPanelInsetX * 2.0f,
        kRankingTableHeight,
    };
    return {
        .header = {
            preview_panel.x + kRankingPanelInsetX,
            preview_panel.y + kRankingHeaderY,
            preview_panel.width - kRankingPanelInsetX * 2.0f,
            26.0f,
        },
        .table = table,
        .placement_header = {table.x + 12.0f, table.y + 10.0f, 32.0f, 16.0f},
        .player_header = {table.x + 58.0f, table.y + 10.0f, 180.0f, 16.0f},
        .score_header = {table.x + 300.0f, table.y + 10.0f, 116.0f, 16.0f},
        .accuracy_header = {table.x + 444.0f, table.y + 10.0f, 80.0f, 16.0f},
        .clear_header = {table.x + table.width - 76.0f, table.y + 10.0f, 60.0f, 16.0f},
        .empty_message = {table.x + 20.0f, table.y + 70.0f, table.width - 40.0f, 28.0f},
    };
}

preview_ranking_row_layout preview_ranking_row_layout_for(Rectangle table, int index) {
    const Rectangle row = {
        table.x + 10.0f,
        table.y + kRankingRowTop + static_cast<float>(index) * kRankingRowStep,
        table.width - 20.0f,
        kRankingRowHeight,
    };
    return {
        .row = row,
        .placement = {row.x + 4.0f, row.y + 5.0f, 34.0f, 16.0f},
        .player = {row.x + 48.0f, row.y + 5.0f, 190.0f, 16.0f},
        .score = {row.x + 270.0f, row.y + 5.0f, 128.0f, 16.0f},
        .accuracy = {row.x + 424.0f, row.y + 5.0f, 78.0f, 16.0f},
        .clear = {row.x + row.width - 54.0f, row.y + 4.0f, 44.0f, 18.0f},
    };
}

Rectangle centered_icon_rect(Rectangle rect, float inset) {
    const float size = std::max(1.0f, std::min(rect.width, rect.height) - inset * 2.0f);
    return {
        rect.x + (rect.width - size) * 0.5f,
        rect.y + (rect.height - size) * 0.5f,
        size,
        size
    };
}

Rectangle song_card_jacket_rect(Rectangle card) {
    const float jacket_size = std::min(card.height - 42.0f, 96.0f);
    return {
        card.x + 16.0f,
        card.y + 16.0f,
        jacket_size,
        jacket_size,
    };
}

int selected_song_display_index(const state& state) {
    const auto indices = detail::filtered_indices(state);
    const int selected_index = detail::selected_song_index_ref(state);
    const auto it = std::find(indices.begin(), indices.end(), selected_index);
    if (it == indices.end()) {
        return -1;
    }
    return static_cast<int>(it - indices.begin());
}

std::string join_labels(const std::vector<std::string>& values, const char* separator) {
    std::string result;
    for (const std::string& value : values) {
        if (value.empty()) {
            continue;
        }
        if (!result.empty()) {
            result += separator;
        }
        result += value;
    }
    return result;
}

std::string genre_summary(const song_meta& meta) {
    const std::string joined = join_labels(meta.genres, " / ");
    if (!joined.empty()) {
        return joined;
    }
    return meta.genre;
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

std::string primary_genre_label(const song_meta& meta) {
    if (!meta.genres.empty()) {
        return meta.genres.front();
    }
    return meta.genre;
}

std::string shelf_display_title(const std::string& key) {
    if (key == "rising") {
        return "RISING THIS WEEK";
    }
    if (key == "hidden_gems") {
        return "HIDDEN GEMS";
    }
    if (key == "fresh_charts") {
        return "FRESH CHARTS";
    }
    if (key == "new") {
        return "NEW ARRIVALS";
    }
    if (key == "needs_charts") {
        return "NEEDS CHARTS";
    }
    if (key == "recommended") {
        return "RECOMMENDED";
    }
    return "DISCOVERY";
}

Color genre_color_for_label(const std::string& label) {
    static constexpr Color kPalette[] = {
        {147, 94, 226, 255},   // violet
        {38, 167, 216, 255},   // cyan
        {214, 143, 43, 255},   // amber
        {132, 204, 45, 255},   // lime
        {216, 78, 133, 255},   // rose
        {62, 126, 220, 255},   // blue
        {39, 181, 154, 255},   // mint
        {218, 91, 61, 255},    // coral
        {190, 181, 48, 255},   // yellow
        {162, 103, 231, 255},  // purple
        {65, 190, 96, 255},    // green
        {212, 94, 172, 255},   // pink
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

Color keyword_color_for_label(const std::string& label) {
    return genre_color_for_label("keyword:" + label);
}

std::string song_subtitle(const song_meta& meta) {
    return meta.artist;
}

const char* format_count_label(const char* label, int count) {
    if (count >= 1000000) {
        return TextFormat("%s %.1fm", label, static_cast<float>(count) / 1000000.0f);
    }
    if (count >= 1000) {
        return TextFormat("%s %.1fk", label, static_cast<float>(count) / 1000.0f);
    }
    return TextFormat("%s %d", label, count);
}

std::string format_score(int value) {
    std::string digits = std::to_string(std::max(0, value));
    for (int insert_at = static_cast<int>(digits.size()) - 3; insert_at > 0; insert_at -= 3) {
        digits.insert(static_cast<size_t>(insert_at), ",");
    }
    return digits;
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

bool view_matches_shelf(discovery_view view, const std::string& key) {
    switch (view) {
    case discovery_view::overview:
        return false;
    case discovery_view::new_arrivals:
        return key == "new";
    case discovery_view::rising:
        return key == "rising";
    case discovery_view::hidden_gems:
        return key == "hidden_gems";
    case discovery_view::recommended:
        return key == "recommended";
    case discovery_view::needs_charts:
        return key == "needs_charts";
    }
    return false;
}

const char* view_label(discovery_view view) {
    switch (view) {
    case discovery_view::overview:
        return "Overview";
    case discovery_view::new_arrivals:
        return "New";
    case discovery_view::rising:
        return "Rising";
    case discovery_view::hidden_gems:
        return "Hidden gems";
    case discovery_view::recommended:
        return "Recommended";
    case discovery_view::needs_charts:
        return "Needs charts";
    }
    return "";
}

const char* source_label(source_filter source) {
    switch (source) {
    case source_filter::all:
        return "All";
    case source_filter::official:
        return "Official";
    case source_filter::community:
        return "Community";
    }
    return "";
}

float chart_level_value(const std::string& value, float fallback);

bool chart_filters_active(const state& state) {
    const float min_level = chart_level_value(state.min_level_input.value, kChartFilterMinLevel);
    const float max_level = chart_level_value(state.max_level_input.value, kChartFilterMaxLevel);
    return !state.chart_search_input.value.empty() ||
        std::fabs(min_level - kChartFilterMinLevel) > 0.001f ||
        std::fabs(max_level - kChartFilterMaxLevel) > 0.001f ||
        state.chart_source != chart_source_filter::all ||
        state.chart_key_filter != 0 ||
        state.chart_download_filter != 0;
}

const char* chart_source_label(content_status status) {
    switch (status) {
    case content_status::official:
        return "Official";
    case content_status::community:
        return "Community";
    default:
        return "Local";
    }
}

Color chart_source_status_color(const chart_entry_state& chart) {
    if (content_sync_service::is_modified(chart.chart.sync_state)) {
        return g_theme->slow;
    }
    if (chart.installed || chart.update_available) {
        return g_theme->success;
    }
    return chart.chart.source_status == content_status::official ? g_theme->success : g_theme->slow;
}

float chart_level_value(const std::string& value, float fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        size_t parsed = 0;
        const float result = std::stof(value, &parsed);
        if (parsed == value.size()) {
            return std::clamp(result, 0.0f, 99.0f);
        }
    } catch (...) {
    }
    return fallback;
}

void draw_level_range_slider(Rectangle chart_list, const state& state, unsigned char alpha) {
    float min_level = chart_level_value(state.min_level_input.value, kChartFilterMinLevel);
    float max_level = chart_level_value(state.max_level_input.value, kChartFilterMaxLevel);
    const Rectangle range = detail::chart_level_slider_rect(chart_list);
    song_select::level_filter::draw_range_slider(range, min_level, max_level, alpha, {
        .body_text = true,
    });
}

void draw_browse_body_text_in_rect(const char* text,
                            int font_size,
                            Rectangle rect,
                            Color color,
                            ui::text_align align = ui::text_align::center) {
    ui::draw_body_text_in_rect(text, font_size, rect, color, align);
}

void draw_browse_body_text_f(const char* text, float x, float y, int font_size, Color color) {
    ui::draw_text_body(text, {x, y}, static_cast<float>(font_size), 0.0f, color);
}

void draw_body_marquee_text(const char* text,
                            Rectangle clip_rect,
                            int font_size,
                            Color color,
                            double time,
                            ui::text_align align = ui::text_align::left) {
    if (text == nullptr || *text == '\0' || clip_rect.width <= 0.0f || clip_rect.height <= 0.0f) {
        return;
    }

    const float text_width = ui::measure_body_text_size(text, static_cast<float>(font_size), 0.0f).x;
    const float draw_y = clip_rect.y + (clip_rect.height - ui::text_layout_font_size(static_cast<float>(font_size))) * 0.5f;
    if (text_width <= clip_rect.width) {
        float draw_x = clip_rect.x;
        if (align == ui::text_align::center) {
            draw_x = clip_rect.x + (clip_rect.width - text_width) * 0.5f;
        } else if (align == ui::text_align::right) {
            draw_x = clip_rect.x + clip_rect.width - text_width;
        }
        ui::scoped_clip_rect clip_scope(clip_rect);
        draw_browse_body_text_f(text, draw_x, draw_y, font_size, color);
        return;
    }

    constexpr float kScrollSpeed = 42.0f;
    constexpr float kPauseSeconds = 1.0f;
    const float overflow = text_width - clip_rect.width;
    const float travel_time = overflow / kScrollSpeed;
    const float cycle = travel_time + kPauseSeconds * 2.0f;
    const float cycle_t = static_cast<float>(std::fmod(time, static_cast<double>(std::max(cycle, 0.001f))));
    float offset = 0.0f;
    if (cycle_t > kPauseSeconds && cycle_t < kPauseSeconds + travel_time) {
        offset = (cycle_t - kPauseSeconds) * kScrollSpeed;
    } else if (cycle_t >= kPauseSeconds + travel_time) {
        offset = overflow;
    }
    ui::scoped_clip_rect clip_scope(clip_rect);
    draw_browse_body_text_f(text, clip_rect.x - offset, draw_y, font_size, color);
}

void draw_body_marquee_text(const char* text,
                            float x,
                            float y,
                            int font_size,
                            Color color,
                            float max_width,
                            double time) {
    draw_body_marquee_text(text,
                           {x, y, max_width, ui::text_layout_font_size(static_cast<float>(font_size))},
                           font_size,
                           color,
                           time,
                           ui::text_align::left);
}

ui::text_input_result draw_song_search_input(Rectangle rect, ui::text_input_state& state,
                                             const char* label, const char* placeholder,
                                             int font_size, size_t max_length,
                                             Color button_base, Color button_hover, Color button_selected,
                                             unsigned char normal_row_alpha,
                                             unsigned char hover_row_alpha,
                                             unsigned char selected_row_alpha,
                                             unsigned char alpha) {
    (void)button_hover;
    return ui::search_text_input(rect, state, label, placeholder, {
        .font_size = font_size,
        .max_length = max_length,
        .content_padding_x = 14.0f,
        .button_base = button_base,
        .button_selected = button_selected,
        .normal_row_alpha = normal_row_alpha,
        .hover_row_alpha = hover_row_alpha,
        .selected_row_alpha = selected_row_alpha,
        .alpha = alpha,
    });
}

void draw_download_icon_button(Rectangle rect, bool update, unsigned char alpha) {
    const auto& t = *g_theme;
    const Color tone = update ? t.accent : t.success;
    const Color stroke = with_alpha(tone, alpha);
    ui::icon_button(rect, raythm_icons::draw_download, {
        .border_width = 1.2f,
        .bg = with_alpha(lerp_color(t.section, tone, 0.10f), static_cast<unsigned char>(alpha * 0.82f)),
        .bg_hover = with_alpha(lerp_color(t.section, tone, 0.18f), alpha),
        .icon_color = stroke,
        .icon_hover_color = stroke,
        .border_color = stroke,
        .icon_inset = 7.0f,
        .icon_stroke_width = 2.8f,
        .pressed_inset = 1.2f,
        .border_alpha_tracks_fill = false,
    });
}

Color action_tone_for_state(bool update_available, bool installed, bool downloading) {
    const auto& t = *g_theme;
    if (downloading) {
        return t.text_muted;
    }
    if (update_available) {
        return t.accent;
    }
    if (!installed) {
        return t.success;
    }
    return t.fast;
}

bool lifecycle_blocks_song_download(const song_entry_state& song) {
    return song.song.online_identity.has_value() &&
           !content_lifecycle::lifecycle_is_active(song.song.online_identity->lifecycle_status);
}

void draw_toned_button(Rectangle rect,
                       const char* label,
                       int font_size,
                       Color tone,
                       unsigned char alpha,
                       unsigned char base_alpha,
                       unsigned char hover_alpha) {
    const auto& t = *g_theme;
    const Color base = with_alpha(lerp_color(t.section, tone, 0.14f), base_alpha);
    const Color hover = with_alpha(lerp_color(t.section, tone, 0.28f), hover_alpha);
    ui::button(rect, label, {
        .font_size = font_size,
        .border_width = 1.4f,
        .bg = base,
        .bg_hover = hover,
        .text_color = with_alpha(t.text, alpha),
        .custom_colors = true,
    });
}

}  // namespace

void draw(state& state, const title_audio_controller& audio_controller, float anim_t, Rectangle origin_rect) {
    const auto& t = *g_theme;
    const float play_t = std::clamp(anim_t, 0.0f, 1.0f);
    if (play_t <= 0.01f) {
        return;
    }

    const layout current = make_layout(anim_t, origin_rect);
    const float content_fade_t = std::clamp((play_t - 0.16f) / 0.66f, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(255.0f * content_fade_t);
    const double now = GetTime();
    const Color button_base = t.row_soft;
    const Color button_hover = t.row_soft_hover;
    const Color button_selected = t.row_soft_selected;
    const unsigned char normal_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_alpha) / 255);
    const unsigned char hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_hover_alpha) / 255);
    const unsigned char selected_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_alpha) / 255);

    const auto indices = detail::filtered_indices(state);
    const auto& songs = detail::active_songs(state);
    const bool loading = state.catalog_loading;
    const char* caption = detail::catalog_caption(state, songs);
    const float detail_t = std::clamp(state.detail_transition, 0.0f, 1.0f);
    const float jacket_t = state.detail_open
        ? tween::ease_out_cubic(std::pow(detail_t, 1.35f))
        : tween::ease_out_cubic(detail_t);
    const float detail_content_t = std::clamp((detail_t - 0.16f) / 0.84f, 0.0f, 1.0f);
    const float grid_fade_t = detail_t >= 0.96f ? 0.0f : std::clamp(1.0f - detail_t / 0.96f, 0.0f, 1.0f);
    const unsigned char grid_alpha =
        static_cast<unsigned char>(static_cast<float>(alpha) * std::clamp(grid_fade_t, 0.0f, 1.0f));
    const unsigned char detail_alpha =
        static_cast<unsigned char>(static_cast<float>(alpha) * detail_content_t);

    ui::button(current.back_rect, "戻る", {
        .font_size = 16,
        .border_width = 1.5f,
        .bg = with_alpha(button_base, normal_row_alpha),
        .bg_hover = with_alpha(button_hover, hover_row_alpha),
        .text_color = with_alpha(t.text, alpha),
        .custom_colors = true,
    });

    ui::surface(current.sidebar_rect,
                with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * grid_fade_t / 2.0f)),
                with_alpha(t.border_light, grid_alpha),
                1.2f);
    const unsigned char faded_normal_row_alpha = static_cast<unsigned char>(normal_row_alpha * grid_fade_t);
    const unsigned char faded_hover_row_alpha = static_cast<unsigned char>(hover_row_alpha * grid_fade_t);
    const unsigned char faded_selected_row_alpha = static_cast<unsigned char>(selected_row_alpha * grid_fade_t);
    draw_song_search_input(current.search_rect, state.search_input, "", localization::tr_literal("Search"),
                           14, 64,
                           button_base, button_hover, button_selected,
                           faded_normal_row_alpha, faded_hover_row_alpha, faded_selected_row_alpha,
                           grid_alpha);
    constexpr float kSidebarTitleY = 106.0f;
    constexpr float kSourceTitleY = 594.0f;
    constexpr float kSidebarDividerY = 562.0f;
    draw_browse_body_text_in_rect("DISCOVERY", 15,
                          {current.sidebar_rect.x + 24.0f, current.sidebar_rect.y + kSidebarTitleY,
                           current.sidebar_rect.width - 48.0f, 20.0f},
                          with_alpha(t.accent, grid_alpha), ui::text_align::left);
    for (int index = 0; index < static_cast<int>(detail::kDiscoveryViews.size()); ++index) {
        const discovery_view view = detail::kDiscoveryViews[static_cast<size_t>(index)];
        const bool active = state.view == view;
        ui::button(detail::sidebar_button_rect(current.sidebar_rect, index), view_label(view), {
            .font_size = 14,
            .border_width = 1.2f,
            .bg = with_alpha(active ? button_selected : button_base,
                             static_cast<unsigned char>((active ? selected_row_alpha : normal_row_alpha) * grid_fade_t)),
            .bg_hover = with_alpha(active ? button_selected : button_hover,
                                   static_cast<unsigned char>((active ? selected_row_alpha : hover_row_alpha) * grid_fade_t)),
            .text_color = with_alpha(active ? t.text : t.text_secondary, grid_alpha),
            .custom_colors = true,
        });
    }
    ui::draw_line_ex({current.sidebar_rect.x + 24.0f, current.sidebar_rect.y + kSidebarDividerY},
                     {current.sidebar_rect.x + current.sidebar_rect.width - 24.0f, current.sidebar_rect.y + kSidebarDividerY},
                     1.2f, with_alpha(t.border_active, grid_alpha));
    draw_browse_body_text_in_rect("SOURCE", 15,
                          {current.sidebar_rect.x + 24.0f, current.sidebar_rect.y + kSourceTitleY,
                           current.sidebar_rect.width - 48.0f, 20.0f},
                          with_alpha(t.accent, grid_alpha), ui::text_align::left);
    for (int index = 0; index < static_cast<int>(detail::kSourceFilters.size()); ++index) {
        const source_filter source = detail::kSourceFilters[static_cast<size_t>(index)];
        const bool active = state.source == source;
        ui::button(detail::source_button_rect(current.sidebar_rect, index), source_label(source), {
            .font_size = 14,
            .border_width = 1.2f,
            .bg = with_alpha(active ? button_selected : button_base,
                             static_cast<unsigned char>((active ? selected_row_alpha : normal_row_alpha) * grid_fade_t)),
            .bg_hover = with_alpha(active ? button_selected : button_hover,
                                   static_cast<unsigned char>((active ? selected_row_alpha : hover_row_alpha) * grid_fade_t)),
            .text_color = with_alpha(active ? t.text : t.text_secondary, grid_alpha),
            .custom_colors = true,
        });
    }

    ui::surface(current.content_rect,
                with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * grid_fade_t / 2.0f)),
                with_alpha(t.border_light, grid_alpha),
                1.2f);
    const std::string content_caption = state.view == discovery_view::overview
        ? "Overview"
        : view_label(state.view);
    draw_browse_body_text_in_rect(content_caption.c_str(), 17,
                          {current.content_rect.x + 12.0f, current.content_rect.y + 6.0f,
                           current.content_rect.width * 0.52f, 20.0f},
                          with_alpha(loading ? t.text : t.text_secondary, grid_alpha), ui::text_align::left);
    draw_browse_body_text_in_rect(TextFormat("%d songs", static_cast<int>(indices.size())),
                          14,
                          {current.content_rect.x + current.content_rect.width * 0.46f, current.content_rect.y + 8.0f,
                           current.content_rect.width * 0.5f - 12.0f, 16.0f},
                          with_alpha(t.text_muted, grid_alpha), ui::text_align::right);
    draw_browse_body_text_in_rect(localization::tr_literal("Press Esc to return to the grid"),
                          14,
                          {current.content_rect.x + current.content_rect.width * 0.46f, current.content_rect.y + 8.0f,
                           current.content_rect.width * 0.5f - 12.0f, 16.0f},
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::right);

    Rectangle source_jacket_rect = current.hero_jacket_rect;
    bool selected_card_drawn = false;
    {
        ui::scoped_clip_rect song_clip(current.song_grid_rect);
        if (indices.empty() && grid_alpha > 0) {
            const Rectangle placeholder = {
                current.song_grid_rect.x + 96.0f,
                current.song_grid_rect.y + current.song_grid_rect.height * 0.5f - 42.0f,
                current.song_grid_rect.width - 192.0f,
                84.0f,
            };
            ui::surface(placeholder,
                        with_alpha(button_base, static_cast<unsigned char>(selected_row_alpha * grid_fade_t)),
                        with_alpha(t.border_light, grid_alpha),
                        1.5f);
            const char* empty_title = loading
                ? localization::tr_literal("Loading...")
                : (state.mode == catalog_mode::owned && state.owned_loading)
                    ? localization::tr_literal("Syncing owned songs...")
                : state.catalog_maintenance
                    ? localization::tr_literal("Server maintenance")
                : (state.catalog_request_failed
                    ? localization::tr_literal("Could not reach raythm-Server.")
                    : localization::tr_literal("No songs found."));
            draw_browse_body_text_in_rect(empty_title,
                                  26, {placeholder.x, placeholder.y + 8.0f, placeholder.width, 28.0f},
                                  with_alpha(t.text, grid_alpha), ui::text_align::center);
            if (!loading && state.catalog_request_failed) {
                const std::string detail = !state.catalog_status_message.empty()
                    ? state.catalog_status_message
                    : state.catalog_maintenance
                        ? localization::tr_literal("Online features are temporarily unavailable. Please try again later.")
                        : localization::tr_literal("Check the server URL and confirm raythm-Server is running.");
                draw_browse_body_text_in_rect(detail.c_str(),
                                      14, {placeholder.x + 20.0f, placeholder.y + 42.0f, placeholder.width - 40.0f, 16.0f},
                                      with_alpha(t.text_muted, grid_alpha), ui::text_align::center);
                if (!state.catalog_server_url.empty()) {
                    const std::string server_label =
                        std::string(localization::tr_literal("Tried: ")) + state.catalog_server_url;
                    draw_browse_body_text_in_rect(server_label.c_str(),
                                          12, {placeholder.x + 20.0f, placeholder.y + 58.0f, placeholder.width - 40.0f, 14.0f},
                                          with_alpha(t.text_hint, grid_alpha), ui::text_align::center);
                }
            }
        }

        const std::vector<detail::overview_shelf_row> overview_rows = detail::overview_shelf_rows(state);
        for (int shelf_row = 0; shelf_row < static_cast<int>(overview_rows.size()); ++shelf_row) {
            const detail::overview_shelf_row& row = overview_rows[static_cast<size_t>(shelf_row)];
            const Rectangle prev_arrow = detail::overview_shelf_prev_button_rect(
                current.song_grid_rect, shelf_row, state.song_scroll_y);
            const Rectangle arrow = detail::overview_shelf_next_button_rect(
                current.song_grid_rect, shelf_row, state.song_scroll_y);
            if (arrow.y + arrow.height < current.song_grid_rect.y - 4.0f ||
                arrow.y > current.song_grid_rect.y + current.song_grid_rect.height + 4.0f) {
                continue;
            }

            const float row_top = arrow.y - 34.0f;
            draw_browse_body_text_in_rect(shelf_display_title(row.key).c_str(),
                                  15,
                                  {current.song_grid_rect.x + 10.0f, row_top + 7.0f,
                                   current.song_grid_rect.width - 70.0f, 20.0f},
                                  with_alpha(t.text, grid_alpha), ui::text_align::left);
            if (row.total_count > detail::kSongGridColumns) {
                const auto target_it = state.overview_shelf_scroll_x_target.find(row.key);
                const float target_scroll = target_it == state.overview_shelf_scroll_x_target.end()
                    ? row.scroll_x
                    : target_it->second;
                const bool can_prev = target_scroll > 0.001f || row.scroll_x > 0.001f;
                const bool can_next = target_scroll < static_cast<float>(row.total_count - detail::kSongGridColumns) - 0.001f ||
                    row.scroll_x < static_cast<float>(row.total_count - detail::kSongGridColumns) - 0.001f;
                const auto draw_shelf_arrow = [&](Rectangle rect, bool next, bool enabled) {
                    const bool hovered = enabled && ui::is_hovered(rect);
                    const unsigned char arrow_alpha = static_cast<unsigned char>(
                        (enabled ? (hovered ? hover_row_alpha : normal_row_alpha) : normal_row_alpha / 3) * grid_fade_t);
                    ui::surface(rect,
                                with_alpha(hovered ? button_hover : button_base, arrow_alpha),
                                with_alpha(hovered ? t.border_active : t.border_light,
                                           enabled ? grid_alpha : static_cast<unsigned char>(grid_alpha / 3)),
                                1.15f);
                    const Color icon = with_alpha(enabled ? t.text : t.text_muted,
                                                  enabled ? grid_alpha : static_cast<unsigned char>(grid_alpha / 3));
                    if (next) {
                        raythm_icons::draw_chevron_right(centered_icon_rect(rect, 8.0f), icon, 3.0f);
                    } else {
                        raythm_icons::draw_chevron_left(centered_icon_rect(rect, 8.0f), icon, 3.0f);
                    }
                };
                draw_shelf_arrow(prev_arrow, false, can_prev);
                draw_shelf_arrow(arrow, true, can_next);
            }
        }

        for (int display_index = 0; display_index < static_cast<int>(indices.size()); ++display_index) {
            const int song_index = indices[static_cast<size_t>(display_index)];
            const song_entry_state& song = songs[static_cast<size_t>(song_index)];
            Rectangle card = detail::song_row_rect(state, current.song_grid_rect, display_index, state.song_scroll_y);
            if (card.y + card.height < current.song_grid_rect.y - 4.0f ||
                card.y > current.song_grid_rect.y + current.song_grid_rect.height + 4.0f) {
                continue;
            }
            const Rectangle track = detail::uses_overview_shelves(state)
                ? detail::overview_shelf_track_rect(current.song_grid_rect)
                : current.song_grid_rect;
            if (card.x + card.width < track.x - 4.0f || card.x > track.x + track.width + 4.0f) {
                continue;
            }
            const Rectangle card_clip = detail::uses_overview_shelves(state)
                ? Rectangle{track.x, card.y, track.width, card.height}
                : current.song_grid_rect;
            ui::scoped_clip_rect card_clip_scope(card_clip);

            const bool selected = song_index == detail::selected_song_index_ref(state);
            const bool hovered = ui::is_hovered(card);
            const unsigned char row_alpha = static_cast<unsigned char>((selected ? selected_row_alpha
                : hovered ? hover_row_alpha
                          : normal_row_alpha) * grid_fade_t);
            ui::surface(card,
                        with_alpha(selected ? button_selected : button_base, row_alpha),
                        with_alpha(selected ? t.border_active : t.border_light, grid_alpha),
                        1.15f);

            const Rectangle jacket_rect = song_card_jacket_rect(card);
            if (selected) {
                source_jacket_rect = jacket_rect;
                selected_card_drawn = true;
            }
            const bool hide_selected_jacket = selected && detail_t > 0.001f;
            if (!hide_selected_jacket) {
                if (const Texture2D* jacket = state.jackets.get(song.song.song)) {
                    ui::draw_texture(*jacket, jacket_rect, with_alpha(WHITE, grid_alpha));
                } else {
                    const float selected_placeholder_t = selected
                        ? tween::smoothstep(tween::remap_clamped(detail_t, 0.12f, 0.0f))
                        : 1.0f;
                    const unsigned char placeholder_alpha =
                        static_cast<unsigned char>(static_cast<float>(grid_alpha) * selected_placeholder_t);
                    ui::placeholder(jacket_rect, "JACKET", {
                        .font_size = 18,
                        .body_text = true,
                        .draw_border = false,
                        .fill = with_alpha(t.bg_alt, row_alpha),
                        .text_color = with_alpha(t.text_muted, placeholder_alpha),
                        .custom_colors = true,
                    });
                }
                ui::frame(jacket_rect, with_alpha(t.border_image, grid_alpha), 1.0f);
            }

            const std::string badge_label = detail::song_status_label(song);
            if (!badge_label.empty()) {
                const float badge_width = badge_label.size() > 8 ? 132.0f : 72.0f;
                const Rectangle badge_rect = {card.x + card.width - badge_width - 18.0f,
                                              card.y + 12.0f, badge_width, 18.0f};
                draw_browse_body_text_in_rect(badge_label.c_str(), 12, badge_rect,
                                      with_alpha(detail::song_status_color(song), grid_alpha), ui::text_align::right);
            }

            draw_body_marquee_text(song.song.song.meta.title.c_str(),
                              {jacket_rect.x + jacket_rect.width + 16.0f, jacket_rect.y,
                               card.width - jacket_rect.width - 48.0f, 28.0f},
                              18, with_alpha(t.text, grid_alpha), now);
            const std::string card_subtitle = song_subtitle(song.song.song.meta);
            draw_body_marquee_text(card_subtitle.c_str(),
                              {jacket_rect.x + jacket_rect.width + 16.0f, jacket_rect.y + 28.0f,
                               card.width - jacket_rect.width - 48.0f, 22.0f},
                              13, with_alpha(t.text_muted, grid_alpha), now);
            const auto draw_card_tag_row = [&](const std::vector<std::string>& labels,
                                               float y,
                                               bool keyword) {
                float tag_x = jacket_rect.x + jacket_rect.width + 16.0f;
                const float max_x = card.x + card.width - 18.0f;
                for (const std::string& label : labels) {
                    if (label.empty()) {
                        continue;
                    }
                    const Color tag_color = keyword ? keyword_color_for_label(label) : genre_color_for_label(label);
                    const float width = std::clamp(ui::measure_body_text_size(label.c_str(), 11.0f).x + 16.0f,
                                                   58.0f, 118.0f);
                    if (tag_x + width > max_x) {
                        break;
                    }
                    const Rectangle tag_rect = {tag_x, y, width, 20.0f};
                    ui::surface(tag_rect, with_alpha(button_base, row_alpha), with_alpha(tag_color, grid_alpha), 1.0f);
                    draw_browse_body_text_in_rect(label.c_str(), 11, tag_rect,
                                          with_alpha(tag_color, grid_alpha), ui::text_align::center);
                    tag_x += width + 7.0f;
                }
            };
            draw_card_tag_row(genre_labels(song.song.song.meta), jacket_rect.y + 54.0f, false);
            draw_card_tag_row(song.song.song.meta.keywords, jacket_rect.y + 78.0f, true);
            const float footer_x = card.x + 16.0f;
            const float footer_y = card.y + card.height - 28.0f;
            if (song.song.song.meta.has_play_count) {
                draw_browse_body_text_in_rect(format_count_label("plays", song.song.song.meta.play_count),
                                      12,
                                      {footer_x, footer_y, 88.0f, 16.0f},
                                      with_alpha(t.text_muted, grid_alpha), ui::text_align::left);
            }
            draw_browse_body_text_in_rect(TextFormat("charts %d", std::max(song.song.song.meta.chart_count,
                                                                    static_cast<int>(song.charts.size()))),
                                  12,
                                  {footer_x + (song.song.song.meta.has_play_count ? 96.0f : 0.0f),
                                   footer_y, 120.0f, 16.0f},
                                  with_alpha(t.text_muted, grid_alpha), ui::text_align::left);
        }
    }

    const song_entry_state* song = selected_song(state);
    const chart_entry_state* chart = selected_chart(state);
    if (song == nullptr) {
        return;
    }

    if (!selected_card_drawn) {
        const int display_index = selected_song_display_index(state);
        if (display_index >= 0) {
            source_jacket_rect =
                song_card_jacket_rect(detail::song_row_rect(state, current.song_grid_rect, display_index, state.song_scroll_y));
        }
    }

    if (!state.detail_open) {
        ui::surface(current.preview_panel_rect,
                    with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)),
                    with_alpha(t.border_light, alpha),
                    1.2f);

        const Rectangle jacket_rect = {
            current.preview_panel_rect.x + (current.preview_panel_rect.width - 260.0f) * 0.5f,
            current.preview_panel_rect.y + 38.0f,
            260.0f,
            260.0f,
        };
        if (const Texture2D* jacket = state.jackets.get(song->song.song)) {
            ui::draw_texture(*jacket, jacket_rect, with_alpha(WHITE, alpha));
        } else {
            ui::placeholder(jacket_rect, "JACKET", {
                .font_size = 24,
                .body_text = true,
                .draw_border = false,
                .fill = with_alpha(t.bg_alt, selected_row_alpha),
                .text_color = with_alpha(t.text_muted, alpha),
                .custom_colors = true,
            });
        }
        ui::frame(jacket_rect, with_alpha(t.border_image, alpha), 1.4f);

        draw_body_marquee_text(song->song.song.meta.title.c_str(),
                          {current.preview_panel_rect.x + 26.0f, jacket_rect.y + jacket_rect.height + 24.0f,
                           current.preview_panel_rect.width - 52.0f, 38.0f},
                          25, with_alpha(t.text, alpha), now, ui::text_align::center);
        draw_body_marquee_text(song->song.song.meta.artist.c_str(),
                          {current.preview_panel_rect.x + 26.0f, jacket_rect.y + jacket_rect.height + 62.0f,
                           current.preview_panel_rect.width - 52.0f, 26.0f},
                          17, with_alpha(t.text_secondary, alpha), now, ui::text_align::center);

        const title_preview_snapshot preview = audio_controller.preview_snapshot(&song->song);
        title_preview_transport::skip_button(detail::preview_panel_prev_button_rect(current.preview_panel_rect), false, alpha);
        title_preview_transport::toggle_button(detail::preview_panel_play_button_rect(current.preview_panel_rect),
                                               preview.playing, alpha);
        title_preview_transport::skip_button(detail::preview_panel_next_button_rect(current.preview_panel_rect), true, alpha);
        const Rectangle bar = detail::preview_panel_progress_rect(current.preview_panel_rect);
        const double preview_length = detail::preview_display_length_seconds(*song, preview);
        const double preview_position = state.preview_bar_dragging
            ? state.preview_bar_drag_position_seconds
            : preview.position_seconds;
        const float preview_ratio =
            preview_length > 0.0 ? std::clamp(static_cast<float>(preview_position / preview_length), 0.0f, 1.0f) : 0.0f;
        ui::progress_bar(bar, preview_ratio, {
            .bg = with_alpha(t.bg_alt, normal_row_alpha),
            .fill = with_alpha(t.accent, alpha),
            .border_color = with_alpha(t.border_light, alpha),
            .border_width = 1.0f,
            .custom_colors = true,
        });
        draw_browse_body_text_in_rect(
            TextFormat("%s / %s",
                       detail::format_time_label(preview_position).c_str(),
                       preview_length > 0.0 ? detail::format_time_label(preview_length).c_str() : "--:--"),
            12,
            {bar.x, bar.y + 16.0f, bar.width, 18.0f},
            with_alpha(t.text_muted, alpha), ui::text_align::right);

        draw_browse_body_text_in_rect("GENRES", 13,
                              {current.preview_panel_rect.x + 24.0f, bar.y + 42.0f, 140.0f, 18.0f},
                              with_alpha(t.accent, alpha), ui::text_align::left);
        const auto draw_preview_tag_row = [&](const std::vector<std::string>& labels,
                                              float y,
                                              Color fallback_color,
                                              int color_mode,
                                              int max_rows) {
            float x = current.preview_panel_rect.x + 24.0f;
            float row_y = y;
            int row = 0;
            const float max_x = current.preview_panel_rect.x + current.preview_panel_rect.width - 24.0f;
            for (const std::string& label : labels) {
                if (label.empty()) {
                    continue;
                }
                const Color color = color_mode == 1 ? genre_color_for_label(label)
                    : color_mode == 2             ? keyword_color_for_label(label)
                                                  : fallback_color;
                const float width = std::clamp(ui::measure_body_text_size(label.c_str(), 13.0f).x + 22.0f, 70.0f, 150.0f);
                if (x + width > max_x) {
                    ++row;
                    if (row >= max_rows) {
                        break;
                    }
                    x = current.preview_panel_rect.x + 24.0f;
                    row_y += 38.0f;
                }
                const Rectangle tag_rect = {x, row_y, width, 30.0f};
                ui::surface(tag_rect, with_alpha(button_base, normal_row_alpha), with_alpha(color, alpha), 1.0f);
                draw_browse_body_text_in_rect(label.c_str(), 13, tag_rect,
                                      with_alpha(color, alpha), ui::text_align::center);
                x += width + 8.0f;
            }
        };
        draw_preview_tag_row(genre_labels(song->song.song.meta), bar.y + 64.0f, t.accent, 1, 2);
        draw_browse_body_text_in_rect("KEYWORDS", 13,
                              {current.preview_panel_rect.x + 24.0f, bar.y + 152.0f, 140.0f, 18.0f},
                              with_alpha(t.accent, alpha), ui::text_align::left);
        draw_preview_tag_row(song->song.song.meta.keywords, bar.y + 174.0f, t.fast, 2, 2);
        const auto draw_preview_stat = [&](Rectangle rect, const char* label, const char* value, Color accent) {
            ui::surface(rect, with_alpha(button_base, normal_row_alpha), with_alpha(accent, alpha), 1.0f);
            draw_browse_body_text_in_rect(label, 11,
                                  {rect.x + 10.0f, rect.y + 7.0f, rect.width - 20.0f, 14.0f},
                                  with_alpha(t.text_muted, alpha), ui::text_align::left);
            draw_browse_body_text_in_rect(value, 17,
                                  {rect.x + 10.0f, rect.y + 22.0f, rect.width - 20.0f, 20.0f},
                                  with_alpha(t.text, alpha), ui::text_align::right);
        };
        const float stat_gap = 10.0f;
        const float stat_width = (current.preview_panel_rect.width - 48.0f - stat_gap) * 0.5f;
        const Rectangle bpm_stat = {current.preview_panel_rect.x + 24.0f, bar.y + 262.0f, stat_width, 48.0f};
        const Rectangle charts_stat = {bpm_stat.x + stat_width + stat_gap, bpm_stat.y, stat_width, bpm_stat.height};
        draw_preview_stat(bpm_stat, "BPM", TextFormat("%.0f", song->song.song.meta.base_bpm), t.accent);
        draw_preview_stat(charts_stat, "CHARTS",
                          TextFormat("%d", std::max(song->song.song.meta.chart_count,
                                                    static_cast<int>(song->charts.size()))),
                          t.fast);
        draw_toned_button(detail::preview_panel_open_button_rect(current.preview_panel_rect), "OPEN SONG", 16,
                          t.accent, alpha, selected_row_alpha, hover_row_alpha);
        return;
    }

    const Rectangle animated_jacket_rect = tween::lerp(source_jacket_rect, current.hero_jacket_rect, jacket_t);

    ui::surface(current.detail_left_rect,
                with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * detail_content_t / 2.0f)),
                with_alpha(t.border_light, detail_alpha),
                1.2f);
    ui::surface(current.detail_right_rect,
                with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * detail_content_t / 2.0f)),
                with_alpha(t.border_light, detail_alpha),
                1.2f);
    ui::surface(current.detail_preview_rect,
                with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * detail_content_t / 2.0f)),
                with_alpha(t.border_light, detail_alpha),
                1.2f);

    if (detail_alpha == 0) {
        return;
    }

    const Rectangle preview_panel = current.detail_preview_rect;
    if (detail_t > 0.001f) {
        if (const Texture2D* hero_jacket = state.jackets.get(song->song.song)) {
            ui::draw_texture(*hero_jacket, animated_jacket_rect, with_alpha(WHITE, alpha));
        } else {
            ui::placeholder(animated_jacket_rect, "JACKET", {
                .font_size = 22,
                .body_text = true,
                .draw_border = false,
                .fill = with_alpha(t.bg_alt, selected_row_alpha),
                .text_color = with_alpha(t.text_muted, detail_alpha),
                .custom_colors = true,
            });
        }
        ui::frame(animated_jacket_rect, with_alpha(t.border_image, alpha), 1.5f);
    }

    const detail_preview_layout detail_layout =
        detail_preview_layout_for(current.hero_jacket_rect,
                                  preview_panel,
                                  current.preview_bar_rect,
                                  current.primary_action_rect);
    draw_body_marquee_text(song->song.song.meta.title.c_str(), detail_layout.title, 24,
                           with_alpha(t.text, detail_alpha), now);
    draw_body_marquee_text(song->song.song.meta.artist.c_str(),
                      detail_layout.artist,
                      15, with_alpha(t.text_secondary, detail_alpha), now);
    const auto draw_compact_preview_tags = [&](const char* heading,
                                               const std::vector<std::string>& labels,
                                               compact_tag_row_layout tag_layout,
                                               int color_mode) {
        draw_browse_body_text_in_rect(localization::tr_literal(heading), 11,
                              tag_layout.heading,
                              with_alpha(t.accent, detail_alpha), ui::text_align::left);
        float x = tag_layout.tag_area.x;
        const float max_x = tag_layout.tag_area.x + tag_layout.tag_area.width;
        int drawn = 0;
        for (const std::string& label : labels) {
            if (label.empty() || drawn >= 3) {
                continue;
            }
            const Color color = color_mode == 1 ? genre_color_for_label(label) : keyword_color_for_label(label);
            const float width = std::clamp(ui::measure_body_text_size(label.c_str(), 12.0f).x + 20.0f, 70.0f, 138.0f);
            if (x + width > max_x) {
                break;
            }
            const Rectangle tag_rect = {x, tag_layout.tag_area.y, width, tag_layout.tag_area.height};
            ui::surface(tag_rect, with_alpha(button_base, normal_row_alpha), with_alpha(color, detail_alpha), 1.0f);
            draw_browse_body_text_in_rect(label.c_str(), 12, tag_rect,
                                  with_alpha(color, detail_alpha), ui::text_align::center);
            x += width + 8.0f;
            ++drawn;
        }
    };
    draw_compact_preview_tags("GENRES", genre_labels(song->song.song.meta), detail_layout.genres, 1);
    draw_compact_preview_tags("KEYWORDS", song->song.song.meta.keywords, detail_layout.keywords, 2);
    if (chart != nullptr) {
        draw_browse_body_text_in_rect(
            TextFormat("%s  %s",
                       detail::key_mode_label(chart->chart.meta.key_count).c_str(),
                       chart->chart.meta.difficulty.c_str()),
            13,
            detail_layout.chart_summary,
            with_alpha(t.text, detail_alpha), ui::text_align::left);
        draw_difficulty_level_badge(chart->chart.meta.level,
                                    detail_layout.chart_level_badge,
                                    12, detail_alpha);
        draw_browse_body_text_in_rect(
            TextFormat("by %s", chart->chart.meta.chart_author.empty() ? "Unknown" : chart->chart.meta.chart_author.c_str()),
            12,
            detail_layout.chart_author,
            with_alpha(t.text_muted, detail_alpha), ui::text_align::right);
    }

    const title_preview_snapshot preview = audio_controller.preview_snapshot(&song->song);
    const double preview_length = detail::preview_display_length_seconds(*song, preview);
    const double preview_position = state.preview_bar_dragging
        ? state.preview_bar_drag_position_seconds
        : preview.position_seconds;
    const float preview_ratio =
        preview_length > 0.0 ? std::clamp(static_cast<float>(preview_position / preview_length), 0.0f, 1.0f) : 0.0f;
    ui::progress_bar(current.preview_bar_rect, preview_ratio, {
        .bg = with_alpha(t.bg_alt, static_cast<unsigned char>(normal_row_alpha * detail_content_t)),
        .fill = with_alpha(t.accent, detail_alpha),
        .border_color = with_alpha(t.border_light, detail_alpha),
        .border_width = 1.0f,
        .custom_colors = true,
    });
    draw_browse_body_text_in_rect(
        TextFormat("%s / %s",
                   detail::format_time_label(preview_position).c_str(),
                   preview_length > 0.0 ? detail::format_time_label(preview_length).c_str() : "--:--"),
        12,
        detail_layout.preview_time,
        with_alpha(t.text_muted, detail_alpha), ui::text_align::right);
    title_preview_transport::toggle_button(current.preview_play_rect, preview.playing, detail_alpha);

    const preview_ranking_layout ranking_layout = preview_ranking_layout_for(preview_panel);
    draw_browse_body_text_in_rect(localization::tr_literal("GLOBAL RANKING"), 14, ranking_layout.header,
                          with_alpha(t.accent, detail_alpha), ui::text_align::left);
    ui::surface(ranking_layout.table,
                with_alpha(button_base, static_cast<unsigned char>(normal_row_alpha * detail_content_t)),
                with_alpha(t.border_light, detail_alpha),
                1.0f);
    draw_browse_body_text_in_rect("#", 10, ranking_layout.placement_header,
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
    draw_browse_body_text_in_rect("PLAYER", 10, ranking_layout.player_header,
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
    draw_browse_body_text_in_rect("SCORE", 10, ranking_layout.score_header,
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::right);
    draw_browse_body_text_in_rect("ACC", 10, ranking_layout.accuracy_header,
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::right);
    draw_browse_body_text_in_rect("CLEAR", 10, ranking_layout.clear_header,
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::right);
    if (!state.ranking_listing.available || state.ranking_listing.entries.empty()) {
        const std::string message = state.ranking_loading ? "Loading global ranking..."
            : (state.ranking_listing.message.empty() ? "No global entries yet." : state.ranking_listing.message);
        draw_browse_body_text_in_rect(message.c_str(), 13, ranking_layout.empty_message,
                              with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
    } else {
        const int row_count = std::min(6, static_cast<int>(state.ranking_listing.entries.size()));
        for (int i = 0; i < row_count; ++i) {
            const ranking_service::entry& entry = state.ranking_listing.entries[static_cast<size_t>(i)];
            const preview_ranking_row_layout row_layout =
                preview_ranking_row_layout_for(ranking_layout.table, i);
            ui::surface_fill(row_layout.row, with_alpha(i % 2 == 0 ? t.section : button_base,
                                             static_cast<unsigned char>(normal_row_alpha * detail_content_t)));
            draw_browse_body_text_in_rect(TextFormat("%d", entry.placement), 12,
                                  row_layout.placement,
                                  with_alpha(t.text, detail_alpha), ui::text_align::left);
            draw_body_marquee_text(entry.player_display_name.c_str(),
                              row_layout.player,
                              12, with_alpha(t.text, detail_alpha), now);
            draw_browse_body_text_in_rect(format_score(entry.score).c_str(), 12,
                                  row_layout.score,
                                  with_alpha(t.text_secondary, detail_alpha), ui::text_align::right);
            draw_browse_body_text_in_rect(TextFormat("%.2f%%", entry.accuracy), 12,
                                  row_layout.accuracy,
                                  with_alpha(t.text_secondary, detail_alpha), ui::text_align::right);
            draw_browse_body_text_in_rect(rank_label(entry.clear_rank()), 13,
                                  row_layout.clear,
                                  with_alpha(rank_color(entry.clear_rank()), detail_alpha), ui::text_align::right);
        }
    }

    const bool selected_chart_update =
        chart != nullptr && chart->installed && chart->update_available;
    const bool selected_chart_repair =
        chart != nullptr && chart->installed && content_sync_service::is_modified(chart->chart.sync_state);
    const bool selected_song_repair = content_sync_service::is_modified(song->song.sync_state);
    const bool song_lifecycle_blocked = lifecycle_blocks_song_download(*song);
    const bool selected_play_locked =
        content_unlock_is_locked(song->song.song.meta.extra.unlock) ||
        (chart != nullptr && content_unlock_is_locked(chart->chart.meta.extra.unlock));
    const std::string selected_lock_reason = chart != nullptr
        ? content_play_lock_reason(song->song.song.meta, chart->chart.meta)
        : content_unlock_reason_or_default(song->song.song.meta.extra.unlock);
    const std::string song_lifecycle_label = song->song.online_identity.has_value()
        ? content_lifecycle::display_label(song->song.online_identity->review_status,
                                           song->song.online_identity->lifecycle_status)
        : "";
    const std::string primary_label = state.download_in_progress ? "DOWNLOADING..."
        : (song_lifecycle_blocked ? (song_lifecycle_label.empty() ? "UNAVAILABLE" : song_lifecycle_label)
           : needs_download(*song) ? (selected_song_repair ? "REPAIR SONG"
                                      : song->update_available ? "UPDATE SONG"
                                                               : "DOWNLOAD SONG")
           : (selected_chart_repair ? "REPAIR CHART"
                                    : selected_chart_update ? "UPDATE CHART"
                                    : selected_play_locked ? "LOCKED"
                                                           : "OPEN LOCAL"));
    if (state.download_in_progress && state.download_progress) {
        const int total_steps = std::max(1, state.download_progress->total_steps.load());
        const int completed_steps = std::clamp(state.download_progress->completed_steps.load(), 0, total_steps);
        const size_t current_bytes = state.download_progress->current_bytes.load();
        const size_t current_total_bytes = state.download_progress->current_total_bytes.load();
        const float current_ratio = current_total_bytes > 0
            ? std::clamp(static_cast<float>(static_cast<double>(current_bytes) /
                                            static_cast<double>(current_total_bytes)), 0.0f, 1.0f)
            : 0.0f;
        const float progress_ratio =
            std::clamp((static_cast<float>(completed_steps) + current_ratio) /
                           static_cast<float>(total_steps),
                       0.0f, 1.0f);
        ui::progress_bar(detail_layout.download_progress, progress_ratio, {
            .bg = with_alpha(t.bg_alt, static_cast<unsigned char>(normal_row_alpha * detail_content_t)),
            .fill = with_alpha(t.accent, detail_alpha),
            .border_color = with_alpha(t.border_light, detail_alpha),
            .border_width = 1.0f,
            .custom_colors = true,
        });
    }
    if (selected_play_locked && !selected_lock_reason.empty() && !needs_download(*song)) {
        draw_browse_body_text_in_rect(selected_lock_reason.c_str(), 12,
                              detail_layout.lock_reason,
                              with_alpha(t.slow, detail_alpha),
                              ui::text_align::center);
    }
    draw_toned_button(current.primary_action_rect,
                      primary_label.c_str(),
                      15,
                      selected_play_locked && !needs_download(*song) ? t.slow :
                      action_tone_for_state(song->update_available || selected_chart_update || selected_chart_repair,
                                            song->installed,
                                            state.download_in_progress),
                      detail_alpha,
                      selected_row_alpha,
                      hover_row_alpha);

    const Rectangle filter_panel = current.detail_left_rect;
    const detail::chart_filter_layout filter_layout = detail::chart_filter_layout_for(filter_panel);
    draw_browse_body_text_in_rect(localization::tr_literal("FILTER"), 15,
                          filter_layout.title_label,
                          with_alpha(t.accent, detail_alpha), ui::text_align::left);
    draw_song_search_input(filter_layout.search_input, state.chart_search_input,
                           "", localization::tr_literal("Search"),
                           13, 80,
                           button_base, button_hover, button_selected,
                           normal_row_alpha, hover_row_alpha, selected_row_alpha,
                           detail_alpha);
    draw_browse_body_text_in_rect(localization::tr_literal("SOURCE"), 12,
                          filter_layout.source_label,
                          with_alpha(t.accent, detail_alpha), ui::text_align::left);
    const auto draw_filter_button = [&](Rectangle rect, const char* label, bool active) {
        song_select::filter_widget::option_button(rect, label, {
            .font_size = 11,
            .border_width = 1.0f,
            .selected = active,
            .base = button_base,
            .hover = button_hover,
            .selected_base = button_selected,
            .selected_hover = button_selected,
            .text = t.text_secondary,
            .selected_text = t.text,
            .normal_alpha = normal_row_alpha,
            .hover_alpha = hover_row_alpha,
            .selected_alpha = selected_row_alpha,
            .selected_hover_alpha = selected_row_alpha,
            .text_alpha = detail_alpha,
        });
    };
    for (int index = 0; index < static_cast<int>(detail::kChartSourceFilters.size()); ++index) {
        const detail::chart_source_filter_option& option =
            detail::kChartSourceFilters[static_cast<size_t>(index)];
        const bool active = state.chart_source == option.value;
        draw_filter_button(detail::chart_source_button_rect(filter_panel, index), option.label, active);
    }

    draw_browse_body_text_in_rect(localization::tr_literal("STATUS"), 12,
                          filter_layout.status_label,
                          with_alpha(t.accent, detail_alpha), ui::text_align::left);
    for (int index = 0; index < static_cast<int>(detail::kChartStatusFilters.size()); ++index) {
        const detail::chart_status_filter_option& option =
            detail::kChartStatusFilters[static_cast<size_t>(index)];
        const bool active = state.chart_download_filter == option.value;
        draw_filter_button(detail::chart_status_button_rect(filter_panel, index), option.label, active);
    }

    draw_browse_body_text_in_rect(localization::tr_literal("LEVEL"), 12,
                          filter_layout.level_label,
                          with_alpha(t.accent, detail_alpha), ui::text_align::left);
    draw_level_range_slider(filter_panel, state, detail_alpha);

    draw_browse_body_text_in_rect(localization::tr_literal("KEYS"), 12,
                          filter_layout.keys_label,
                          with_alpha(t.accent, detail_alpha), ui::text_align::left);
    for (int index = 0; index < static_cast<int>(detail::kChartKeyFilters.size()); ++index) {
        const detail::chart_key_filter_option& option =
            detail::kChartKeyFilters[static_cast<size_t>(index)];
        const bool active = state.chart_key_filter == option.value;
        draw_filter_button(detail::chart_key_button_rect(filter_panel, index), option.label, active);
    }

    const bool filters_active = chart_filters_active(state);
    song_select::filter_widget::option_button(detail::chart_clear_button_rect(filter_panel), localization::tr_literal("CLEAR FILTERS"), {
        .font_size = 12,
        .border_width = 1.0f,
        .selected = filters_active,
        .base = t.section,
        .hover = button_hover,
        .selected_base = button_base,
        .selected_hover = button_hover,
        .text = t.text_muted,
        .selected_text = t.text_secondary,
        .normal_alpha = normal_row_alpha,
        .hover_alpha = hover_row_alpha,
        .selected_alpha = normal_row_alpha,
        .selected_hover_alpha = hover_row_alpha,
        .text_alpha = detail_alpha,
    });
    const auto chart_indices = detail::filtered_chart_indices(state);
    const int visible_chart_count = song->song.song.meta.chart_count > 0
        ? std::max(song->song.song.meta.chart_count, static_cast<int>(song->charts.size()))
        : static_cast<int>(song->charts.size());
    const int filtered_chart_count = static_cast<int>(chart_indices.size());
    const detail::chart_list_header_layout chart_header =
        detail::chart_list_header_layout_for(current.detail_right_rect);
    draw_browse_body_text_in_rect("CHARTS", 15,
                          chart_header.title,
                          with_alpha(t.text, detail_alpha), ui::text_align::left);
    draw_browse_body_text_in_rect("Find charts to download", 12,
                          chart_header.subtitle,
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
    draw_browse_body_text_in_rect(
        !chart_filters_active(state)
            ? TextFormat("%d items", visible_chart_count)
            : TextFormat("%d / %d items", filtered_chart_count, visible_chart_count),
        14,
                          chart_header.count,
                          with_alpha(t.text_muted, detail_alpha), ui::text_align::right);

    ui::scoped_clip_rect chart_clip(current.chart_list_rect);
    if (song->charts.empty() || chart_indices.empty()) {
        const Rectangle placeholder = detail::chart_empty_placeholder_rect(current.chart_list_rect);
        ui::surface(placeholder,
                    with_alpha(button_base, static_cast<unsigned char>(selected_row_alpha * detail_content_t)),
                    with_alpha(t.border_light, detail_alpha),
                    1.5f);
        const char* chart_empty = song->charts_loading ? "Loading charts..."
            : (song->charts_failed ? "Could not load charts."
                                   : (song->charts.empty() ? "No charts found."
                                                           : "No charts match."));
        draw_browse_body_text_in_rect(chart_empty, 28, placeholder, with_alpha(t.text, detail_alpha), ui::text_align::center);
    }

    for (int display_index = 0; display_index < static_cast<int>(chart_indices.size()); ++display_index) {
        const int index = chart_indices[static_cast<size_t>(display_index)];
        const chart_entry_state& item = song->charts[static_cast<size_t>(index)];
        const Rectangle card = detail::chart_row_rect(current.chart_list_rect, display_index, state.chart_scroll_y);
        if (card.y + card.height < current.chart_list_rect.y - 4.0f ||
            card.y > current.chart_list_rect.y + current.chart_list_rect.height + 4.0f) {
            continue;
        }

        const bool selected = chart != nullptr && index == detail::selected_chart_index_ref(state);
        const bool hovered = ui::is_hovered(card);
        const unsigned char row_alpha = static_cast<unsigned char>((selected ? selected_row_alpha
            : hovered ? hover_row_alpha
                      : normal_row_alpha) * detail_content_t);
        ui::surface(card,
                    with_alpha(selected ? button_selected : button_base, row_alpha),
                    with_alpha(selected ? t.border_active : t.border_light, detail_alpha),
                    1.0f);

        const detail::chart_row_layout row_layout = detail::chart_row_layout_for(card);
        draw_browse_body_text_in_rect(detail::key_mode_label(item.chart.meta.key_count).c_str(),
                              15,
                              row_layout.key_mode,
                              with_alpha(detail::key_mode_color(item.chart.meta.key_count), detail_alpha),
                              ui::text_align::left);
        draw_browse_body_text_in_rect(item.chart.meta.difficulty.c_str(),
                              15,
                              row_layout.difficulty,
                              with_alpha(t.text, detail_alpha),
                              ui::text_align::left);
        const bool can_download_chart = !state.download_in_progress && detail::can_download_chart(*song, item);
        const bool chart_locked = content_is_play_locked(song->song.song.meta, item.chart.meta);
        const std::string chart_badge = chart_locked ? "LOCKED" : detail::chart_status_label(item);
        const Rectangle download_icon_rect = row_layout.download_icon;
        const bool has_review_badge =
            item.chart.online_identity.has_value() &&
            !content_lifecycle::display_label(item.chart.online_identity->review_status,
                                              item.chart.online_identity->lifecycle_status).empty();
        const bool show_chart_badge = !chart_badge.empty() && (!can_download_chart || has_review_badge || chart_locked);
        if (show_chart_badge) {
            const float badge_width = chart_badge.size() > 8 ? 122.0f : 62.0f;
            const Rectangle badge_rect =
                detail::chart_status_badge_rect(card, badge_width, can_download_chart);
            const bool chart_modified = content_sync_service::is_modified(item.chart.sync_state);
            Color badge_color = chart_modified ? t.slow
                : item.update_available ? t.accent
                                        : t.text_muted;
            if (chart_locked) {
                badge_color = t.slow;
            }
            if (has_review_badge) {
                badge_color = content_lifecycle::is_pending_review(item.chart.online_identity->review_status,
                                                                    item.chart.online_identity->lifecycle_status)
                    ? t.accent
                    : t.slow;
            }
            draw_browse_body_text_in_rect(chart_badge.c_str(), 12,
                                  badge_rect,
                                  with_alpha(badge_color, detail_alpha),
                                  ui::text_align::right);
        }
        if (can_download_chart) {
            draw_download_icon_button(download_icon_rect,
                                      item.update_available ||
                                          content_sync_service::is_modified(item.chart.sync_state),
                                      detail_alpha);
        }
        draw_difficulty_level_badge(item.chart.meta.level,
                                    row_layout.level_badge,
                                    12, detail_alpha);
        draw_browse_body_text_in_rect(TextFormat("%d notes", item.chart.note_count), 13,
                              row_layout.notes,
                              with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
        draw_browse_body_text_in_rect(TextFormat("by %s", item.chart.meta.chart_author.empty()
                                                   ? "Unknown"
                                                   : item.chart.meta.chart_author.c_str()),
                              13,
                              row_layout.author,
                              with_alpha(t.text_muted, detail_alpha), ui::text_align::left);
        const char* source_label = chart_source_label(item.chart.source_status);
        const float source_badge_width = item.chart.source_status == content_status::community ? 104.0f
            : item.chart.source_status == content_status::official ? 84.0f
                                                                    : 58.0f;
        const Rectangle source_badge =
            detail::chart_source_badge_rect(card, source_badge_width, can_download_chart, show_chart_badge);
        const Color source_color = chart_source_status_color(item);
        ui::surface(source_badge,
                    with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha * detail_content_t)),
                    with_alpha(source_color, detail_alpha),
                    1.0f);
        draw_browse_body_text_in_rect(source_label, 13, source_badge, with_alpha(source_color, detail_alpha),
                              ui::text_align::center);
    }
}

}  // namespace title_online_view
