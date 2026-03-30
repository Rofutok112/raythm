#include "result_scene.h"

#include <algorithm>
#include <memory>

#include "play_scene.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "theme.h"
#include "virtual_screen.h"

namespace {

constexpr Rectangle kMainPanel = {24.0f, 24.0f, 1232.0f, 672.0f};
constexpr Rectangle kSongInfoRect = {48.0f, 48.0f, 580.0f, 108.0f};
constexpr Rectangle kRankRect = {660.0f, 48.0f, 200.0f, 168.0f};
constexpr Rectangle kScoreRect = {48.0f, 180.0f, 580.0f, 108.0f};
constexpr Rectangle kJudgeRect = {48.0f, 312.0f, 580.0f, 260.0f};
constexpr Rectangle kStatsRect = {660.0f, 240.0f, 572.0f, 332.0f};

const char* rank_label(rank r) {
    switch (r) {
        case rank::ss: return "SS";
        case rank::s:  return "S";
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

}  // namespace

result_scene::result_scene(scene_manager& manager, result_data result, bool ranking_enabled,
                           song_data song, std::string chart_path, chart_meta chart, int key_count)
    : scene(manager), result_(result), ranking_enabled_(ranking_enabled),
      song_(std::move(song)), chart_path_(std::move(chart_path)), chart_(std::move(chart)), key_count_(key_count) {
}

void result_scene::update(float dt) {
    fade_in_timer_ = std::max(0.0f, fade_in_timer_ - dt);

    if (IsKeyPressed(KEY_ENTER)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
    } else if (IsKeyPressed(KEY_R)) {
        manager_.change_scene(std::make_unique<play_scene>(manager_, song_, chart_path_, key_count_));
    }
}

void result_scene::draw() {
    const auto& t = *g_theme;
    virtual_screen::begin();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);

    // メインパネル
    DrawRectangleRec(kMainPanel, t.panel);
    DrawRectangleLinesEx(kMainPanel, 2.0f, t.border);

    // 楽曲情報
    DrawRectangleRec(kSongInfoRect, t.section);
    DrawRectangleLinesEx(kSongInfoRect, 1.5f, t.border_light);
    const float song_info_max_w = kSongInfoRect.width - 32.0f;
    const double now = GetTime();
    {
        const int sy = static_cast<int>(kSongInfoRect.y + (kSongInfoRect.height - 82) * 0.5f);
        const int lx = static_cast<int>(kSongInfoRect.x + 16);
        draw_marquee_text(song_.meta.title.c_str(), lx, sy, 32, t.text, song_info_max_w, now);
        draw_marquee_text(song_.meta.artist.c_str(), lx, sy + 38, 22, t.text_dim, song_info_max_w, now);
        const char* chart_label = TextFormat("%s %s Lv.%d", key_mode_label(chart_.key_count),
                                             chart_.difficulty.c_str(), chart_.level);
        const int chart_label_w = MeasureText(chart_label, 20);
        DrawText(chart_label, lx, sy + 66, 20, t.accent);
        DrawText(chart_.chart_author.c_str(), lx + chart_label_w + 16, sy + 66, 20, t.text_muted);
    }

    // ランク表示
    DrawRectangleRec(kRankRect, t.section);
    DrawRectangleLinesEx(kRankRect, 1.5f, t.border_light);
    const char* rlabel = rank_label(result_.clear_rank);
    const Color rcolor = rank_color(result_.clear_rank);
    const int rank_text_w = MeasureText(rlabel, 96);
    DrawText(rlabel,
             static_cast<int>(kRankRect.x + kRankRect.width * 0.5f) - rank_text_w / 2,
             static_cast<int>(kRankRect.y + kRankRect.height * 0.5f) - 48,
             96, rcolor);

    // 称号（Full Combo / All Perfect）
    if (result_.is_all_perfect) {
        DrawText("ALL PERFECT",
                 static_cast<int>(kRankRect.x + kRankRect.width * 0.5f) - MeasureText("ALL PERFECT", 20) / 2,
                 static_cast<int>(kRankRect.y + kRankRect.height - 28), 20, t.all_perfect);
    } else if (result_.is_full_combo) {
        DrawText("FULL COMBO",
                 static_cast<int>(kRankRect.x + kRankRect.width * 0.5f) - MeasureText("FULL COMBO", 20) / 2,
                 static_cast<int>(kRankRect.y + kRankRect.height - 28), 20, t.full_combo);
    }

    // FAILED 表示
    if (result_.failed) {
        DrawText("FAILED",
                 static_cast<int>(kRankRect.x + kRankRect.width + 20),
                 static_cast<int>(kRankRect.y + 20), 40, t.error);
    }

    // スコア・精度（フレーム中央に配置）
    DrawRectangleRec(kScoreRect, t.section);
    DrawRectangleLinesEx(kScoreRect, 1.5f, t.border_light);
    {
        const int content_h = 36 + 8 + 36;  // 2行 + 間隔
        const int start_y = static_cast<int>(kScoreRect.y + (kScoreRect.height - content_h) * 0.5f);
        const int lx = static_cast<int>(kScoreRect.x + 16);
        DrawText("SCORE", lx, start_y + 6, 20, t.text_dim);
        DrawText(TextFormat("%07d", result_.score), lx + 100, start_y, 36, t.text);
        DrawText("ACCURACY", lx, start_y + 8 + 36 + 6, 20, t.text_dim);
        DrawText(TextFormat("%.2f%%", result_.accuracy), lx + 140, start_y + 8 + 36, 36, t.text_secondary);
    }

    // 判定内訳（フレーム中央に配置）
    DrawRectangleRec(kJudgeRect, t.section);
    DrawRectangleLinesEx(kJudgeRect, 1.5f, t.border_light);

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
        constexpr int judge_row_h = 42;
        constexpr int judge_count = 5;
        const int judge_content_h = judge_count * judge_row_h - (judge_row_h - 30);
        int jy = static_cast<int>(kJudgeRect.y + (kJudgeRect.height - judge_content_h) * 0.5f);
        const int jx = static_cast<int>(kJudgeRect.x + 16);
        for (const auto& row : rows) {
            DrawRectangle(jx, jy - 2, 160, 30, row.color);
            DrawText(row.label, jx + 8, jy + 2, 22, t.text);
            DrawText(TextFormat("%d", row.count), jx + 176, jy + 2, 22, t.text);
            jy += judge_row_h;
        }
    }

    // 統計情報（フレーム中央に配置）
    DrawRectangleRec(kStatsRect, t.section);
    DrawRectangleLinesEx(kStatsRect, 1.5f, t.border_light);

    {
        constexpr int stat_row_h = 40;
        constexpr int stat_count = 5;
        const int stat_content_h = stat_count * stat_row_h;
        int sy = static_cast<int>(kStatsRect.y + (kStatsRect.height - stat_content_h - 40) * 0.5f);
        const int sx = static_cast<int>(kStatsRect.x + 24);
        const int sv = static_cast<int>(kStatsRect.x + 200);

        DrawText("Max Combo", sx, sy, 24, t.text_dim);
        DrawText(TextFormat("%d", result_.max_combo), sv, sy, 24, t.text);
        sy += stat_row_h;

        DrawText("Avg Offset", sx, sy, 24, t.text_dim);
        DrawText(TextFormat("%.1f ms", result_.avg_offset), sv, sy, 24, t.text);
        sy += stat_row_h;

        DrawText("Fast", sx, sy, 24, t.text_dim);
        DrawText(TextFormat("%d", result_.fast_count), sv, sy, 24, t.fast);
        sy += stat_row_h;

        DrawText("Slow", sx, sy, 24, t.text_dim);
        DrawText(TextFormat("%d", result_.slow_count), sv, sy, 24, t.slow);
        sy += stat_row_h;

        DrawText("Ranking", sx, sy, 24, t.text_dim);
        DrawText(ranking_enabled_ ? "Eligible" : "Disabled", sv, sy, 24,
                 ranking_enabled_ ? t.success : t.error);
        sy += stat_row_h + 10;

        // 操作案内
        DrawText("ENTER: Song Select    R: Retry", sx, sy, 20, t.text_hint);
    }

    // フェードイン（暗い状態から明るくなる）
    if (fade_in_timer_ > 0.0f) {
        const unsigned char alpha = static_cast<unsigned char>(std::min(fade_in_timer_, 1.0f) * 0.65f * 255.0f);
        DrawRectangle(0, 0, kScreenWidth, kScreenHeight, {0, 0, 0, alpha});
    }

    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
