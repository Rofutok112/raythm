#include "multiplayer_result/multiplayer_result_detail_view.h"

#include <algorithm>
#include <string>

#include "theme.h"
#include "ui_draw.h"

namespace multiplayer_result::detail_view {
namespace {

constexpr Rectangle kSelectedMetricGridRect{430.0f, 376.0f, 720.0f, 260.0f};
constexpr ui::rect_pair kSelectedMetricRows = ui::split_rows(kSelectedMetricGridRect, 118.0f, 24.0f);
constexpr ui::rect_pair kSelectedMetricTopColumns = ui::split_columns(kSelectedMetricRows.first, 344.0f, 32.0f);
constexpr ui::rect_pair kSelectedMetricBottomColumns = ui::split_columns(kSelectedMetricRows.second, 344.0f, 32.0f);
constexpr Rectangle kSelectedAccuracyRect = kSelectedMetricTopColumns.first;
constexpr Rectangle kSelectedComboRect = kSelectedMetricTopColumns.second;
constexpr Rectangle kSelectedOffsetRect = kSelectedMetricBottomColumns.first;
constexpr Rectangle kSelectedFastSlowRect = kSelectedMetricBottomColumns.second;
constexpr Rectangle kSelectedJudgementRect{kSelectedMetricGridRect.x, 674.0f, kSelectedMetricGridRect.width, 330.0f};
constexpr Rectangle kSelectedSummaryRect{430.0f, 127.0f, 720.0f, 205.0f};
constexpr Rectangle kSelectedNameRect{kSelectedSummaryRect.x, kSelectedSummaryRect.y, kSelectedSummaryRect.width, 34.0f};
constexpr Rectangle kSelectedSummaryBodyRect{kSelectedSummaryRect.x, 142.0f, kSelectedSummaryRect.width, 190.0f};
constexpr ui::rect_pair kSelectedSummaryColumns = ui::split_columns(kSelectedSummaryBodyRect, 254.0f, 52.0f);
constexpr Rectangle kSelectedRankRect = kSelectedSummaryColumns.first;
constexpr Rectangle kSelectedScoreRect{kSelectedSummaryColumns.second.x,
                                       kSelectedSummaryBodyRect.y + 10.0f,
                                       kSelectedSummaryColumns.second.width,
                                       96.0f};
constexpr Rectangle kSelectedScoreDividerRect{kSelectedScoreRect.x, 270.0f, kSelectedScoreRect.width, 3.0f};
constexpr Rectangle kSelectedStateRowRect{kSelectedScoreRect.x, 284.0f, kSelectedScoreRect.width, 42.0f};
constexpr ui::rect_pair kSelectedStateColumns = ui::split_columns(kSelectedStateRowRect, 190.0f, 16.0f);
constexpr Rectangle kSelectedRcRect = kSelectedStateColumns.first;
constexpr Rectangle kSelectedClearRect = kSelectedStateColumns.second;
constexpr Rectangle kSelectedSummaryDividerRect{kSelectedRankRect.x + kSelectedRankRect.width + 20.0f,
                                                160.0f,
                                                0.0f,
                                                162.0f};

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
    return "F";
}

Color result_rank_color(rank value) {
    switch (value) {
        case rank::ss: return g_theme->rank_ss;
        case rank::s: return g_theme->rank_s;
        case rank::aa: return g_theme->rank_aa;
        case rank::a: return g_theme->rank_a;
        case rank::b: return g_theme->rank_b;
        case rank::c: return g_theme->rank_c;
        case rank::f: return g_theme->rank_f;
    }
    return g_theme->rank_f;
}

std::string format_score(int score) {
    std::string value = std::to_string(std::max(0, score));
    for (int i = static_cast<int>(value.size()) - 3; i > 0; i -= 3) {
        value.insert(static_cast<size_t>(i), ",");
    }
    return value;
}

void draw_result_panel(Rectangle rect, Color border = {0, 0, 0, 0}) {
    const Color resolved_border = border.a > 0 ? border : g_theme->border;
    ui::surface(rect, with_alpha(g_theme->panel, 214), resolved_border, 1.5f);
}

void draw_compact_metric(Rectangle rect, const char* label, const char* value, Color value_color) {
    ui::surface(rect, g_theme->section, g_theme->border_light, 1.5f);
    const ui::rect_pair rows = ui::split_rows(
        ui::inset(rect, ui::edge_insets{12.0f, 16.0f, 28.0f, 16.0f}),
        24.0f,
        12.0f);
    ui::draw_text_in_rect(label, 18, rows.first, g_theme->text_muted, ui::text_align::left);
    ui::draw_text_in_rect(value, 32, rows.second, value_color, ui::text_align::left);
}

void draw_fast_slow_panel(Rectangle rect, bool has_details, int fast_count, int slow_count) {
    ui::surface(rect, g_theme->section, g_theme->border_light, 1.5f);
    const float column_widths[] = {112.0f, 112.0f};
    Rectangle columns[2]{};
    ui::hstack_widths(ui::inset(rect, ui::edge_insets{22.0f, 26.0f, 22.0f, 26.0f}),
                      column_widths,
                      58.0f,
                      columns);
    const Rectangle left = columns[0];
    const Rectangle right = columns[1];
    const Color fast_color = has_details ? g_theme->fast : g_theme->text_muted;
    const Color slow_color = has_details ? g_theme->slow : g_theme->text_muted;
    ui::draw_text_in_rect("FAST", 18, {left.x, left.y, left.width, 24.0f}, fast_color, ui::text_align::left);
    ui::divider({left.x, left.y + 30.0f, 50.0f, 2.0f}, fast_color);
    ui::draw_text_in_rect(has_details ? std::to_string(fast_count).c_str() : "--", 34,
                          {left.x, left.y + 42.0f, left.width, 38.0f}, fast_color, ui::text_align::left);
    ui::draw_line_ex({rect.x + rect.width * 0.5f, rect.y + 24.0f},
                     {rect.x + rect.width * 0.5f, rect.y + rect.height - 24.0f},
                     1.0f, g_theme->border);
    ui::draw_text_in_rect("SLOW", 18, {right.x, right.y, right.width, 24.0f}, slow_color, ui::text_align::left);
    ui::divider({right.x, right.y + 30.0f, 50.0f, 2.0f}, slow_color);
    ui::draw_text_in_rect(has_details ? std::to_string(slow_count).c_str() : "--", 34,
                          {right.x, right.y + 42.0f, right.width, 38.0f}, slow_color, ui::text_align::left);
}

void draw_judgement_breakdown(Rectangle rect, const play_multiplayer_score_row& score, bool has_details) {
    draw_result_panel(rect);
    const Rectangle judge_content = ui::inset(rect, ui::edge_insets::symmetric(24.0f, 22.0f));
    const char* judge_labels[5] = {"Perfect", "Great", "Good", "Bad", "Miss"};
    const Color judge_colors[5] = {
        g_theme->judge_perfect,
        g_theme->judge_great,
        g_theme->judge_good,
        g_theme->judge_bad,
        g_theme->judge_miss,
    };
    Rectangle judge_rows[5]{};
    ui::vstack_fill(judge_content, 0.0f, judge_rows);
    for (int i = 0; i < 5; ++i) {
        const Rectangle row_rect = judge_rows[i];
        const ui::rect_pair row_columns = ui::split_columns(row_rect, 220.0f, 20.0f);
        ui::draw_text_in_rect(judge_labels[i], 26, row_columns.first,
                              has_details ? judge_colors[i] : g_theme->text_muted,
                              ui::text_align::left);
        const std::string count_text = has_details
            ? std::to_string(score.judge_counts[static_cast<size_t>(i)])
            : "--";
        ui::draw_text_in_rect(count_text.c_str(), 34, row_columns.second,
                              g_theme->text, ui::text_align::right);
        if (i < 4) {
            ui::draw_line_ex({row_rect.x, row_rect.y + row_rect.height},
                             {row_rect.x + row_rect.width, row_rect.y + row_rect.height},
                             1.0f,
                             g_theme->border_light);
        }
    }
}

}  // namespace

void draw(const play_multiplayer_score_row& score, bool has_details) {
    const rank selected_rank = has_details
        ? score.clear_rank
        : (score.failed ? rank::f : compute_rank(score.accuracy, false));
    const Color selected_rank_color = result_rank_color(selected_rank);

    ui::draw_text_in_rect(score.display_name.c_str(), 26,
                          kSelectedNameRect, g_theme->text_secondary, ui::text_align::left);
    ui::draw_text_in_rect(rank_label(selected_rank), selected_rank == rank::aa ? 108 : 138,
                          kSelectedRankRect, selected_rank_color, ui::text_align::center);
    ui::draw_line_ex({kSelectedSummaryDividerRect.x, kSelectedSummaryDividerRect.y},
                     {kSelectedSummaryDividerRect.x,
                      kSelectedSummaryDividerRect.y + kSelectedSummaryDividerRect.height},
                     1.5f,
                     g_theme->border);

    ui::draw_text_in_rect(format_score(score.score).c_str(), 82,
                          kSelectedScoreRect,
                          g_theme->text, ui::text_align::right);
    ui::divider(kSelectedScoreDividerRect, selected_rank_color);
    ui::draw_text_in_rect(has_details ? TextFormat("RC %.0f", score.rc_value) : "RC --", 28,
                          kSelectedRcRect, g_theme->text_secondary, ui::text_align::left);
    const char* clear_label = score.failed ? "FAILED" :
        (has_details && score.is_all_perfect ? "ALL PERFECT" :
         (has_details && score.is_full_combo ? "FULL COMBO" : "CLEAR"));
    const Color clear_color = score.failed ? g_theme->error :
        (has_details && score.is_all_perfect ? g_theme->all_perfect :
         (has_details && score.is_full_combo ? g_theme->full_combo : g_theme->success));
    ui::draw_text_in_rect(clear_label, 28, kSelectedClearRect, clear_color, ui::text_align::right);

    draw_compact_metric(kSelectedAccuracyRect,
                        "Accuracy", TextFormat("%.2f%%", score.accuracy), g_theme->fast);
    draw_compact_metric(kSelectedComboRect,
                        "Max Combo", std::to_string(score.combo).c_str(), g_theme->accent);
    draw_compact_metric(kSelectedOffsetRect,
                        "Avg Offset", has_details ? TextFormat("%+.1fms", score.avg_offset) : "--",
                        g_theme->text_secondary);
    draw_fast_slow_panel(kSelectedFastSlowRect, has_details, score.fast_count, score.slow_count);

    draw_judgement_breakdown(kSelectedJudgementRect, score, has_details);
}

}  // namespace multiplayer_result::detail_view
