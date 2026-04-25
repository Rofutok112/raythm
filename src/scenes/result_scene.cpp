#include "result_scene.h"

#include <algorithm>
#include <memory>
#include <thread>

#include "play_scene.h"
#include "raylib.h"
#include "ranking_service.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui/ui_font.h"
#include "virtual_screen.h"

namespace {

constexpr float kOuterMargin = 36.0f;
constexpr float kPanelPadding = 36.0f;
constexpr float kLeftColumnWidth = 870.0f;
constexpr float kColumnGap = 48.0f;
constexpr float kSongInfoHeight = 162.0f;
constexpr float kScoreTop = 198.0f;
constexpr float kScoreHeight = 162.0f;
constexpr float kJudgeTop = 396.0f;
constexpr float kJudgeHeight = 390.0f;
constexpr float kRankWidth = 300.0f;
constexpr float kRankHeight = 252.0f;
constexpr float kStatsTop = 288.0f;
constexpr float kStatsHeight = 498.0f;
constexpr float kContentInset = 24.0f;
constexpr float kRankTitleHeight = 180.0f;
constexpr float kRankBadgeBottom = 60.0f;
constexpr float kRankBadgeHeight = 36.0f;
constexpr float kStatsBottomReserved = 102.0f;
constexpr float kStatsHintBottom = 96.0f;
constexpr float kOnlineStatusBottom = 54.0f;
constexpr float kSongTextPaddingX = 24.0f;
constexpr float kSongMetaGap = 24.0f;
constexpr float kFailedOffset = 30.0f;
constexpr float kScoreLabelWidth = 150.0f;
constexpr float kAccuracyLabelWidth = 210.0f;
constexpr float kJudgeRowExtraHeight = 12.0f;
constexpr float kJudgeRowGap = 6.0f;
constexpr float kJudgeBadgeWidth = 240.0f;
constexpr float kJudgeCountX = 264.0f;
constexpr float kStatRowGap = 12.0f;

constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kMainPanel = ui::inset(kScreenRect, kOuterMargin);
constexpr Rectangle kContentRect = ui::inset(kMainPanel, kPanelPadding);
constexpr Rectangle kLeftColRect = {kContentRect.x, kContentRect.y, kLeftColumnWidth, kContentRect.height};
constexpr Rectangle kRightColRect = {
    kContentRect.x + kLeftColumnWidth + kColumnGap,
    kContentRect.y,
    kContentRect.width - kLeftColumnWidth - kColumnGap,
    kContentRect.height
};
constexpr Rectangle kSongInfoRect = {kLeftColRect.x, kLeftColRect.y, kLeftColRect.width, kSongInfoHeight};
constexpr Rectangle kScoreRect = {kLeftColRect.x, kLeftColRect.y + kScoreTop, kLeftColRect.width, kScoreHeight};
constexpr Rectangle kJudgeRect = {kLeftColRect.x, kLeftColRect.y + kJudgeTop, kLeftColRect.width, kJudgeHeight};
constexpr Rectangle kRankRect = {kRightColRect.x, kRightColRect.y, kRankWidth, kRankHeight};
constexpr Rectangle kStatsRect = {kRightColRect.x, kRightColRect.y + kStatsTop, kRightColRect.width, kStatsHeight};
constexpr Rectangle kScoreContentRect = ui::inset(kScoreRect, kContentInset);
constexpr Rectangle kRankTitleRect = {kRankRect.x, kRankRect.y, kRankRect.width, kRankTitleHeight};
constexpr Rectangle kRankBadgeRect = {
    kRankRect.x, kRankRect.y + kRankRect.height - kRankBadgeBottom, kRankRect.width, kRankBadgeHeight
};
constexpr Rectangle kJudgeRowsRect = ui::inset(kJudgeRect, kContentInset);
constexpr Rectangle kStatsRowsRect = ui::inset(kStatsRect, ui::edge_insets{
    kContentInset, kContentInset, kStatsBottomReserved, kContentInset
});
constexpr Rectangle kStatsHintRect = {
    kStatsRect.x + kContentInset,
    kStatsRect.y + kStatsRect.height - kStatsHintBottom,
    kStatsRect.width - kContentInset * 2.0f,
    kContentInset
};
constexpr Rectangle kOnlineStatusRect = {
    kStatsRect.x + kContentInset,
    kStatsRect.y + kStatsRect.height - kOnlineStatusBottom,
    kStatsRect.width - kContentInset * 2.0f,
    kContentInset
};

const char* rank_label(rank r) {
    switch (r) {
        case rank::ss: return "SS";
        case rank::s:  return "S";
        case rank::aa: return "AA";
        case rank::a:  return "A";
        case rank::b:  return "B";
        case rank::c:  return "C";
        case rank::f:  return "F";
    }
    return "?";
}

Color rank_color(rank r) {
    switch (r) {
        case rank::ss: return g_theme->rank_ss;
        case rank::s:  return g_theme->rank_s;
        case rank::aa: return g_theme->rank_aa;
        case rank::a:  return g_theme->rank_a;
        case rank::b:  return g_theme->rank_b;
        case rank::c:  return g_theme->rank_c;
        case rank::f:  return g_theme->rank_f;
    }
    return g_theme->text_secondary;
}

const char* key_mode_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

Rectangle text_line_rect(float x, float y, float width, int font_size) {
    return {x, y, width, ui::text_layout_font_size(static_cast<float>(font_size))};
}

}  // namespace

result_scene::result_scene(scene_manager& manager, result_data result, bool ranking_enabled,
                           song_data song, std::string chart_path, chart_meta chart, int key_count)
    : scene(manager), result_(result), ranking_enabled_(ranking_enabled),
      song_(std::move(song)), chart_path_(std::move(chart_path)), chart_(std::move(chart)), key_count_(key_count) {
}

void result_scene::on_enter() {
    const ranking_service::local_submit_result local_result =
        ranking_service::submit_local_result_detailed(chart_, result_);

    if (ranking_enabled_) {
        if (ranking_service::should_attempt_online_submit(local_result)) {
            online_submit_status_message_ = "Submitting online ranking...";
            online_submit_status_is_error_ = false;

            online_submit_task_ = std::make_shared<online_submit_task_state>();
            std::shared_ptr<online_submit_task_state> task = online_submit_task_;
            const song_data song = song_;
            const std::string chart_path = chart_path_;
            const chart_meta chart = chart_;
            const std::string recorded_at = local_result.submitted_entry->recorded_at;
            const result_data result_payload = result_;
            std::thread([task, song, chart_path, chart, result_payload, recorded_at]() {
                ranking_service::online_submit_result submit_result =
                    ranking_service::submit_online_result(song, chart_path, chart, result_payload, recorded_at);
                {
                    std::scoped_lock lock(task->mutex);
                    task->result = std::move(submit_result);
                }
                task->done.store(true);
            }).detach();
        }
    }
}

void result_scene::return_to_song_select() const {
    song_select::recent_result_offset recent_result;
    recent_result.song_id = song_.meta.song_id;
    recent_result.chart_id = chart_.chart_id;
    recent_result.avg_offset_ms = result_.avg_offset;
    manager_.change_scene(song_select::make_seamless_song_select_scene(
        manager_, song_.meta.song_id, chart_.chart_id, recent_result));
}

void result_scene::update(float dt) {
    fade_in_.update(dt);
    poll_online_submit();

    if (IsKeyPressed(KEY_ENTER)) {
        return_to_song_select();
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        return_to_song_select();
    } else if (IsKeyPressed(KEY_R)) {
        manager_.change_scene(std::make_unique<play_scene>(manager_, song_, chart_path_, key_count_));
    }
}

void result_scene::poll_online_submit() {
    if (!online_submit_task_ || online_submit_consumed_ || !online_submit_task_->done.load()) {
        return;
    }

    online_submit_consumed_ = true;
    ranking_service::online_submit_result result;
    {
        std::scoped_lock lock(online_submit_task_->mutex);
        result = online_submit_task_->result;
    }

    if (!result.message.empty()) {
        online_submit_status_message_ = result.message;
        online_submit_status_is_error_ = !result.success;
    } else if (result.success && result.updated) {
        online_submit_status_message_ = "Online ranking updated.";
        online_submit_status_is_error_ = false;
    } else if (result.success) {
        online_submit_status_message_ = "Submitted score did not beat your online best.";
        online_submit_status_is_error_ = false;
    } else {
        online_submit_status_message_.clear();
        online_submit_status_is_error_ = false;
    }
}

void result_scene::draw() {
    const auto& t = *g_theme;
    virtual_screen::begin_ui();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);

    ui::draw_panel(kMainPanel);

    // 楽曲情報
    ui::draw_section(kSongInfoRect);
    const float song_info_max_w = kSongInfoRect.width - kSongTextPaddingX * 2.0f;
    const double now = GetTime();
    {
        const float title_h = ui::text_layout_font_size(32.0f);
        const float artist_h = ui::text_layout_font_size(22.0f);
        const float meta_h = ui::text_layout_font_size(20.0f);
        constexpr float kTitleArtistGap = 9.0f;
        constexpr float kArtistMetaGap = 12.0f;
        const float block_h = title_h + kTitleArtistGap + artist_h + kArtistMetaGap + meta_h;
        const float sy = kSongInfoRect.y + (kSongInfoRect.height - block_h) * 0.5f;
        const float lx = kSongInfoRect.x + kSongTextPaddingX;
        draw_marquee_text(song_.meta.title.c_str(), text_line_rect(lx, sy, song_info_max_w, 32),
                          32, t.text, now);
        draw_marquee_text(song_.meta.artist.c_str(),
                          text_line_rect(lx, sy + title_h + kTitleArtistGap, song_info_max_w, 22),
                          22, t.text_dim, now);
        const char* chart_label = TextFormat("%s %s Lv.%.1f", key_mode_label(chart_.key_count),
                                             chart_.difficulty.c_str(), chart_.level);
        const float chart_label_w = ui::measure_text_size(chart_label, 20.0f, 0.0f).x;
        const float meta_y = sy + title_h + kTitleArtistGap + artist_h + kArtistMetaGap;
        ui::draw_text_f(chart_label, lx, meta_y, 20, t.accent);
        ui::draw_text_f(chart_.chart_author.c_str(), lx + chart_label_w + kSongMetaGap,
                        meta_y, 20, t.text_muted);
    }

    // ランク表示
    ui::draw_section(kRankRect);
    const char* rlabel = rank_label(result_.clear_rank);
    const Color rcolor = rank_color(result_.clear_rank);
    ui::draw_text_in_rect(rlabel, 96, kRankTitleRect, rcolor);

    // 称号（Full Combo / All Perfect）
    if (result_.is_all_perfect) {
        ui::draw_text_in_rect("ALL PERFECT", 20, kRankBadgeRect, t.all_perfect);
    } else if (result_.is_full_combo) {
        ui::draw_text_in_rect("FULL COMBO", 20, kRankBadgeRect, t.full_combo);
    }

    // FAILED 表示
    if (result_.failed) {
        ui::draw_text_f("FAILED", kRankRect.x + kRankRect.width + kFailedOffset,
                        kRankRect.y + kFailedOffset, 40, t.error);
    }

    // スコア・精度（フレーム中央に配置）
    ui::draw_section(kScoreRect);
    {
        Rectangle score_rows[2];
        const float score_row_h = ui::text_layout_font_size(36.0f);
        const float score_gap = std::max(0.0f, (kScoreContentRect.height - score_row_h * 2.0f) * 0.5f);
        ui::vstack(kScoreContentRect, score_row_h, score_gap, score_rows);

        const ui::rect_pair score_columns = ui::split_columns(score_rows[0], kScoreLabelWidth);
        ui::draw_text_in_rect("SCORE", 20, score_columns.first, t.text_dim, ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%07d", result_.score), 36, score_columns.second, t.text, ui::text_align::left);

        const ui::rect_pair accuracy_columns = ui::split_columns(score_rows[1], kAccuracyLabelWidth);
        ui::draw_text_in_rect("ACCURACY", 20, accuracy_columns.first, t.text_dim, ui::text_align::left);
        ui::draw_text_in_rect(TextFormat("%.2f%%", result_.accuracy), 36, accuracy_columns.second,
                              t.text_secondary, ui::text_align::left);
    }

    // 判定内訳（フレーム中央に配置）
    ui::draw_section(kJudgeRect);

    struct judge_row {
        const char* label;
        int count;
        Color color;
    };
    const judge_row rows[] = {
        {"PERFECT", result_.judge_counts[0], t.judge_perfect},
        {"GREAT",   result_.judge_counts[1], t.judge_great},
        {"GOOD",    result_.judge_counts[2], t.judge_good},
        {"BAD",     result_.judge_counts[3], t.judge_bad},
        {"MISS",    result_.judge_counts[4], t.judge_miss},
    };

    {
        Rectangle judge_rects[5];
        const float judge_row_h = ui::text_layout_font_size(22.0f) + kJudgeRowExtraHeight;
        const float judge_gap = kJudgeRowGap;
        ui::vstack(kJudgeRowsRect, judge_row_h, judge_gap, judge_rects);
        for (int i = 0; i < 5; ++i) {
            const auto& row = rows[i];
            const ui::row_state row_state = ui::draw_row(judge_rects[i], t.section, t.section, t.border_light, 0.0f);
            const Rectangle content = ui::inset(row_state.visual, ui::edge_insets::symmetric(0.0f, 0.0f));
            const Rectangle badge_rect = {
                content.x,
                content.y + (content.height - ui::text_layout_font_size(22.0f)) * 0.5f,
                kJudgeBadgeWidth,
                ui::text_layout_font_size(22.0f)
            };
            const Rectangle count_rect = {
                content.x + kJudgeCountX,
                content.y,
                content.width - kJudgeCountX,
                content.height
            };
            ui::draw_rect_f(badge_rect, row.color);
            ui::draw_text_in_rect(row.label, 22, badge_rect, t.text);
            ui::draw_text_in_rect(TextFormat("%d", row.count), 22, count_rect, t.text, ui::text_align::left);
        }
    }

    // 統計情報（フレーム中央に配置）
    ui::draw_section(kStatsRect);

    {
        Rectangle stat_rows[4];
        const float stat_row_h = ui::text_layout_font_size(24.0f);
        ui::vstack(kStatsRowsRect, stat_row_h, kStatRowGap, stat_rows);
        ui::draw_label_value(stat_rows[0], "Max Combo", TextFormat("%d", result_.max_combo),
                             24, t.text_dim, t.text);
        ui::draw_label_value(stat_rows[1], "Avg Offset", TextFormat("%.1f ms", result_.avg_offset),
                             24, t.text_dim, t.text);
        ui::draw_label_value(stat_rows[2], "Fast", TextFormat("%d", result_.fast_count),
                             24, t.text_dim, t.fast);
        ui::draw_label_value(stat_rows[3], "Slow", TextFormat("%d", result_.slow_count),
                             24, t.text_dim, t.slow);
        ui::draw_text_in_rect("ENTER: Song Select    R: Retry    Use AUTO APPLY there", 20,
                              {kStatsHintRect.x, kStatsHintRect.y, kStatsHintRect.width,
                               ui::text_layout_font_size(20.0f)},
                              t.text_hint, ui::text_align::left);
        if (!online_submit_status_message_.empty()) {
            ui::draw_text_in_rect(online_submit_status_message_.c_str(), 18,
                                  {kOnlineStatusRect.x, kOnlineStatusRect.y, kOnlineStatusRect.width,
                                   ui::text_layout_font_size(18.0f)},
                                  online_submit_status_is_error_ ? t.error : t.text_secondary,
                                  ui::text_align::left);
        }
    }

    // フェードイン（暗い状態から明るくなる）
    fade_in_.draw();

    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
