#include "result/result_scene_view.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <string>

#include "core/tween.h"
#include "localization/localization.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_layout.h"
#include "ui/ui_font.h"

namespace result_scene_view {
namespace {

constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kContentRect = {36.0f, 108.0f, 1848.0f, 786.0f};
constexpr Rectangle kTitleRect = {36.0f, 28.0f, 520.0f, 62.0f};
constexpr Rectangle kMainPanelRect = {36.0f, 108.0f, 1080.0f, 790.0f};
constexpr Rectangle kJacketRect = {60.0f, 132.0f, 320.0f, 320.0f};
constexpr Rectangle kSongInfoRect = {404.0f, 146.0f, 650.0f, 178.0f};
constexpr Rectangle kRankRect = {60.0f, 518.0f, 382.0f, 184.0f};
constexpr Rectangle kScoreRect = {476.0f, 528.0f, 590.0f, 150.0f};
constexpr Rectangle kAchievementRect = {60.0f, 792.0f, 1030.0f, 78.0f};
constexpr Rectangle kMetricTopRowRect = {1152.0f, 130.0f, 596.0f, 158.0f};
constexpr ui::rect_pair kMetricTopColumns = ui::split_columns(kMetricTopRowRect, 312.0f, 18.0f);
constexpr Rectangle kMetricAccuracyRect = kMetricTopColumns.first;
constexpr Rectangle kMetricComboRect = kMetricTopColumns.second;
constexpr Rectangle kMetricMidRowRect = {1152.0f, 306.0f, 596.0f, 146.0f};
constexpr ui::rect_pair kMetricMidColumns = ui::split_columns(kMetricMidRowRect, 266.0f, 18.0f);
constexpr Rectangle kMetricOffsetRect = kMetricMidColumns.first;
constexpr Rectangle kOffsetRect = kMetricMidColumns.second;
constexpr Rectangle kJudgementRect = {kMetricTopRowRect.x, 470.0f, kMetricTopRowRect.width, 428.0f};
constexpr Rectangle kActionBarRect = {50.0f, 928.0f, 1820.0f, 92.0f};
constexpr float kActionButtonGap = 56.0f;
constexpr std::array<float, 3> kActionButtonWidths = {520.0f, 668.0f, 520.0f};

struct action_button_definition {
    action value;
    const char* label;
};

constexpr std::array<action_button_definition, 3> kActionButtons = {{
    {action::retry, "Retry"},
    {action::song_select, "Song Select"},
    {action::replay, "Replay"},
}};

struct action_button_layout {
    action value;
    Rectangle rect;
    const char* label;
};

struct result_metric_descriptor {
    Rectangle rect;
    const char* label;
    std::string value;
    Color tone;
    float reveal_t;
};

struct achievement_chip_descriptor {
    Rectangle rect;
    const char* label;
    std::string value;
    Color tone;
};

struct song_text_descriptor {
    Rectangle rect;
    const char* text;
    int font_size;
    Color color;
};

struct song_badge_descriptor {
    Rectangle rect;
    std::string text;
    Color text_color;
    Color border_color;
    float horizontal_inset;
};

struct song_value_descriptor {
    Rectangle rect;
    std::string text;
    int font_size;
    Color color;
    ui::text_align align;
};

struct result_title_layout {
    Rectangle accent;
    Rectangle title;
    Rectangle rule;
};

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
    switch (value) {
        case rank::ss: return g_theme->rank_ss;
        case rank::s: return g_theme->rank_s;
        case rank::aa: return g_theme->rank_aa;
        case rank::a: return g_theme->rank_a;
        case rank::b: return g_theme->rank_b;
        case rank::c: return g_theme->rank_c;
        case rank::f: return g_theme->rank_f;
    }
    return g_theme->text_secondary;
}

const char* key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

std::string format_score(int score) {
    std::string value = std::to_string(std::max(0, score));
    for (int i = static_cast<int>(value.size()) - 3; i > 0; i -= 3) {
        value.insert(static_cast<size_t>(i), ",");
    }
    return value;
}

std::string format_delta(int delta) {
    if (delta > 0) {
        return "+" + format_score(delta);
    }
    if (delta < 0) {
        return "-" + format_score(-delta);
    }
    return "+0";
}

std::string localized_text(const std::string& text) {
    return localization::tr_literal(text.c_str());
}

Rectangle text_rect(float x, float y, float width, int font_size) {
    return {x, y, width, ui::text_layout_font_size(static_cast<float>(font_size))};
}

float measured_width(const std::string& text, int font_size) {
    return ui::measure_text_size(text.c_str(), static_cast<float>(font_size), 0.0f).x;
}

int fit_font_size(const std::string& text, int desired_size, float max_width, int min_size = 14) {
    const std::string localized = localized_text(text);
    int size = desired_size;
    while (size > min_size && measured_width(localized, size) > max_width) {
        --size;
    }
    return size;
}

void draw_fit_text(const std::string& text, int desired_size, Rectangle rect, Color color,
                   ui::text_align align = ui::text_align::left, int min_size = 14) {
    const std::string localized = localized_text(text);
    const int size = fit_font_size(text, desired_size, rect.width, min_size);
    ui::draw_text_in_rect(localized.c_str(), size, rect, color, align);
}

Color alpha(Color color, float amount) {
    return with_alpha(color, static_cast<unsigned char>(std::clamp(amount, 0.0f, 1.0f) * static_cast<float>(color.a)));
}

float delayed(float reveal_t, float delay) {
    return tween::ease_out_cubic(tween::remap_clamped(reveal_t, delay, 1.0f));
}

Rectangle reveal_rect(Rectangle rect, float t, float offset_x = 0.0f, float offset_y = 22.0f) {
    return {rect.x + offset_x * (1.0f - t), rect.y + offset_y * (1.0f - t), rect.width, rect.height};
}

result_title_layout result_title_layout_for(float reveal_t) {
    const float t = delayed(reveal_t, 0.0f);
    return {
        {36.0f, 31.0f, 6.0f, 56.0f},
        reveal_rect(kTitleRect, t, -12.0f, 0.0f),
        {378.0f, 77.0f, 435.0f, 2.0f},
    };
}

std::array<Rectangle, kActionButtons.size()> action_button_rects() {
    std::array<Rectangle, kActionButtons.size()> rects{};
    ui::hstack_widths(kActionBarRect, kActionButtonWidths, kActionButtonGap, rects);
    return rects;
}

std::array<action_button_layout, kActionButtons.size()> action_button_layouts() {
    const std::array<Rectangle, kActionButtons.size()> rects = action_button_rects();
    std::array<action_button_layout, kActionButtons.size()> layouts{};
    for (std::size_t i = 0; i < kActionButtons.size(); ++i) {
        layouts[i] = {
            .value = kActionButtons[i].value,
            .rect = rects[i],
            .label = kActionButtons[i].label,
        };
    }
    return layouts;
}

void draw_panel_rect(Rectangle rect, float reveal_t, Color fill, Color border) {
    const Rectangle visual = reveal_rect(rect, reveal_t);
    ui::surface(visual, alpha(fill, 0.92f * reveal_t), alpha(border, reveal_t), 1.5f);
}

void draw_accent_rule(Rectangle rect, Color color, float reveal_t) {
    ui::accent_bar({rect.x, rect.y, rect.width * reveal_t, rect.height}, alpha(color, reveal_t));
}

Rectangle dynamic_badge_rect(float x, float y, const std::string& text, int font_size,
                             float horizontal_padding, float min_width, float max_width) {
    const float width = std::clamp(
        measured_width(text, font_size) + horizontal_padding * 2.0f,
        min_width,
        max_width);
    return {x, y, width, 38.0f};
}

struct line_segment {
    Vector2 start;
    Vector2 end;
};

struct song_info_layout {
    Rectangle title;
    Rectangle artist;
    Rectangle difficulty;
    Rectangle level;
    Rectangle key_mode;
    Rectangle rc;
};

song_info_layout song_info_layout_for(Rectangle visual, const chart_meta& chart, const std::string& level_text) {
    constexpr float kBadgePadding = 30.0f;
    constexpr float kBadgeGap = 12.0f;
    const Rectangle difficulty = dynamic_badge_rect(visual.x, visual.y + 116.0f, chart.difficulty, 18,
                                                    kBadgePadding, 112.0f, 240.0f);
    const Rectangle level = dynamic_badge_rect(difficulty.x + difficulty.width + kBadgeGap, visual.y + 116.0f,
                                               level_text, 18, kBadgePadding * 0.5f, 92.0f, 150.0f);
    return {
        .title = text_rect(visual.x, visual.y, visual.width, 44),
        .artist = text_rect(visual.x, visual.y + 64.0f, visual.width, 26),
        .difficulty = difficulty,
        .level = level,
        .key_mode = {level.x + level.width + kBadgeGap + 8.0f, visual.y + 116.0f, 62.0f, 38.0f},
        .rc = {difficulty.x, visual.y + 264.0f, difficulty.width + level.width + 180.0f, 42.0f},
    };
}

std::array<song_text_descriptor, 2> song_text_descriptors_for(const song_info_layout& layout,
                                                              const song_data& song,
                                                              float reveal_t) {
    return {{
        {layout.title, song.meta.title.c_str(), 44, alpha(g_theme->text, reveal_t)},
        {layout.artist, song.meta.artist.c_str(), 26, alpha(g_theme->text_secondary, reveal_t)},
    }};
}

std::array<song_badge_descriptor, 2> song_badge_descriptors_for(const song_info_layout& layout,
                                                                const chart_meta& chart,
                                                                const std::string& level_text) {
    return {{
        {layout.difficulty, chart.difficulty, g_theme->error, g_theme->border_active, 14.0f},
        {layout.level, level_text, g_theme->text_secondary, g_theme->border, 10.0f},
    }};
}

std::array<song_value_descriptor, 2> song_value_descriptors_for(const song_info_layout& layout,
                                                                int key_count,
                                                                float rc_value,
                                                                float reveal_t) {
    return {{
        {layout.key_mode, key_mode_label(key_count), 18, alpha(g_theme->accent, reveal_t), ui::text_align::left},
        {layout.rc, TextFormat("RC %.0f", rc_value), 32, alpha(g_theme->text, reveal_t), ui::text_align::left},
    }};
}

struct result_status_layout {
    const char* label = nullptr;
    int font_size = 24;
    Rectangle rect{};
    Color color{};
};

result_status_layout status_layout_for(const result_data& result) {
    if (result.failed) {
        return {"FAILED", 30, {902.0f, 494.0f, 160.0f, 44.0f}, g_theme->error};
    }
    if (result.is_all_perfect) {
        return {"ALL PERFECT", 24, {858.0f, 494.0f, 206.0f, 44.0f}, g_theme->all_perfect};
    }
    if (result.is_full_combo) {
        return {"FULL COMBO", 24, {858.0f, 494.0f, 206.0f, 44.0f}, g_theme->full_combo};
    }
    return {};
}

struct rank_score_layout {
    Rectangle rank;
    line_segment divider;
    Rectangle score;
    Rectangle score_rule;
    result_status_layout status;
};

struct rank_score_text_descriptor {
    Rectangle rect;
    std::string text;
    int font_size;
    Color color;
    ui::text_align align;
    int min_font_size;
    bool fit;
};

rank_score_layout rank_score_layout_for(const result_data& result, float reveal_t) {
    return {
        .rank = reveal_rect(kRankRect, reveal_t, -28.0f, 0.0f),
        .divider = {{448.0f, 536.0f}, {448.0f, 688.0f}},
        .score = reveal_rect(kScoreRect, reveal_t, 18.0f, 0.0f),
        .score_rule = {476.0f, 700.0f, 590.0f, 3.0f},
        .status = status_layout_for(result),
    };
}

std::array<rank_score_text_descriptor, 2> rank_score_text_descriptors_for(const rank_score_layout& layout,
                                                                          const result_data& result,
                                                                          float reveal_t) {
    const Color rcolor = rank_color(result.clear_rank);
    return {{
        {layout.rank,
         rank_label(result.clear_rank),
         result.clear_rank == rank::aa ? 118 : 150,
         alpha(rcolor, reveal_t),
         ui::text_align::center,
         66,
         false},
        {layout.score,
         format_score(result.score),
         96,
         alpha(g_theme->text, reveal_t),
         ui::text_align::right,
         66,
         true},
    }};
}

struct metric_layout {
    Rectangle label;
    Rectangle accent;
    Rectangle value;
};

metric_layout metric_layout_for(Rectangle visual) {
    return {
        .label = {visual.x + 26.0f, visual.y + 24.0f, visual.width - 52.0f, 26.0f},
        .accent = {visual.x + 26.0f, visual.y + 56.0f, 48.0f, 2.0f},
        .value = {visual.x + 20.0f, visual.y + 78.0f, visual.width - 40.0f, 54.0f},
    };
}

struct offset_count_layout {
    Rectangle label;
    Rectangle accent;
    Rectangle value;
};

struct offset_count_descriptor {
    offset_count_layout layout;
    const char* label;
    std::string value;
    Color tone;
};

offset_count_layout offset_count_layout_for(Rectangle column) {
    return {
        .label = {column.x, column.y, column.width, 26.0f},
        .accent = {column.x, column.y + 32.0f, 56.0f, 2.0f},
        .value = {column.x, column.y + 50.0f, column.width, 42.0f},
    };
}

struct offsets_layout {
    offset_count_layout fast;
    offset_count_layout slow;
    line_segment divider;
};

offsets_layout offsets_layout_for(Rectangle visual) {
    const ui::rect_pair columns = ui::split_columns(
        {visual.x + 26.0f, visual.y + 24.0f, visual.width - 52.0f, 92.0f},
        112.0f,
        36.0f);
    return {
        .fast = offset_count_layout_for(columns.first),
        .slow = offset_count_layout_for(columns.second),
        .divider = {{visual.x + visual.width * 0.5f, visual.y + 38.0f},
                    {visual.x + visual.width * 0.5f, visual.y + 114.0f}},
    };
}

std::array<offset_count_descriptor, 2> offset_count_descriptors_for(const offsets_layout& layout,
                                                                    const result_data& result) {
    return {{
        {layout.fast, "FAST", std::to_string(result.fast_count), g_theme->fast},
        {layout.slow, "SLOW", std::to_string(result.slow_count), g_theme->slow},
    }};
}

struct judgement_row_layout {
    Rectangle label;
    Rectangle count;
    line_segment divider;
};

struct judgement_definition {
    const char* label;
    std::size_t count_index;
};

constexpr std::array<judgement_definition, 5> kJudgements = {{
    {"Perfect", 0},
    {"Great", 1},
    {"Good", 2},
    {"Bad", 3},
    {"Miss", 4},
}};

judgement_row_layout judgement_row_layout_for(Rectangle row_rect) {
    const ui::rect_pair columns = ui::split_columns(row_rect, 190.0f, 20.0f);
    return {
        .label = columns.first,
        .count = columns.second,
        .divider = {{row_rect.x, row_rect.y + row_rect.height},
                    {row_rect.x + row_rect.width, row_rect.y + row_rect.height}},
    };
}

std::array<judgement_row_layout, kJudgements.size()> judgement_layouts_for(Rectangle visual) {
    const Rectangle content = ui::inset(visual, ui::edge_insets::symmetric(30.0f, 28.0f));
    std::array<Rectangle, kJudgements.size()> rows{};
    ui::vstack_fill(content, 8.0f, rows);

    std::array<judgement_row_layout, kJudgements.size()> layouts{};
    for (std::size_t i = 0; i < layouts.size(); ++i) {
        layouts[i] = judgement_row_layout_for(rows[i]);
    }
    return layouts;
}

Color judgement_color(std::size_t index) {
    switch (index) {
        case 0: return g_theme->judge_perfect;
        case 1: return g_theme->judge_great;
        case 2: return g_theme->judge_good;
        case 3: return g_theme->judge_bad;
        case 4: return g_theme->judge_miss;
        default: return g_theme->text_muted;
    }
}

void draw_badge(Rectangle rect, const std::string& text, Color text_color, Color border_color, float reveal_t,
                float horizontal_inset = 12.0f) {
    ui::surface(rect, alpha(g_theme->row_soft, reveal_t), alpha(border_color, reveal_t), 1.5f);
    draw_fit_text(text, 18, ui::inset(rect, ui::edge_insets::symmetric(0.0f, horizontal_inset)),
                  alpha(text_color, reveal_t), ui::text_align::center, 14);
}

void draw_background(const model& data) {
    draw_scene_background(*g_theme);
    const Color veil = with_alpha(BLACK, g_theme == &kDarkTheme ? 76 : 34);
    ui::vertical_gradient(kScreenRect, with_alpha(g_theme->panel, 120), with_alpha(g_theme->bg_alt, 245));
    ui::backdrop({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)}, veil);

    if (data.jacket_texture != nullptr && data.jacket_texture->id != 0) {
        ui::draw_texture(*data.jacket_texture, kScreenRect, with_alpha(WHITE, 24));
        ui::backdrop({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                     with_alpha(g_theme->bg, 210));
    }

    for (int x = 0; x < kScreenWidth; x += 32) {
        ui::draw_line_ex({static_cast<float>(x), 0.0f}, {static_cast<float>(x), static_cast<float>(kScreenHeight)},
                         1.0f, with_alpha(g_theme->border_light, 24));
    }
    for (int y = 0; y < kScreenHeight; y += 32) {
        ui::draw_line_ex({0.0f, static_cast<float>(y)}, {static_cast<float>(kScreenWidth), static_cast<float>(y)},
                         1.0f, with_alpha(g_theme->border_light, 18));
    }
}

void draw_title(float reveal_t) {
    const float t = delayed(reveal_t, 0.0f);
    const result_title_layout layout = result_title_layout_for(reveal_t);
    ui::accent_bar(layout.accent, alpha(g_theme->fast, t));
    ui::draw_text_in_rect("RESULT", 58, layout.title, alpha(g_theme->text, t), ui::text_align::left);
    draw_accent_rule(layout.rule, with_alpha(g_theme->border, 160), t);
}

void draw_jacket(const Texture2D* jacket, float reveal_t) {
    draw_panel_rect(kJacketRect, reveal_t, with_alpha(g_theme->panel, 210), g_theme->border_image);
    const Rectangle visual = reveal_rect(kJacketRect, reveal_t);
    const Rectangle inner = ui::inset(visual, 7.0f);
    if (jacket != nullptr && jacket->id != 0) {
        ui::draw_texture(*jacket, inner, alpha(WHITE, reveal_t));
    } else {
        ui::placeholder(inner, "NO JACKET", {
            .font_size = 26,
            .draw_border = false,
            .fill = alpha(g_theme->section, 0.9f * reveal_t),
            .text_color = alpha(g_theme->text_muted, reveal_t),
            .custom_colors = true,
        });
    }
}

void draw_song_info(const song_data& song, const chart_meta& chart, int key_count, float rc_value,
                    double now, float reveal_t) {
    const Rectangle visual = reveal_rect(kSongInfoRect, reveal_t, 18.0f, 0.0f);
    const std::string level_text = TextFormat("Lv. %.1f", chart.level);
    const song_info_layout layout = song_info_layout_for(visual, chart, level_text);
    for (const song_text_descriptor& text : song_text_descriptors_for(layout, song, reveal_t)) {
        draw_marquee_text(text.text, text.rect, text.font_size, text.color, now);
    }
    for (const song_badge_descriptor& badge : song_badge_descriptors_for(layout, chart, level_text)) {
        draw_badge(badge.rect, badge.text, badge.text_color, badge.border_color, reveal_t, badge.horizontal_inset);
    }
    for (const song_value_descriptor& value : song_value_descriptors_for(layout, key_count, rc_value, reveal_t)) {
        ui::draw_text_in_rect(value.text.c_str(), value.font_size, value.rect, value.color, value.align);
    }
}

void draw_result_status(const result_status_layout& status, float reveal_t) {
    if (status.label == nullptr) {
        return;
    }
    ui::draw_text_in_rect(status.label, status.font_size, status.rect,
                          alpha(status.color, reveal_t));
}

void draw_rank_score(const result_data& result, float reveal_t) {
    const rank_score_layout layout = rank_score_layout_for(result, reveal_t);
    const Color rcolor = rank_color(result.clear_rank);
    for (const rank_score_text_descriptor& text : rank_score_text_descriptors_for(layout, result, reveal_t)) {
        if (text.fit) {
            draw_fit_text(text.text, text.font_size, text.rect, text.color, text.align, text.min_font_size);
        } else {
            ui::draw_text_in_rect(text.text.c_str(), text.font_size, text.rect, text.color, text.align);
        }
    }
    ui::draw_line_ex(layout.divider.start, layout.divider.end, 1.5f, alpha(g_theme->border, reveal_t));

    draw_accent_rule(layout.score_rule, rcolor, reveal_t);

    draw_result_status(layout.status, reveal_t);
}

void draw_chip(Rectangle rect, const char* label, const std::string& value, Color tone, float reveal_t) {
    ui::surface(rect,
                alpha(lerp_color(g_theme->panel, tone, 0.12f), 0.92f * reveal_t),
                alpha(lerp_color(g_theme->border, tone, 0.4f), reveal_t),
                1.5f);
    const ui::rect_pair chip_rows = ui::split_rows(
        ui::inset(rect, ui::edge_insets{10.0f, 24.0f, 12.0f, 24.0f}),
        24.0f,
        4.0f);
    ui::draw_text_in_rect(label, 18, chip_rows.first,
                          alpha(tone, reveal_t), ui::text_align::left);
    draw_fit_text(value, 23, chip_rows.second, alpha(g_theme->text, reveal_t), ui::text_align::left, 16);
}

void draw_achievements(const model& data, float reveal_t) {
    Rectangle chips[3];
    ui::hstack_fill(kAchievementRect, 18.0f, chips);

    std::string local_value = "NOT UPDATED";
    Color local_color = g_theme->text_muted;
    if (data.local_submit != nullptr && data.local_submit->success) {
        if (data.local_submit->best_updated) {
            local_color = g_theme->rank_c;
            const int delta = data.local_submit->previous_best.has_value()
                ? data.result.score - data.local_submit->previous_best->score
                : data.result.score;
            local_value = format_delta(delta);
        } else if (data.local_submit->previous_best.has_value()) {
            local_value = localized_text("BEST") + " " + format_score(data.local_submit->previous_best->score);
        }
    }

    std::string online_value = "PENDING";
    Color online_color = g_theme->accent;
    if (!data.online_status_message.empty() && data.online_status_is_error) {
        online_value = "FAILED";
        online_color = g_theme->error;
    } else if (data.online_submit != nullptr && data.online_submit->success) {
        if (data.online_submit->updated) {
            online_value = "UPDATED";
            online_color = g_theme->success;
        } else {
            online_value = "NOT UPDATED";
            online_color = g_theme->text_muted;
        }
    } else if (data.online_submit == nullptr && data.online_status_message.empty()) {
        online_value = "OFFLINE";
        online_color = g_theme->text_muted;
    }

    std::string rank_value = "--";
    if (data.online_submit != nullptr && data.online_submit->entry.has_value()) {
        const int current = data.online_submit->entry->placement;
        if (data.online_submit->previous_entry.has_value() && data.online_submit->previous_entry->placement > 0) {
            rank_value = "#" + std::to_string(data.online_submit->previous_entry->placement) +
                         " -> #" + std::to_string(current);
        } else {
            rank_value = "#" + std::to_string(current);
        }
    } else if (data.local_submit != nullptr && data.local_submit->submitted_entry.has_value() &&
               data.local_submit->submitted_entry->placement > 0) {
        rank_value = localized_text("LOCAL") + " #" + std::to_string(data.local_submit->submitted_entry->placement);
    }

    const std::array<achievement_chip_descriptor, 3> achievements{{
        {reveal_rect(chips[0], reveal_t, -10.0f, 14.0f), "LOCAL BEST", local_value, local_color},
        {reveal_rect(chips[1], reveal_t, 0.0f, 14.0f), "ONLINE BEST", online_value, online_color},
        {reveal_rect(chips[2], reveal_t, 10.0f, 14.0f), "RANK", rank_value, g_theme->fast},
    }};
    for (const achievement_chip_descriptor& achievement : achievements) {
        draw_chip(achievement.rect, achievement.label, achievement.value, achievement.tone, reveal_t);
    }
}

void draw_metric(Rectangle rect, const char* label, const std::string& value, Color tone, float reveal_t) {
    draw_panel_rect(rect, reveal_t, with_alpha(g_theme->panel, 218), lerp_color(g_theme->border, tone, 0.25f));
    const Rectangle visual = reveal_rect(rect, reveal_t);
    const metric_layout layout = metric_layout_for(visual);
    ui::draw_text_in_rect(label, 20, layout.label,
                          alpha(g_theme->text, reveal_t), ui::text_align::left);
    draw_accent_rule(layout.accent, tone, reveal_t);
    draw_fit_text(value, 42, layout.value,
                  alpha(g_theme->text, reveal_t), ui::text_align::center, 24);
}

void draw_offsets(const result_data& result, float reveal_t) {
    draw_panel_rect(kOffsetRect, reveal_t, with_alpha(g_theme->panel, 218), g_theme->border);
    const Rectangle visual = reveal_rect(kOffsetRect, reveal_t);
    const offsets_layout layout = offsets_layout_for(visual);
    for (const offset_count_descriptor& count : offset_count_descriptors_for(layout, result)) {
        ui::draw_text_in_rect(count.label, 20, count.layout.label, alpha(count.tone, reveal_t),
                              ui::text_align::left);
        draw_accent_rule(count.layout.accent, count.tone, reveal_t);
        ui::draw_text_in_rect(count.value.c_str(), 38, count.layout.value,
                              alpha(count.tone, reveal_t), ui::text_align::left);
    }
    ui::draw_line_ex(layout.divider.start, layout.divider.end, 1.0f, alpha(g_theme->border, reveal_t));
}

void draw_judgements(const result_data& result, float reveal_t) {
    draw_panel_rect(kJudgementRect, reveal_t, with_alpha(g_theme->panel, 218), g_theme->border);
    const std::array<judgement_row_layout, kJudgements.size()> layouts =
        judgement_layouts_for(reveal_rect(kJudgementRect, reveal_t));
    for (std::size_t i = 0; i < kJudgements.size(); ++i) {
        const judgement_definition& judgement = kJudgements[i];
        const judgement_row_layout& layout = layouts[i];
        ui::draw_text_in_rect(judgement.label, 23, layout.label,
                              alpha(judgement_color(judgement.count_index), reveal_t), ui::text_align::left);
        ui::draw_text_in_rect(std::to_string(result.judge_counts[judgement.count_index]).c_str(), 26,
                              layout.count,
                              alpha(g_theme->text, reveal_t), ui::text_align::right);
        if (i + 1 < kJudgements.size()) {
            ui::draw_line_ex(layout.divider.start,
                             layout.divider.end,
                             1.0f, alpha(g_theme->border_light, reveal_t * 0.75f));
        }
    }
}

std::array<result_metric_descriptor, 3> result_metric_descriptors_for(const result_data& result,
                                                                      float metric_t,
                                                                      float detail_t) {
    return {{
        {kMetricAccuracyRect, "Accuracy", TextFormat("%.2f%%", result.accuracy), g_theme->fast, metric_t},
        {kMetricComboRect, "Max Combo", std::to_string(result.max_combo), g_theme->accent, metric_t},
        {kMetricOffsetRect, "Avg Offset", TextFormat("%+.1fms", result.avg_offset), g_theme->text_secondary, detail_t},
    }};
}

action draw_action_button(const action_button_layout& button, float reveal_t) {
    const Rectangle rect = button.rect;
    const Rectangle visual = reveal_rect(rect, reveal_t, 0.0f, 18.0f);
    const unsigned char row_alpha =
        static_cast<unsigned char>(static_cast<float>(g_theme->row_soft_alpha) * reveal_t);
    const unsigned char hover_alpha =
        static_cast<unsigned char>(static_cast<float>(g_theme->row_soft_hover_alpha) * reveal_t);
    const std::string localized = localized_text(button.label);
    const int font_size = fit_font_size(button.label, 30, visual.width - 52.0f, 20);
    if (ui::button(visual, localized.c_str(), {
        .font_size = font_size,
        .border_width = 1.5f,
        .bg = with_alpha(g_theme->row_soft, row_alpha),
        .bg_hover = with_alpha(g_theme->row_soft_hover, hover_alpha),
        .text_color = alpha(g_theme->text, reveal_t),
        .custom_colors = true,
        .border_color = alpha(g_theme->border, reveal_t),
        .custom_border = true,
    }).clicked) {
        return button.value;
    }
    return action::none;
}

}  // namespace

action shortcut_action() {
    if (ui::is_enter_pressed() || ui::is_escape_pressed()) {
        return action::song_select;
    }
    if (ui::is_key_pressed(KEY_R)) {
        return action::retry;
    }
    return action::none;
}

draw_result draw(const model& data) {
    draw_result result;
    const float title_t = delayed(data.reveal_t, 0.00f);
    const float main_t = delayed(data.reveal_t, 0.05f);
    const float metric_t = delayed(data.reveal_t, 0.14f);
    const float detail_t = delayed(data.reveal_t, 0.22f);
    const float action_t = delayed(data.reveal_t, 0.32f);

    draw_background(data);
    draw_title(title_t);
    draw_panel_rect(kMainPanelRect, main_t, with_alpha(g_theme->panel, 210), g_theme->border);
    draw_jacket(data.jacket_texture, main_t);
    draw_song_info(data.song, data.chart, data.key_count, data.result.rc_value, data.now, main_t);
    draw_rank_score(data.result, main_t);
    draw_achievements(data, delayed(data.reveal_t, 0.20f));

    for (const result_metric_descriptor& metric : result_metric_descriptors_for(data.result, metric_t, detail_t)) {
        draw_metric(metric.rect, metric.label, metric.value, metric.tone, metric.reveal_t);
    }
    draw_offsets(data.result, detail_t);
    draw_judgements(data.result, detail_t);

    const std::array<action_button_layout, kActionButtons.size()> action_layouts = action_button_layouts();
    for (const action_button_layout& button : action_layouts) {
        const action clicked = draw_action_button(button, action_t);
        if (clicked != action::none) {
            result.requested_action = clicked;
        }
    }
    return result;
}

}  // namespace result_scene_view
