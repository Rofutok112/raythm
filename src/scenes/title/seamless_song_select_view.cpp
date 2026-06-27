#include "title/seamless_song_select_view.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "chart_difficulty.h"
#include "localization/localization.h"
#include "ranking_service.h"
#include "raylib.h"
#include "scene_common.h"
#include "shared/content_status_badge.h"
#include "song_loader.h"
#include "song_select/song_select_filter_widget.h"
#include "song_select/song_select_level_filter_widget.h"
#include "song_select/ranking_source_policy.h"
#include "song_select/song_select_confirmation_dialog.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_login_dialog.h"
#include "title/center_panel_view.h"
#include "title/create_tools_view.h"
#include "title/preview_transport_widget.h"
#include "title/ranking_panel_view.h"
#include "title/song_list_view.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_text_input.h"
#include "ui_tooltip.h"
#include "ui/icons/raythm_icons.h"
#include "virtual_screen.h"

namespace title_play_view {
namespace {

constexpr float kContextMenuInnerPadding = 6.0f;
constexpr int kPlaySongContextMenuItemCount = 1;
constexpr int kPlayChartContextMenuItemCount = 1;

Rectangle centered_icon_rect(Rectangle rect, float inset) {
    const float size = std::max(1.0f, std::min(rect.width, rect.height) - inset * 2.0f);
    return {
        rect.x + (rect.width - size) * 0.5f,
        rect.y + (rect.height - size) * 0.5f,
        size,
        size
    };
}

Rectangle context_menu_item_rect(Rectangle menu_rect, int index = 0) {
    return {
        menu_rect.x + kContextMenuInnerPadding,
        menu_rect.y + kContextMenuInnerPadding +
            static_cast<float>(index) *
                (song_select::layout::kContextMenuItemHeight + song_select::layout::kContextMenuItemSpacing),
        menu_rect.width - kContextMenuInnerPadding * 2.0f,
        song_select::layout::kContextMenuItemHeight
    };
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
        return "-";
    }
    const int total_seconds = static_cast<int>(std::round(seconds));
    return TextFormat("%d:%02d", total_seconds / 60, total_seconds % 60);
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
    const auto& t = *g_theme;
    switch (value) {
        case rank::ss: return t.rank_ss;
        case rank::s: return t.rank_s;
        case rank::aa: return t.rank_aa;
        case rank::a: return t.rank_a;
        case rank::b: return t.rank_b;
        case rank::c: return t.rank_c;
        case rank::f: return t.rank_f;
    }
    return t.text_secondary;
}

std::string format_score(int value) {
    std::string digits = std::to_string(std::max(0, value));
    for (int insert_at = static_cast<int>(digits.size()) - 3; insert_at > 0; insert_at -= 3) {
        digits.insert(static_cast<size_t>(insert_at), ",");
    }
    return digits;
}

std::string mod_summary(const play_mods& mods) {
    std::vector<std::string> labels;
    if (mods.auto_play) {
        labels.push_back("AUTO");
    }
    if (mods.no_fail) {
        labels.push_back("NOFAIL");
    }
    if (labels.empty()) {
        return "No Mod";
    }
    std::string summary = labels.front();
    for (size_t i = 1; i < labels.size(); ++i) {
        summary += " + " + labels[i];
    }
    return summary;
}

std::string format_bpm_range(float min_bpm, float max_bpm, float fallback_bpm) {
    if (min_bpm <= 0.0f && max_bpm <= 0.0f) {
        return TextFormat("%.0f", fallback_bpm);
    }
    if (std::fabs(max_bpm - min_bpm) < 0.05f) {
        return TextFormat("%.0f", min_bpm);
    }
    return TextFormat("%.0f-%.0f", min_bpm, max_bpm);
}

unsigned char scaled_alpha(unsigned char alpha, float scale) {
    return static_cast<unsigned char>(
        std::clamp(static_cast<float>(alpha) * scale, 0.0f, 255.0f));
}

void draw_right_pane_frame(Rectangle rect, unsigned char alpha, unsigned char normal_row_alpha) {
    const auto& t = *g_theme;
    ui::surface(rect,
                with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)),
                with_alpha(t.border_light, alpha),
                1.2f);
}

void draw_chart_summary_pane_frame(Rectangle rect,
                                   const song_select::chart_option* chart,
                                   unsigned char alpha,
                                   unsigned char normal_row_alpha) {
    const auto& t = *g_theme;
    const Color chart_tone = chart != nullptr ? difficulty_level_color(chart->meta.level) : t.border_light;
    ui::surface_fill(rect, with_alpha(lerp_color(t.section, chart_tone, chart != nullptr ? 0.045f : 0.0f),
                                      static_cast<unsigned char>(normal_row_alpha / 2)));
    ui::accent_bar({rect.x, rect.y, 4.0f, rect.height},
                   with_alpha(chart_tone, chart != nullptr ? scaled_alpha(alpha, 0.82f) : 0));
    ui::divider({rect.x + 4.0f, rect.y, rect.width - 4.0f, 2.0f},
                with_alpha(chart_tone, chart != nullptr ? scaled_alpha(alpha, 0.18f) : 0));
    ui::frame(rect, with_alpha(t.border_light, scaled_alpha(alpha, 0.82f)), 1.2f);
}

void draw_play_search_input(Rectangle rect,
                            ui::text_input_state& input,
                            unsigned char alpha,
                            Color button_base,
                            Color button_hover,
                            Color button_selected,
                            unsigned char normal_row_alpha,
                            unsigned char hover_row_alpha,
                            unsigned char selected_row_alpha) {
    (void)button_hover;
    ui::search_text_input(rect, input, "", localization::tr_literal("Search"), {
        .font_size = 13,
        .max_length = 80,
        .content_padding_x = 12.0f,
        .button_base = button_base,
        .button_selected = button_selected,
        .normal_row_alpha = normal_row_alpha,
        .hover_row_alpha = hover_row_alpha,
        .selected_row_alpha = selected_row_alpha,
        .alpha = alpha,
    });
}

void draw_play_filter_panel(const title_play_view::layout& current,
                            song_select::state& state,
                            unsigned char alpha,
                            Color button_base,
                            Color button_selected,
                            unsigned char normal_row_alpha,
                            unsigned char selected_row_alpha) {
    const auto& t = *g_theme;
    const Rectangle panel = current.song_column;
    ui::surface(panel,
                with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)),
                with_alpha(t.border_light, alpha),
                1.2f);

    ui::draw_text_in_rect("ALL SONGS", 14,
                          {panel.x + 18.0f, panel.y + 18.0f, panel.width * 0.5f, 24.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);
    const std::vector<int> indices = song_select::filtered_song_indices(state);
    ui::draw_text_in_rect(TextFormat("%d songs", static_cast<int>(indices.size())), 12,
                          {panel.x + panel.width * 0.5f, panel.y + 18.0f, panel.width * 0.5f - 18.0f, 24.0f},
                          with_alpha(t.text_muted, alpha), ui::text_align::right);

    const Rectangle search = {panel.x + 18.0f, panel.y + 62.0f, panel.width - 94.0f, 46.0f};
    draw_play_search_input(search, state.play_search_input, alpha, button_base, t.row_soft_hover,
                           button_selected, normal_row_alpha, normal_row_alpha, selected_row_alpha);

    const bool filters_active = state.chart_source != song_select::chart_source_filter::all ||
                                !state.play_search_input.value.empty() ||
                                state.chart_key_filter != 0 ||
                                std::fabs(state.chart_min_level - kChartFilterMinLevel) > 0.001f ||
                                std::fabs(state.chart_max_level - kChartFilterMaxLevel) > 0.001f;
    song_select::filter_widget::filter_icon_button(play_filter_button_rect(panel), {
        .border_width = 1.2f,
        .active = filters_active || state.play_filter_modal_open,
        .base = button_base,
        .hover = button_base,
        .active_base = button_selected,
        .active_hover = button_selected,
        .border = t.border_light,
        .active_border = t.accent,
        .icon = t.text_secondary,
        .active_icon = t.accent,
        .normal_alpha = normal_row_alpha,
        .hover_alpha = normal_row_alpha,
        .active_alpha = selected_row_alpha,
        .active_hover_alpha = selected_row_alpha,
        .icon_alpha = alpha,
    });
}

void draw_play_filter_modal(const title_play_view::layout& current,
                            const song_select::state& state,
                            unsigned char alpha,
                            Color button_base,
                            Color button_selected,
                            unsigned char normal_row_alpha,
                            unsigned char selected_row_alpha) {
    if (!state.play_filter_modal_open) {
        return;
    }
    const auto& t = *g_theme;
    const Rectangle panel = play_filter_modal_rect(current);
    ui::backdrop({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                 with_alpha(BLACK, static_cast<unsigned char>(80.0f * (static_cast<float>(alpha) / 255.0f))));
    ui::surface(panel,
                with_alpha(t.panel, static_cast<unsigned char>(235.0f * (static_cast<float>(alpha) / 255.0f))),
                with_alpha(t.border_active, alpha),
                1.4f);

    const auto draw_heading = [&](const char* label, float y) {
        ui::draw_text_in_rect(label, 12, {panel.x + 18.0f, y, panel.width - 36.0f, 18.0f},
                              with_alpha(t.accent, alpha), ui::text_align::left);
    };
    const auto draw_button = [&](Rectangle rect, const char* label, bool selected) {
        song_select::filter_widget::option_button(rect, label, {
            .font_size = 11,
            .border_width = 1.0f,
            .selected = selected,
            .base = button_base,
            .hover = t.row_soft_hover,
            .selected_base = button_selected,
            .selected_hover = t.row_soft_hover,
            .text = t.text,
            .selected_text = t.text,
            .normal_alpha = normal_row_alpha,
            .hover_alpha = normal_row_alpha,
            .selected_alpha = selected_row_alpha,
            .selected_hover_alpha = normal_row_alpha,
            .text_alpha = alpha,
        });
    };
    ui::draw_text_in_rect("FILTER", 16, {panel.x + 18.0f, panel.y + 16.0f, panel.width - 36.0f, 24.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);
    draw_heading("SOURCE", panel.y + 60.0f);
    draw_button(play_filter_source_button_rect(panel, 0), "ALL",
                state.chart_source == song_select::chart_source_filter::all);
    draw_button(play_filter_source_button_rect(panel, 1), "OFFICIAL",
                state.chart_source == song_select::chart_source_filter::official);
    draw_button(play_filter_source_button_rect(panel, 2), "COMMUNITY",
                state.chart_source == song_select::chart_source_filter::community);

    draw_heading("LEVEL", panel.y + 164.0f);
    const Rectangle range = play_filter_level_slider_rect(panel);
    song_select::level_filter::draw_range_slider(range, state.chart_min_level, state.chart_max_level, alpha);

    draw_heading("KEYS", panel.y + 292.0f);
    const char* key_labels[] = {"ALL", "4K", "5K", "6K", "7K"};
    for (int i = 0; i < 5; ++i) {
        draw_button(play_filter_key_button_rect(panel, i), key_labels[i],
                    state.chart_key_filter == (i == 0 ? 0 : i + 3));
    }

    const bool filters_active = state.chart_source != song_select::chart_source_filter::all ||
                                !state.play_search_input.value.empty() ||
                                state.chart_key_filter != 0 ||
                                std::fabs(state.chart_min_level - kChartFilterMinLevel) > 0.001f ||
                                std::fabs(state.chart_max_level - kChartFilterMaxLevel) > 0.001f;
    draw_button(play_filter_clear_button_rect(panel), "CLEAR FILTERS", filters_active);
}

const char* difficulty_factor_label(const std::string& name) {
    if (name == "density") {
        return "Density";
    }
    if (name == "stream") {
        return "Stream";
    }
    if (name == "pattern") {
        return "Pattern";
    }
    if (name == "read") {
        return "Read";
    }
    if (name == "rhythm") {
        return "Rhythm";
    }
    if (name == "tempo") {
        return "Tempo";
    }
    if (name == "rest") {
        return "No Rest";
    }
    if (name == "overlap") {
        return "LN Mix";
    }
    if (name == "hold_conflict") {
        return "LN Hand";
    }
    if (name == "hand") {
        return "Hand";
    }
    if (name == "stamina") {
        return "Stamina";
    }
    if (name == "release") {
        return "Release";
    }
    if (name == "jump") {
        return "Motion";
    }
    if (name == "chord") {
        return "Chord";
    }
    if (name == "hold") {
        return "Hold";
    }
    if (name == "balance") {
        return "Balance";
    }
    return name.c_str();
}

Color difficulty_factor_color(const std::string& name) {
    if (name == "density") {
        return {240, 186, 70, 255};
    }
    if (name == "stream" || name == "stamina") {
        return {70, 190, 230, 255};
    }
    if (name == "tempo") {
        return {82, 210, 214, 255};
    }
    if (name == "pattern" || name == "rhythm") {
        return {168, 106, 245, 255};
    }
    if (name == "release" || name == "hold" || name == "hold_conflict") {
        return {236, 105, 142, 255};
    }
    if (name == "overlap" || name == "read") {
        return {74, 205, 158, 255};
    }
    if (name == "hand" || name == "balance") {
        return {245, 132, 78, 255};
    }
    if (name == "rest") {
        return {225, 210, 90, 255};
    }
    if (name == "jump" || name == "chord") {
        return {96, 145, 238, 255};
    }
    return g_theme->accent;
}

float status_badge_width(content_status status) {
    switch (status) {
        case content_status::community: return 92.0f;
        case content_status::official: return 78.0f;
        case content_status::modified: return 76.0f;
        case content_status::checking: return 76.0f;
        case content_status::update: return 64.0f;
        case content_status::local: return 54.0f;
    }
    return 54.0f;
}

void draw_square_status_badge(Rectangle rect, content_status status, unsigned char alpha, int font_size = 10) {
    const Color color = content_status_badge::color(status);
    if (status == content_status::modified) {
        raythm_icons::draw_triangle_alert(centered_icon_rect(rect, 1.0f), with_alpha(color, alpha), 2.6f);
        return;
    }

    ui::surface(rect, with_alpha(g_theme->row_soft, scaled_alpha(alpha, 0.27f)), with_alpha(color, alpha), 1.0f);
    ui::draw_text_in_rect(content_status_badge::label(status), font_size, rect,
                          with_alpha(color, alpha), ui::text_align::center);
}

void enqueue_modified_status_tooltip(Rectangle badge_rect, content_status status, unsigned char alpha) {
    if (status != content_status::modified) {
        return;
    }
    ui::enqueue_hover_tooltip(centered_icon_rect(badge_rect, 1.0f), "変更されています", alpha);
}

std::optional<chart_difficulty::difficulty_breakdown> cached_difficulty_breakdown(const song_select::chart_option* chart) {
    static std::unordered_map<std::string, std::optional<chart_difficulty::difficulty_breakdown>> cache;
    if (chart == nullptr || chart->path.empty()) {
        return std::nullopt;
    }

    const auto found = cache.find(chart->path);
    if (found != cache.end()) {
        return found->second;
    }

    const chart_parse_result parsed = song_loader::load_chart(chart->path);
    if (!parsed.success || !parsed.data.has_value()) {
        cache.emplace(chart->path, std::nullopt);
        return std::nullopt;
    }
    auto breakdown = chart_difficulty::calculate_breakdown(*parsed.data);
    cache.emplace(chart->path, breakdown);
    return breakdown;
}

constexpr int kDifficultyFactorVisibleRows = 5;
constexpr float kDifficultyFactorRowHeight = 19.0f;
constexpr float kDifficultyFactorLabelWidth = 88.0f;
constexpr float kDifficultyFactorValueWidth = 34.0f;
constexpr float kDifficultyFactorFullBarContribution = 4.0f;

struct difficulty_breakdown_layout {
    Rectangle header_label;
    Rectangle header_line;
    Rectangle empty_label;
};

difficulty_breakdown_layout difficulty_breakdown_rects(Rectangle rect) {
    return {
        {rect.x, rect.y, rect.width, 17.0f},
        {rect.x, rect.y + 15.0f, rect.width, 1.0f},
        {rect.x, rect.y + 20.0f, rect.width, 24.0f},
    };
}

struct difficulty_factor_row_layout {
    Rectangle label;
    Rectangle bar_track;
    Rectangle value;
};

difficulty_factor_row_layout difficulty_factor_row_rects(Rectangle rect, int index) {
    const float row_y = rect.y + 24.0f + static_cast<float>(index) * kDifficultyFactorRowHeight;
    const Rectangle label = {
        rect.x,
        row_y - 2.0f,
        kDifficultyFactorLabelWidth,
        kDifficultyFactorRowHeight,
    };
    const Rectangle bar_track = {
        rect.x + kDifficultyFactorLabelWidth,
        row_y + 4.0f,
        rect.width - kDifficultyFactorLabelWidth - kDifficultyFactorValueWidth,
        9.0f,
    };
    const Rectangle value = {
        bar_track.x + bar_track.width + 6.0f,
        row_y - 2.0f,
        kDifficultyFactorValueWidth - 6.0f,
        kDifficultyFactorRowHeight,
    };
    return {label, bar_track, value};
}

float difficulty_factor_reveal_t(int index, float chart_change_anim_t) {
    const float raw_reveal_t = 1.0f - std::clamp(chart_change_anim_t, 0.0f, 1.0f);
    const float row_delay = static_cast<float>(index) * 0.045f;
    return tween::ease_out_cubic(
        std::clamp((raw_reveal_t - row_delay) / (1.0f - row_delay), 0.0f, 1.0f));
}

Rectangle difficulty_factor_fill_rect(Rectangle bar_track, float ratio, float reveal_t) {
    return {
        bar_track.x,
        bar_track.y,
        bar_track.width * ratio * reveal_t,
        bar_track.height,
    };
}

Rectangle difficulty_factor_sweep_rect(Rectangle fill) {
    return {
        fill.x + std::max(0.0f, fill.width - 10.0f),
        fill.y,
        std::min(10.0f, fill.width),
        fill.height,
    };
}

struct chart_summary_frame_layout {
    Rectangle accent;
    Rectangle top_divider;
    Rectangle bottom_divider;
    Rectangle empty_label;
};

chart_summary_frame_layout chart_summary_frame_rects(Rectangle rect) {
    return {
        {rect.x, rect.y, 4.0f, rect.height},
        {rect.x + 4.0f, rect.y, rect.width - 4.0f, 1.0f},
        {rect.x + 4.0f, rect.y + rect.height - 1.0f, rect.width - 4.0f, 1.0f},
        ui::inset(rect, 12.0f),
    };
}

struct chart_summary_content_layout {
    Rectangle key_chip;
    Rectangle source_badge;
    Rectangle level_badge;
    Rectangle difficulty;
    Rectangle author_prefix;
    Rectangle author_name;
};

chart_summary_content_layout chart_summary_content_rects(Rectangle rect, const song_select::chart_option& chart) {
    const Rectangle content = ui::inset(rect, ui::edge_insets::symmetric(0.0f, 16.0f));
    const float source_width = status_badge_width(chart.source_status);
    const Rectangle key_chip = {content.x, content.y + 8.0f, 42.0f, 26.0f};
    const Rectangle level_badge = {content.x + content.width - 82.0f, content.y + 7.0f, 78.0f, 28.0f};
    const Rectangle source_badge = {
        level_badge.x - source_width - 10.0f,
        content.y + 10.0f,
        source_width,
        22.0f,
    };
    constexpr float kAuthorReservedWidth = 220.0f;
    const float difficulty_x = key_chip.x + key_chip.width + 14.0f;
    const float max_difficulty_width =
        std::max(0.0f, source_badge.x - difficulty_x - kAuthorReservedWidth - 24.0f);
    const float measured_difficulty_width = ui::measure_text_size(chart.meta.difficulty.c_str(), 22.0f).x + 8.0f;
    const float difficulty_width = std::clamp(measured_difficulty_width, 64.0f, max_difficulty_width);
    const Rectangle difficulty = {
        difficulty_x,
        content.y + 5.0f,
        difficulty_width,
        32.0f,
    };
    const float author_x = std::max(difficulty.x + difficulty.width + 18.0f,
                                    content.x + 210.0f);
    const Rectangle author = {
        author_x,
        content.y + 10.0f,
        std::max(0.0f, source_badge.x - author_x - 10.0f),
        22.0f,
    };
    const Rectangle author_prefix = {author.x, author.y, 18.0f, author.height};
    const Rectangle author_name = {
        author_prefix.x + author_prefix.width + 8.0f,
        author.y,
        std::max(0.0f, author.width - author_prefix.width - 8.0f),
        author.height,
    };
    return {key_chip, source_badge, level_badge, difficulty, author_prefix, author_name};
}

struct selected_chart_info_layout {
    Rectangle heading;
    Rectangle key;
    Rectangle difficulty;
    Rectangle author;
    Rectangle level_badge;
    Rectangle source_badge;
    Rectangle lock_reason;
    Rectangle difficulty_breakdown;
    Rectangle empty_label;
};

selected_chart_info_layout selected_chart_info_rects(Rectangle info_panel) {
    const Rectangle key = {info_panel.x + 28.0f, info_panel.y + 72.0f, 78.0f, 54.0f};
    return {
        {info_panel.x + 28.0f, info_panel.y + 24.0f, info_panel.width - 56.0f, 24.0f},
        key,
        {key.x + key.width + 10.0f, key.y + 2.0f, 180.0f, 48.0f},
        {key.x + key.width + 204.0f, key.y + 17.0f, 170.0f, 22.0f},
        {info_panel.x + info_panel.width - 146.0f, info_panel.y + 82.0f, 88.0f, 28.0f},
        {info_panel.x + info_panel.width - 250.0f, info_panel.y + 84.0f, 86.0f, 24.0f},
        {info_panel.x + 28.0f, info_panel.y + 134.0f, info_panel.width - 56.0f, 18.0f},
        {info_panel.x + 28.0f, info_panel.y + 164.0f, info_panel.width - 56.0f, 94.0f},
        {info_panel.x + 28.0f, info_panel.y + 86.0f, info_panel.width - 56.0f, 48.0f},
    };
}

void draw_difficulty_breakdown(Rectangle rect,
                               const song_select::chart_option* chart,
                               unsigned char alpha,
                               unsigned char normal_row_alpha,
                               float chart_change_anim_t) {
    const auto& t = *g_theme;
    const difficulty_breakdown_layout layout = difficulty_breakdown_rects(rect);
    ui::draw_text_in_rect("DIFFICULTY FACTORS", 12, layout.header_label,
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    ui::divider(layout.header_line, with_alpha(t.border_light, scaled_alpha(alpha, 0.55f)));

    const std::optional<chart_difficulty::difficulty_breakdown> breakdown = cached_difficulty_breakdown(chart);
    if (!breakdown.has_value() || breakdown->factors.empty()) {
        ui::draw_text_in_rect("-", 13, layout.empty_label,
                              with_alpha(t.text_muted, alpha), ui::text_align::left);
        return;
    }

    std::vector<const chart_difficulty::difficulty_factor_breakdown*> visible_factors;
    visible_factors.reserve(kDifficultyFactorVisibleRows);
    const chart_difficulty::difficulty_factor_breakdown* rhythm_factor = nullptr;
    for (const chart_difficulty::difficulty_factor_breakdown& factor : breakdown->factors) {
        if (factor.name == "rhythm") {
            rhythm_factor = &factor;
        }
        if (static_cast<int>(visible_factors.size()) < kDifficultyFactorVisibleRows) {
            visible_factors.push_back(&factor);
        }
    }
    if (rhythm_factor != nullptr &&
        std::find(visible_factors.begin(), visible_factors.end(), rhythm_factor) == visible_factors.end()) {
        if (visible_factors.empty()) {
            visible_factors.push_back(rhythm_factor);
        } else {
            visible_factors.back() = rhythm_factor;
        }
    }

    const int rows = static_cast<int>(visible_factors.size());
    for (int i = 0; i < rows; ++i) {
        const chart_difficulty::difficulty_factor_breakdown& factor = *visible_factors[static_cast<size_t>(i)];
        const difficulty_factor_row_layout row = difficulty_factor_row_rects(rect, i);
        const float ratio =
            std::clamp(factor.average_contribution / kDifficultyFactorFullBarContribution, 0.0f, 1.0f);
        const Rectangle fill = difficulty_factor_fill_rect(row.bar_track, ratio,
                                                           difficulty_factor_reveal_t(i, chart_change_anim_t));
        const Color factor_color = difficulty_factor_color(factor.name);
        ui::draw_text_in_rect(difficulty_factor_label(factor.name), 12, row.label,
                              with_alpha(factor_color, alpha), ui::text_align::left);
        ui::surface_fill(row.bar_track, with_alpha(t.bg_alt, normal_row_alpha));
        ui::divider({row.bar_track.x, row.bar_track.y, row.bar_track.width, 1.0f},
                    with_alpha(t.border_light, scaled_alpha(alpha, 0.35f)));
        ui::surface_fill(fill, with_alpha(factor_color, alpha));
        if (chart_change_anim_t > 0.001f && fill.width > 3.0f) {
            ui::surface_fill(difficulty_factor_sweep_rect(fill),
                             with_alpha(WHITE, static_cast<unsigned char>(alpha / 3)));
        }
        ui::draw_text_in_rect(TextFormat("%.1f", factor.average_contribution), 11, row.value,
                              with_alpha(t.text_muted, alpha), ui::text_align::right);
    }
}

void draw_chart_summary(Rectangle rect,
                        const song_select::chart_option* chart,
                        unsigned char alpha,
                        unsigned char normal_row_alpha) {
    const auto& t = *g_theme;
    const Color level_color = chart != nullptr ? difficulty_level_color(chart->meta.level) : t.accent;
    const Color fill = lerp_color(t.bg_alt, level_color, chart != nullptr ? 0.08f : 0.0f);
    const chart_summary_frame_layout frame = chart_summary_frame_rects(rect);
    ui::surface_fill(rect, with_alpha(fill, normal_row_alpha));
    ui::accent_bar(frame.accent, with_alpha(level_color, alpha));
    ui::divider(frame.top_divider, with_alpha(t.border_light, scaled_alpha(alpha, 0.65f)));
    ui::divider(frame.bottom_divider, with_alpha(t.border_light, scaled_alpha(alpha, 0.45f)));

    if (chart == nullptr) {
        ui::draw_text_in_rect("No chart selected", 18, frame.empty_label,
                              with_alpha(t.text_muted, alpha), ui::text_align::left);
        return;
    }

    const int keys = chart->meta.key_count > 0 ? chart->meta.key_count : 4;
    const chart_summary_content_layout summary = chart_summary_content_rects(rect, *chart);

    ui::surface(summary.key_chip,
                with_alpha(lerp_color(t.section, level_color, 0.18f), scaled_alpha(alpha, 0.88f)),
                with_alpha(level_color, scaled_alpha(alpha, 0.72f)),
                1.0f);
    ui::draw_text_in_rect(TextFormat("%dK", keys), 15, summary.key_chip,
                          with_alpha(t.text, alpha), ui::text_align::center);
    draw_square_status_badge(summary.source_badge, chart->source_status, alpha, 10);
    enqueue_modified_status_tooltip(summary.source_badge, chart->source_status, alpha);
    draw_marquee_text(chart->meta.difficulty.c_str(), summary.difficulty,
                      22, with_alpha(t.text, alpha), GetTime());
    draw_difficulty_level_badge(chart->meta.level, summary.level_badge, 15, alpha);
    ui::draw_text_in_rect("by", 13, summary.author_prefix,
                          with_alpha(t.text_muted, alpha), ui::text_align::left);
    draw_marquee_text(chart->meta.chart_author.empty() ? "Unknown" : chart->meta.chart_author.c_str(),
                      summary.author_name, 13, with_alpha(t.text_muted, alpha), GetTime());
}

void draw_preview_and_start_panel(const title_play_view::layout& current,
                                  const song_select::state& state,
                                  const title_preview_snapshot& preview,
                                  const song_select::song_entry* song,
                                  const song_select::chart_option* chart,
                                  bool draw_play_actions,
                                  unsigned char alpha,
                                  Color button_base,
                                  Color button_hover,
                                  Color button_selected,
                                  unsigned char normal_row_alpha,
                                  unsigned char hover_row_alpha,
                                  unsigned char selected_row_alpha) {
    const auto& t = *g_theme;
    const Rectangle panel = current.ranking_column;
    const Rectangle info_panel = right_top_pane_rect(panel);
    const Rectangle ranking_panel = right_bottom_pane_rect(panel);
    const bool play_locked = song != nullptr && chart != nullptr &&
                             content_is_play_locked(song->song.meta, chart->meta);
    const std::string lock_reason = play_locked
        ? content_play_lock_reason(song->song.meta, chart->meta)
        : "";
    draw_chart_summary_pane_frame(info_panel, chart, alpha, normal_row_alpha);
    if (draw_play_actions) {
        draw_right_pane_frame(ranking_panel, alpha, normal_row_alpha);
    }

    const selected_chart_info_layout selected = selected_chart_info_rects(info_panel);
    ui::draw_text_in_rect("選択中の譜面", 16, selected.heading,
                          with_alpha(t.text, alpha), ui::text_align::left);
    if (chart != nullptr) {
        const int keys = chart->meta.key_count > 0 ? chart->meta.key_count : 4;
        ui::draw_text_in_rect(TextFormat("%dK", keys), 33, selected.key,
                              with_alpha(t.success, alpha), ui::text_align::left);
        draw_marquee_text(chart->meta.difficulty.c_str(), selected.difficulty,
                          32, with_alpha(t.text, alpha), GetTime());
        draw_marquee_text((std::string("by ") +
                           (chart->meta.chart_author.empty() ? "Unknown" : chart->meta.chart_author)).c_str(),
                          selected.author,
                          14, with_alpha(t.text_muted, alpha), GetTime());
        draw_difficulty_level_badge(chart->meta.level, selected.level_badge, 14, alpha);
        draw_square_status_badge(selected.source_badge, chart->source_status, alpha, 10);
        enqueue_modified_status_tooltip(selected.source_badge, chart->source_status, alpha);
        if (play_locked) {
            ui::backdrop(selected.source_badge, with_alpha(t.bg, scaled_alpha(alpha, 0.54f)));
            raythm_icons::draw_lock(centered_icon_rect(selected.source_badge, 3.0f),
                                    with_alpha(t.slow, alpha), 2.7f);
            ui::draw_text_in_rect(lock_reason.c_str(), 12, selected.lock_reason,
                                  with_alpha(t.slow, alpha), ui::text_align::left);
        }
        draw_difficulty_breakdown(selected.difficulty_breakdown, chart, alpha, normal_row_alpha,
                                  state.chart_change_anim_t);
    } else {
        ui::draw_text_in_rect("No chart selected", 22, selected.empty_label,
                              with_alpha(t.text_muted, alpha), ui::text_align::left);
    }

    const Rectangle progress = current.meta_rect;
    const bool preview_loading =
        preview.audio.status == song_select::preview_audio_loader::load_status::loading;
    const double length = preview.length_seconds;
    const double pos = preview_loading ? 0.0 : state.preview_bar_dragging
        ? state.preview_bar_drag_position_seconds
        : preview.position_seconds;
    const float ratio = length > 0.0 ? std::clamp(static_cast<float>(pos / length), 0.0f, 1.0f) : 0.0f;
    const unsigned char progress_alpha = preview_loading ? scaled_alpha(alpha, 0.38f) : alpha;
    ui::progress_bar(progress, ratio, {
        .bg = with_alpha(t.bg_alt, preview_loading
            ? scaled_alpha(normal_row_alpha, 0.58f)
            : normal_row_alpha),
        .fill = with_alpha(t.accent, progress_alpha),
        .border_color = with_alpha(t.border_light, progress_alpha),
        .border_width = 1.0f,
        .custom_colors = true,
    });
    ui::draw_text_in_rect(
        preview_loading
            ? "loading"
            : TextFormat("%s / %s",
                         format_duration_label(static_cast<float>(pos)).c_str(),
                         length > 0.0 ? format_duration_label(static_cast<float>(length)).c_str() : "--:--"),
        12,
        {progress.x, progress.y + 11.0f, progress.width, 16.0f},
        with_alpha(t.text_muted, progress_alpha), ui::text_align::right);

    const Rectangle prev_button = preview_prev_button_rect(current);
    const Rectangle play_button = preview_play_button_rect(current);
    const Rectangle next_button = preview_next_button_rect(current);
    title_preview_transport::skip_button(prev_button, false, alpha);
    title_preview_transport::toggle_button(play_button, preview.playing, alpha, {
        .hover_border_accent = true,
    });
    title_preview_transport::skip_button(next_button, true, alpha);

    if (!draw_play_actions) {
        return;
    }

    const Rectangle mods = mod_button_rect(panel);
    const bool mods_hovered = ui::is_hovered(mods);
    const bool mods_pressed = ui::is_pressed(mods);
    const Rectangle mods_visual = mods_pressed ? ui::inset(mods, 1.5f) : mods;
    const bool mods_active = state.mods.any_enabled();
    ui::surface(mods_visual,
                with_alpha(mods_active ? button_selected : button_base,
                           mods_hovered ? hover_row_alpha : normal_row_alpha),
                with_alpha(mods_active ? t.accent : t.border_light, alpha),
                mods_active ? 1.6f : 1.0f);
    raythm_icons::draw_settings_gear(
        centered_icon_rect({mods_visual.x + 14.0f, mods_visual.y, 42.0f, mods_visual.height}, 8.0f),
        with_alpha(mods_active ? t.accent : t.text_secondary, alpha), 3.0f);
    ui::draw_text_in_rect("MODS", 18, {mods_visual.x + 62.0f, mods_visual.y + 8.0f, 112.0f, 24.0f},
                          with_alpha(t.text, alpha), ui::text_align::left);
    const std::string mods_label = mod_summary(state.mods);
    ui::draw_text_in_rect(mods_label.c_str(), 14,
                          {mods_visual.x + 62.0f, mods_visual.y + 30.0f, 130.0f, 20.0f},
                          with_alpha(mods_active ? t.accent : t.text_muted, alpha), ui::text_align::left);
    const Rectangle start_rect = start_button_rect(panel);
    ui::button(start_rect,
               localization::tr_literal(play_locked ? "" :
                   (state.filter.multiplayer_queueable_only ? "SELECT" : "START")), {
                   .font_size = 22,
                   .border_width = 1.4f,
                   .bg = with_alpha(play_locked ? lerp_color(t.section, t.slow, 0.20f) : button_selected,
                                    selected_row_alpha),
                   .bg_hover = with_alpha(play_locked ? lerp_color(t.section, t.slow, 0.28f) : button_hover,
                                          hover_row_alpha),
                   .text_color = with_alpha(t.text, alpha),
                   .custom_colors = true,
               });
    if (play_locked) {
        raythm_icons::draw_lock(centered_icon_rect(start_rect, 18.0f), with_alpha(t.slow, alpha), 3.2f);
    }
}

void draw_mod_toggle(Rectangle rect,
                     const char* label,
                     const char* description,
                     bool enabled,
                     unsigned char alpha,
                     unsigned char normal_row_alpha,
                     unsigned char hover_row_alpha) {
    const auto& t = *g_theme;
    const Color fill = enabled ? lerp_color(t.row_soft_selected, t.accent, 0.20f) : t.row_soft;
    ui::toggle_row(rect, label, description, enabled, {
        .layer = song_select::layout::kContextMenuLayer,
        .label_font_size = 18,
        .description_font_size = 12,
        .border_width = 1.0f,
        .checked_border_width = 1.6f,
        .bg = with_alpha(fill, normal_row_alpha),
        .bg_hover = with_alpha(fill, hover_row_alpha),
        .border_color = with_alpha(t.border_light, alpha),
        .checked_border_color = with_alpha(t.accent, alpha),
        .label_color = with_alpha(t.text, alpha),
        .description_color = with_alpha(t.text_muted, alpha),
        .switch_bg = enabled ? with_alpha(t.accent, 160) : with_alpha(t.text_muted, 70),
        .switch_border_color = with_alpha(enabled ? t.accent : t.border_light, alpha),
        .knob_color = with_alpha(enabled ? t.text : t.text_secondary, alpha),
        .custom_colors = true,
    });
}

void draw_mod_modal(const title_play_view::layout& current,
                    const song_select::state& state,
                    unsigned char alpha,
                    unsigned char normal_row_alpha,
                    unsigned char hover_row_alpha) {
    if (!state.play_mod_modal_open) {
        return;
    }
    const Rectangle modal = mod_modal_rect(current.ranking_column);
    ui::enqueue_draw_command(song_select::layout::kContextMenuLayer,
                             [modal, mods = state.mods, alpha, normal_row_alpha, hover_row_alpha]() {
        using namespace title_play_view::mod_layout;
        const auto& theme = *g_theme;
        ui::surface(modal, with_alpha(theme.section, 242), with_alpha(theme.border_light, alpha), 1.4f);
        const Rectangle content = {
            modal.x + kModalSidePadding,
            modal.y + kModalTopPadding,
            modal.width - kModalSidePadding * 2.0f,
            modal.height - kModalTopPadding - kModalBottomPadding,
        };
        ui::draw_text_in_rect("MOD SETTINGS", 13,
                              {content.x, content.y, content.width, kHeaderHeight},
                              with_alpha(theme.text_muted, alpha), ui::text_align::left);
        ui::draw_text_in_rect("Score submission is disabled while mods are active.", 11,
                              {content.x,
                               content.y + kHeaderHeight + kHeaderToDescriptionGap,
                               content.width,
                               kDescriptionHeight},
                              with_alpha(theme.text_muted, alpha), ui::text_align::left);
        draw_mod_toggle(auto_mod_toggle_rect(modal), "Auto", "Perfect autoplay",
                        mods.auto_play, alpha, normal_row_alpha, hover_row_alpha);
        draw_mod_toggle(no_fail_mod_toggle_rect(modal), "NoFail", "Survive gauge failure",
                        mods.no_fail, alpha, normal_row_alpha, hover_row_alpha);
    });
}

void enqueue_delete_context_menu(Rectangle menu_rect, const char* label) {
    const auto& t = *g_theme;
    const Rectangle item_rect = context_menu_item_rect(menu_rect);
    const bool hovered = ui::is_hovered(item_rect, song_select::layout::kContextMenuLayer);
    const bool pressed = ui::is_pressed(item_rect, song_select::layout::kContextMenuLayer);
    const Rectangle visual = pressed ? ui::inset(item_rect, 1.5f) : item_rect;
    const Color bg = hovered ? t.row_hover : t.row;
    const Color fill = with_alpha(lerp_color(bg, t.error, hovered ? 0.16f : 0.08f), 228);
    const Color border = with_alpha(lerp_color(t.border, t.error, 0.35f), 220);
    const std::string label_copy = label != nullptr ? label : "";

    ui::enqueue_draw_command(song_select::layout::kContextMenuLayer,
                             [menu_rect, item_rect, visual, fill, border, label_copy]() {
        ui::section(menu_rect);
        ui::surface(visual, fill, border, 1.5f);
        const Rectangle accent = {
            visual.x + 10.0f,
            visual.y + 9.0f,
            4.0f,
            std::max(0.0f, visual.height - 18.0f),
        };
        ui::accent_bar(accent, with_alpha(g_theme->error, 220));
        ui::draw_text_in_rect(label_copy.c_str(), 16,
                              {visual.x + 24.0f, visual.y, visual.width - 34.0f, visual.height},
                              g_theme->text, ui::text_align::left);
    });
}

}  // namespace

void draw(song_select::state& state,
          const title_selection_media_snapshot& media,
          mode view_mode,
          float anim_t,
          Rectangle origin_rect,
          const title_create_tools_model::view_model* create_tools_model) {
    const auto& t = *g_theme;
    const float play_t = tween::ease_out_cubic(anim_t);
    if (play_t <= 0.01f) {
        return;
    }

    const layout current = make_mode_layout(anim_t, origin_rect, view_mode);
    const float content_fade_t = std::clamp((play_t - 0.18f) / 0.62f, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(255.0f * content_fade_t);
    const bool hide_unloaded_content =
        state.catalog_loading && !state.catalog_loaded_once && state.songs.empty();
    const double now = GetTime();
    const Color button_base = t.row_soft;
    const Color button_hover = t.row_soft_hover;
    const Color button_selected = t.row_soft_selected;
    const Color button_selected_hover = t.row_soft_selected_hover;
    const unsigned char normal_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_alpha) / 255);
    const unsigned char hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_hover_alpha) / 255);
    const unsigned char selected_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_alpha) / 255);
    const unsigned char selected_hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_hover_alpha) / 255);
    ui::button(current.back_rect, "戻る", {
        .font_size = 16,
        .border_width = 1.5f,
        .bg = with_alpha(button_base, normal_row_alpha),
        .bg_hover = with_alpha(button_hover, hover_row_alpha),
        .text_color = with_alpha(t.text, alpha),
        .custom_colors = true,
    });

    draw_play_filter_panel(current, state, alpha, button_base, button_selected, normal_row_alpha, selected_row_alpha);
    ui::surface(current.main_column,
                with_alpha(t.section, static_cast<unsigned char>(normal_row_alpha / 2)),
                with_alpha(t.border_light, alpha),
                1.2f);

    if (!hide_unloaded_content) {
        title_song_list_view::draw(state, {
            .column_rect = song_list_rect(current),
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_selected = button_selected,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .expanded_cards = true,
            .show_header = false,
            .embedded_chart_scroll_y = 0.0f,
            .now = now,
        });
    }

    const song_select::song_entry* song = song_select::selected_song(state);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
    const title_preview_snapshot& preview = media.preview;

    if (!hide_unloaded_content) {
        title_center_view::draw(state, preview, song, chart, filtered, {
            .main_column_rect = current.main_column,
            .jacket_rect = center_jacket_rect(current),
            .chart_detail_rect = center_detail_rect(current),
            .chart_buttons_rect = current.chart_buttons_rect,
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_selected = button_selected,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .compact_song_header = true,
            .now = now,
        });
    }

    if (hide_unloaded_content) {
        return;
    }
    ui::divider({current.main_column.x + 24.0f, current.main_column.y + 342.0f,
                 current.main_column.width - 48.0f, 1.0f},
                with_alpha(t.border_light, alpha));
    draw_preview_and_start_panel(current, state, preview, song, chart, view_mode == mode::play, alpha, button_base,
                                 button_hover, button_selected, normal_row_alpha, hover_row_alpha,
                                 selected_row_alpha);
    if (view_mode == mode::play) {
        title_ranking_view::draw(state.ranking_panel, {
            .header_rect = current.ranking_header_rect,
            .source_friends_rect = current.ranking_source_friends_rect,
            .source_local_rect = current.ranking_source_local_rect,
            .source_online_rect = current.ranking_source_online_rect,
            .list_rect = current.ranking_list_rect,
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_hover = button_hover,
            .button_selected = button_selected,
            .button_selected_hover = button_selected_hover,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .selected_hover_row_alpha = selected_hover_row_alpha,
            .source_availability = song_select::ranking_source_policy::availability_for_chart(chart),
            .ranking_snapshot = media.ranking,
            .self_player_display_name = state.auth.display_name,
            .avatar_base_url = state.auth.server_url,
        });
    } else {
        draw_right_pane_frame(right_bottom_pane_rect(current.ranking_column), alpha, normal_row_alpha);
        const title_create_tools_model::view_model empty_create_tools;
        title_create_tools_view::draw(create_tools_model != nullptr ? *create_tools_model : empty_create_tools, {
            .current = current,
            .alpha = alpha,
            .button_base = button_base,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
        });
    }

    if (view_mode == mode::play || view_mode == mode::create) {
        draw_play_filter_modal(current, state, alpha, button_base, button_selected, normal_row_alpha, selected_row_alpha);
    }
    if (view_mode == mode::play) {
        draw_mod_modal(current, state, alpha, normal_row_alpha, hover_row_alpha);
    }

    if (view_mode == mode::play &&
        state.context_menu.open &&
        (state.context_menu.target == song_select::context_menu_target::song ||
         state.context_menu.target == song_select::context_menu_target::chart)) {
        enqueue_delete_context_menu(
            state.context_menu.rect,
            state.context_menu.target == song_select::context_menu_target::chart
                ? "DELETE CHART"
                : "DELETE SONG");
    }

}

}  // namespace title_play_view
