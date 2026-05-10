#include "result/result_scene_view.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/tween.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui/ui_font.h"

namespace result_scene_view {
namespace {

constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kContentRect = {36.0f, 108.0f, 1848.0f, 786.0f};
constexpr Rectangle kTitleRect = {36.0f, 28.0f, 520.0f, 62.0f};
constexpr Rectangle kMainPanelRect = {36.0f, 108.0f, 980.0f, 704.0f};
constexpr Rectangle kJacketRect = {60.0f, 132.0f, 320.0f, 320.0f};
constexpr Rectangle kSongInfoRect = {404.0f, 146.0f, 560.0f, 178.0f};
constexpr Rectangle kRankRect = {60.0f, 478.0f, 382.0f, 184.0f};
constexpr Rectangle kScoreRect = {482.0f, 498.0f, 468.0f, 142.0f};
constexpr Rectangle kAchievementRect = {60.0f, 690.0f, 930.0f, 92.0f};
constexpr Rectangle kMetricAccuracyRect = {1040.0f, 108.0f, 304.0f, 272.0f};
constexpr Rectangle kMetricComboRect = {1356.0f, 108.0f, 260.0f, 272.0f};
constexpr Rectangle kMetricGaugeRect = {1628.0f, 108.0f, 256.0f, 272.0f};
constexpr Rectangle kOffsetRect = {1040.0f, 392.0f, 382.0f, 196.0f};
constexpr Rectangle kJudgementRect = {1434.0f, 392.0f, 450.0f, 420.0f};
constexpr Rectangle kStatusRect = {612.0f, 836.0f, 696.0f, 70.0f};
constexpr Rectangle kRetryRect = {50.0f, 928.0f, 520.0f, 92.0f};
constexpr Rectangle kSongSelectRect = {626.0f, 928.0f, 668.0f, 92.0f};
constexpr Rectangle kReplayRect = {1350.0f, 928.0f, 520.0f, 92.0f};

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

Rectangle text_rect(float x, float y, float width, int font_size) {
    return {x, y, width, ui::text_layout_font_size(static_cast<float>(font_size))};
}

int fit_font_size(const std::string& text, int desired_size, float max_width, int min_size = 14) {
    int size = desired_size;
    while (size > min_size && ui::measure_text_size(text.c_str(), static_cast<float>(size), 0.0f).x > max_width) {
        --size;
    }
    return size;
}

void draw_fit_text(const std::string& text, int desired_size, Rectangle rect, Color color,
                   ui::text_align align = ui::text_align::left, int min_size = 14) {
    const int size = fit_font_size(text, desired_size, rect.width, min_size);
    ui::draw_text_in_rect(text.c_str(), size, rect, color, align);
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

void draw_panel_rect(Rectangle rect, float reveal_t, Color fill, Color border) {
    const Rectangle visual = reveal_rect(rect, reveal_t);
    ui::draw_rect_f(visual, alpha(fill, 0.92f * reveal_t));
    ui::draw_rect_lines(visual, 1.5f, alpha(border, reveal_t));
}

void draw_accent_rule(Rectangle rect, Color color, float reveal_t) {
    ui::draw_rect_f({rect.x, rect.y, rect.width * reveal_t, rect.height}, alpha(color, reveal_t));
}

void draw_background(const model& data) {
    draw_scene_background(*g_theme);
    const Color veil = with_alpha(BLACK, g_theme == &kDarkTheme ? 76 : 34);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, with_alpha(g_theme->panel, 120), with_alpha(g_theme->bg_alt, 245));
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, veil);

    if (data.jacket_texture != nullptr && data.jacket_texture->id != 0) {
        const Rectangle source = {
            0.0f,
            0.0f,
            static_cast<float>(data.jacket_texture->width),
            static_cast<float>(data.jacket_texture->height)
        };
        DrawTexturePro(*data.jacket_texture, source, kScreenRect, {0.0f, 0.0f}, 0.0f, with_alpha(WHITE, 24));
        DrawRectangle(0, 0, kScreenWidth, kScreenHeight, with_alpha(g_theme->bg, 210));
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
    ui::draw_rect_f({36.0f, 31.0f, 6.0f, 56.0f}, alpha(g_theme->fast, t));
    ui::draw_text_in_rect("RESULT", 58, reveal_rect(kTitleRect, t, -12.0f, 0.0f), alpha(g_theme->text, t),
                          ui::text_align::left);
    draw_accent_rule({378.0f, 77.0f, 435.0f, 2.0f}, with_alpha(g_theme->border, 160), t);
}

void draw_jacket(const Texture2D* jacket, float reveal_t) {
    draw_panel_rect(kJacketRect, reveal_t, with_alpha(g_theme->panel, 210), g_theme->border_image);
    const Rectangle visual = reveal_rect(kJacketRect, reveal_t);
    const Rectangle inner = ui::inset(visual, 7.0f);
    if (jacket != nullptr && jacket->id != 0) {
        DrawTexturePro(*jacket,
                       {0.0f, 0.0f, static_cast<float>(jacket->width), static_cast<float>(jacket->height)},
                       inner, {0.0f, 0.0f}, 0.0f, alpha(WHITE, reveal_t));
    } else {
        ui::draw_rect_f(inner, alpha(g_theme->section, 0.9f * reveal_t));
        ui::draw_text_in_rect("NO JACKET", 26, inner, alpha(g_theme->text_muted, reveal_t));
    }
}

void draw_song_info(const song_data& song, const chart_meta& chart, int key_count, double now, float reveal_t) {
    const Rectangle visual = reveal_rect(kSongInfoRect, reveal_t, 18.0f, 0.0f);
    draw_marquee_text(song.meta.title.c_str(), text_rect(visual.x, visual.y, visual.width, 44), 44,
                      alpha(g_theme->text, reveal_t), now);
    draw_marquee_text(song.meta.artist.c_str(), text_rect(visual.x, visual.y + 64.0f, visual.width, 26), 26,
                      alpha(g_theme->text_secondary, reveal_t), now);

    const Rectangle difficulty = {visual.x, visual.y + 116.0f, 128.0f, 38.0f};
    const Rectangle level = {visual.x + 140.0f, visual.y + 116.0f, 92.0f, 38.0f};
    ui::draw_rect_f(difficulty, alpha(g_theme->row_soft, reveal_t));
    ui::draw_rect_lines(difficulty, 1.5f, alpha(g_theme->border_active, reveal_t));
    ui::draw_text_in_rect(chart.difficulty.c_str(), 18, difficulty, alpha(g_theme->error, reveal_t));
    ui::draw_rect_f(level, alpha(g_theme->row_soft, reveal_t));
    ui::draw_rect_lines(level, 1.5f, alpha(g_theme->border, reveal_t));
    ui::draw_text_in_rect(TextFormat("Lv. %.1f", chart.level), 18, level, alpha(g_theme->text_secondary, reveal_t));
    ui::draw_text_in_rect(key_mode_label(key_count), 18, {visual.x + 248.0f, visual.y + 116.0f, 62.0f, 38.0f},
                          alpha(g_theme->accent, reveal_t));
}

void draw_rank_score(const result_data& result, float reveal_t) {
    const Rectangle rank_visual = reveal_rect(kRankRect, reveal_t, -28.0f, 0.0f);
    const Color rcolor = rank_color(result.clear_rank);
    ui::draw_text_in_rect(rank_label(result.clear_rank), result.clear_rank == rank::aa ? 118 : 150,
                          rank_visual, alpha(rcolor, reveal_t));
    ui::draw_line_ex({448.0f, 496.0f}, {448.0f, 644.0f}, 1.5f, alpha(g_theme->border, reveal_t));

    const Rectangle score_visual = reveal_rect(kScoreRect, reveal_t, 18.0f, 0.0f);
    ui::draw_text_in_rect(format_score(result.score).c_str(), 78, score_visual, alpha(g_theme->text, reveal_t),
                          ui::text_align::left);
    draw_accent_rule({482.0f, 658.0f, 468.0f, 3.0f}, rcolor, reveal_t);

    if (result.failed) {
        ui::draw_text_in_rect("FAILED", 30, {780.0f, 454.0f, 160.0f, 44.0f}, alpha(g_theme->error, reveal_t));
    } else if (result.is_all_perfect) {
        ui::draw_text_in_rect("ALL PERFECT", 24, {744.0f, 454.0f, 206.0f, 44.0f}, alpha(g_theme->all_perfect, reveal_t));
    } else if (result.is_full_combo) {
        ui::draw_text_in_rect("FULL COMBO", 24, {744.0f, 454.0f, 206.0f, 44.0f}, alpha(g_theme->full_combo, reveal_t));
    }
}

void draw_chip(Rectangle rect, const char* label, const std::string& value, Color tone, float reveal_t) {
    ui::draw_rect_f(rect, alpha(lerp_color(g_theme->panel, tone, 0.12f), 0.92f * reveal_t));
    ui::draw_rect_lines(rect, 1.5f, alpha(lerp_color(g_theme->border, tone, 0.4f), reveal_t));
    ui::draw_text_in_rect(label, 20, {rect.x + 24.0f, rect.y + 14.0f, rect.width - 48.0f, 26.0f},
                          alpha(tone, reveal_t), ui::text_align::left);
    draw_fit_text(value, 25, {rect.x + 24.0f, rect.y + 42.0f, rect.width - 48.0f, 30.0f},
                  alpha(g_theme->text, reveal_t), ui::text_align::left, 16);
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
            local_value = "BEST " + format_score(data.local_submit->previous_best->score);
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
        rank_value = "LOCAL #" + std::to_string(data.local_submit->submitted_entry->placement);
    }

    draw_chip(reveal_rect(chips[0], reveal_t, -10.0f, 14.0f), "LOCAL BEST", local_value, local_color, reveal_t);
    draw_chip(reveal_rect(chips[1], reveal_t, 0.0f, 14.0f), "ONLINE BEST", online_value, online_color, reveal_t);
    draw_chip(reveal_rect(chips[2], reveal_t, 10.0f, 14.0f), "RANK", rank_value, g_theme->fast, reveal_t);
}

void draw_metric(Rectangle rect, const char* label, const std::string& value, Color tone, float reveal_t) {
    draw_panel_rect(rect, reveal_t, with_alpha(g_theme->panel, 218), lerp_color(g_theme->border, tone, 0.25f));
    const Rectangle visual = reveal_rect(rect, reveal_t);
    ui::draw_text_in_rect(label, 25, {visual.x + 34.0f, visual.y + 32.0f, visual.width - 68.0f, 32.0f},
                          alpha(g_theme->text, reveal_t), ui::text_align::left);
    draw_accent_rule({visual.x + 34.0f, visual.y + 72.0f, 58.0f, 3.0f}, tone, reveal_t);
    draw_fit_text(value, 52, {visual.x + 22.0f, visual.y + 108.0f, visual.width - 44.0f, 76.0f},
                  alpha(g_theme->text, reveal_t), ui::text_align::center, 34);
}

void draw_gauge_bar(const result_data& result, float reveal_t) {
    const Rectangle visual = reveal_rect(kMetricGaugeRect, reveal_t);
    const Rectangle bar = {visual.x + 32.0f, visual.y + 204.0f, visual.width - 64.0f, 28.0f};
    const float ratio = std::clamp(result.gauge_value / 100.0f, 0.0f, 1.0f);
    ui::draw_progress_bar(bar, ratio, alpha(g_theme->section, reveal_t),
                          alpha(result.gauge_value >= 70.0f ? g_theme->success : g_theme->health_low, reveal_t),
                          alpha(g_theme->border, reveal_t), 1.5f, 4.0f);
}

void draw_offsets(const result_data& result, float reveal_t) {
    draw_panel_rect(kOffsetRect, reveal_t, with_alpha(g_theme->panel, 218), g_theme->border);
    const Rectangle visual = reveal_rect(kOffsetRect, reveal_t);
    const Rectangle left = {visual.x + 34.0f, visual.y + 38.0f, 132.0f, 118.0f};
    const Rectangle right = {visual.x + 224.0f, visual.y + 38.0f, 132.0f, 118.0f};
    ui::draw_text_in_rect("FAST", 25, {left.x, left.y, left.width, 32.0f}, alpha(g_theme->fast, reveal_t),
                          ui::text_align::left);
    draw_accent_rule({left.x, left.y + 40.0f, 82.0f, 2.0f}, g_theme->fast, reveal_t);
    ui::draw_text_in_rect(std::to_string(result.fast_count).c_str(), 46, {left.x, left.y + 62.0f, left.width, 52.0f},
                          alpha(g_theme->fast, reveal_t), ui::text_align::left);
    ui::draw_line_ex({visual.x + visual.width * 0.5f, visual.y + 64.0f},
                     {visual.x + visual.width * 0.5f, visual.y + 134.0f}, 1.0f, alpha(g_theme->border, reveal_t));
    ui::draw_text_in_rect("SLOW", 25, {right.x, right.y, right.width, 32.0f}, alpha(g_theme->slow, reveal_t),
                          ui::text_align::left);
    draw_accent_rule({right.x, right.y + 40.0f, 82.0f, 2.0f}, g_theme->slow, reveal_t);
    ui::draw_text_in_rect(std::to_string(result.slow_count).c_str(), 46,
                          {right.x, right.y + 62.0f, right.width, 52.0f}, alpha(g_theme->slow, reveal_t),
                          ui::text_align::left);
}

void draw_judgements(const result_data& result, float reveal_t) {
    draw_panel_rect(kJudgementRect, reveal_t, with_alpha(g_theme->panel, 218), g_theme->border);
    const Rectangle content = ui::inset(reveal_rect(kJudgementRect, reveal_t), ui::edge_insets::symmetric(34.0f, 34.0f));
    struct row {
        const char* label;
        int count;
        Color color;
    };
    const row rows[] = {
        {"Perfect", result.judge_counts[0], g_theme->judge_perfect},
        {"Great", result.judge_counts[1], g_theme->judge_great},
        {"Good", result.judge_counts[2], g_theme->judge_good},
        {"Bad", result.judge_counts[3], g_theme->judge_bad},
        {"Miss", result.judge_counts[4], g_theme->judge_miss},
    };
    Rectangle rects[5];
    ui::vstack_fill(content, 8.0f, rects);
    for (int i = 0; i < 5; ++i) {
        ui::draw_text_in_rect(rows[i].label, 28, {rects[i].x, rects[i].y, 190.0f, rects[i].height},
                              alpha(rows[i].color, reveal_t), ui::text_align::left);
        ui::draw_text_in_rect(std::to_string(rows[i].count).c_str(), 31,
                              {rects[i].x + 210.0f, rects[i].y, rects[i].width - 210.0f, rects[i].height},
                              alpha(g_theme->text, reveal_t), ui::text_align::right);
        if (i < 4) {
            ui::draw_line_ex({rects[i].x, rects[i].y + rects[i].height},
                             {rects[i].x + rects[i].width, rects[i].y + rects[i].height},
                             1.0f, alpha(g_theme->border_light, reveal_t * 0.75f));
        }
    }
}

void draw_status(const model& data, float reveal_t) {
    std::string message = data.online_status_message;
    Color tone = data.online_status_is_error ? g_theme->error : g_theme->success;
    if (message.empty()) {
        if (data.online_submit != nullptr && data.online_submit->success && data.online_submit->updated) {
            message = "Ranking Updated";
            tone = g_theme->success;
        } else if (data.online_submit != nullptr && data.online_submit->success) {
            message = "Submitted - Not Updated";
            tone = g_theme->text_secondary;
        } else if (data.local_submit != nullptr && data.local_submit->best_updated) {
            message = "Local Best Updated";
            tone = g_theme->rank_c;
        } else {
            message = "Result Saved Locally";
            tone = g_theme->text_secondary;
        }
    }

    const Rectangle visual = reveal_rect(kStatusRect, reveal_t, 0.0f, 16.0f);
    ui::draw_rect_f(visual, alpha(lerp_color(g_theme->panel, tone, 0.16f), 0.92f * reveal_t));
    ui::draw_rect_lines(visual, 1.5f, alpha(lerp_color(g_theme->border, tone, 0.48f), reveal_t));
    ui::draw_text_in_rect(message.c_str(), 30, visual, alpha(tone, reveal_t));
}

void draw_action_button(Rectangle rect, const char* icon, const char* label, float reveal_t) {
    const Rectangle visual = reveal_rect(rect, reveal_t, 0.0f, 18.0f);
    const bool hovered = CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), visual);
    const bool pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const Rectangle pressed_visual = pressed ? ui::inset(visual, 1.5f) : visual;
    const unsigned char row_alpha = static_cast<unsigned char>(
        static_cast<float>(hovered ? g_theme->row_soft_hover_alpha : g_theme->row_soft_alpha) * reveal_t);
    const Color fill = with_alpha(hovered ? g_theme->row_soft_hover : g_theme->row_soft, row_alpha);
    ui::draw_rect_f(pressed_visual, fill);
    ui::draw_rect_lines(pressed_visual, 1.5f, alpha(g_theme->border, reveal_t));
    const std::string button_label = std::string(icon) + "  " + label;
    draw_fit_text(button_label, 30,
                  {pressed_visual.x + 26.0f, pressed_visual.y, pressed_visual.width - 52.0f, pressed_visual.height},
                  alpha(g_theme->text, reveal_t), ui::text_align::center, 20);
}

}  // namespace

action hit_test_action(Vector2 point) {
    if (CheckCollisionPointRec(point, kRetryRect)) {
        return action::retry;
    }
    if (CheckCollisionPointRec(point, kSongSelectRect)) {
        return action::song_select;
    }
    if (CheckCollisionPointRec(point, kReplayRect)) {
        return action::replay;
    }
    return action::none;
}

void draw(const model& data) {
    const float title_t = delayed(data.reveal_t, 0.00f);
    const float main_t = delayed(data.reveal_t, 0.05f);
    const float metric_t = delayed(data.reveal_t, 0.14f);
    const float detail_t = delayed(data.reveal_t, 0.22f);
    const float action_t = delayed(data.reveal_t, 0.32f);

    draw_background(data);
    draw_title(title_t);
    draw_panel_rect(kMainPanelRect, main_t, with_alpha(g_theme->panel, 210), g_theme->border);
    draw_jacket(data.jacket_texture, main_t);
    draw_song_info(data.song, data.chart, data.key_count, data.now, main_t);
    draw_rank_score(data.result, main_t);
    draw_achievements(data, delayed(data.reveal_t, 0.20f));

    draw_metric(kMetricAccuracyRect, "Accuracy", TextFormat("%.2f%%", data.result.accuracy), g_theme->fast, metric_t);
    draw_metric(kMetricComboRect, "Max Combo", std::to_string(data.result.max_combo), g_theme->accent, metric_t);
    draw_metric(kMetricGaugeRect, "Gauge", TextFormat("%.0f%%", data.result.gauge_value), g_theme->success, metric_t);
    draw_gauge_bar(data.result, metric_t);
    draw_offsets(data.result, detail_t);
    draw_judgements(data.result, detail_t);
    draw_status(data, delayed(data.reveal_t, 0.30f));

    draw_action_button(kRetryRect, "R", "Retry", action_t);
    draw_action_button(kSongSelectRect, ">", "Song Select", action_t);
    draw_action_button(kReplayRect, "P", "Replay", action_t);
}

}  // namespace result_scene_view
