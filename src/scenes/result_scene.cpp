#include "result_scene.h"

#include <algorithm>
#include <memory>

#include "play_scene.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "virtual_screen.h"

namespace {

constexpr Rectangle kMainPanel = {24.0f, 24.0f, 1232.0f, 672.0f};
constexpr Rectangle kSongInfoRect = {48.0f, 48.0f, 580.0f, 108.0f};
constexpr Rectangle kRankRect = {660.0f, 48.0f, 200.0f, 168.0f};
constexpr Rectangle kScoreRect = {48.0f, 180.0f, 580.0f, 108.0f};
constexpr Rectangle kJudgeRect = {48.0f, 312.0f, 580.0f, 260.0f};
constexpr Rectangle kStatsRect = {660.0f, 240.0f, 572.0f, 332.0f};

Color rank_color(rank r) {
    switch (r) {
        case rank::ss: return {200, 50, 200, 255};  // マゼンタ
        case rank::s:  return {200, 50, 200, 255};
        case rank::a:  return {220, 50, 50, 255};    // 赤
        case rank::b:  return {50, 100, 220, 255};   // 青
        case rank::c:  return {210, 180, 30, 255};   // 黄色
        case rank::f:  return {120, 120, 120, 255};  // グレー
    }
    return DARKGRAY;
}

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
    virtual_screen::begin();
    ClearBackground(RAYWHITE);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, {255, 255, 255, 255}, {241, 243, 246, 255});

    // メインパネル
    DrawRectangleRec(kMainPanel, Color{248, 249, 251, 255});
    DrawRectangleLinesEx(kMainPanel, 2.0f, Color{206, 210, 218, 255});

    // 楽曲情報
    DrawRectangleRec(kSongInfoRect, Color{240, 242, 246, 255});
    DrawRectangleLinesEx(kSongInfoRect, 1.5f, Color{216, 220, 228, 255});
    const float song_info_max_w = kSongInfoRect.width - 32.0f;
    const double now = GetTime();
    {
        const int sy = static_cast<int>(kSongInfoRect.y + (kSongInfoRect.height - 82) * 0.5f);
        const int lx = static_cast<int>(kSongInfoRect.x + 16);
        draw_marquee_text(song_.meta.title.c_str(), lx, sy, 32, BLACK, song_info_max_w, now);
        draw_marquee_text(song_.meta.artist.c_str(), lx, sy + 38, 22, Color{100, 104, 112, 255}, song_info_max_w, now);
        const char* chart_label = TextFormat("%s %s Lv.%d", key_mode_label(chart_.key_count),
                                             chart_.difficulty.c_str(), chart_.level);
        const int chart_label_w = MeasureText(chart_label, 20);
        DrawText(chart_label, lx, sy + 66, 20, Color{124, 58, 237, 255});
        DrawText(chart_.chart_author.c_str(), lx + chart_label_w + 16, sy + 66, 20, Color{132, 136, 146, 255});
    }

    // ランク表示
    DrawRectangleRec(kRankRect, Color{240, 242, 246, 255});
    DrawRectangleLinesEx(kRankRect, 1.5f, Color{216, 220, 228, 255});
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
                 static_cast<int>(kRankRect.y + kRankRect.height - 28), 20, Color{200, 50, 200, 255});
    } else if (result_.is_full_combo) {
        DrawText("FULL COMBO",
                 static_cast<int>(kRankRect.x + kRankRect.width * 0.5f) - MeasureText("FULL COMBO", 20) / 2,
                 static_cast<int>(kRankRect.y + kRankRect.height - 28), 20, Color{14, 146, 108, 255});
    }

    // FAILED 表示
    if (result_.failed) {
        DrawText("FAILED",
                 static_cast<int>(kRankRect.x + kRankRect.width + 20),
                 static_cast<int>(kRankRect.y + 20), 40, Color{220, 38, 38, 255});
    }

    // スコア・精度（フレーム中央に配置）
    DrawRectangleRec(kScoreRect, Color{240, 242, 246, 255});
    DrawRectangleLinesEx(kScoreRect, 1.5f, Color{216, 220, 228, 255});
    {
        const int content_h = 36 + 8 + 36;  // 2行 + 間隔
        const int start_y = static_cast<int>(kScoreRect.y + (kScoreRect.height - content_h) * 0.5f);
        const int lx = static_cast<int>(kScoreRect.x + 16);
        DrawText("SCORE", lx, start_y + 6, 20, Color{120, 124, 132, 255});
        DrawText(TextFormat("%07d", result_.score), lx + 100, start_y, 36, BLACK);
        DrawText("ACCURACY", lx, start_y + 8 + 36 + 6, 20, Color{120, 124, 132, 255});
        DrawText(TextFormat("%.2f%%", result_.achievement), lx + 140, start_y + 8 + 36, 36, Color{50, 54, 62, 255});
    }

    // 判定内訳（フレーム中央に配置）
    DrawRectangleRec(kJudgeRect, Color{240, 242, 246, 255});
    DrawRectangleLinesEx(kJudgeRect, 1.5f, Color{216, 220, 228, 255});

    struct judge_row {
        const char* label;
        int count;
        Color color;
    };
    const judge_row rows[] = {
        {"PERFECT", result_.judge_counts[0], {239, 244, 255, 255}},
        {"GREAT",   result_.judge_counts[1], {123, 211, 255, 255}},
        {"GOOD",    result_.judge_counts[2], {141, 211, 173, 255}},
        {"BAD",     result_.judge_counts[3], {255, 190, 92, 255}},
        {"MISS",    result_.judge_counts[4], {255, 107, 107, 255}},
    };

    {
        constexpr int judge_row_h = 42;
        constexpr int judge_count = 5;
        const int judge_content_h = judge_count * judge_row_h - (judge_row_h - 30);
        int jy = static_cast<int>(kJudgeRect.y + (kJudgeRect.height - judge_content_h) * 0.5f);
        const int jx = static_cast<int>(kJudgeRect.x + 16);
        for (const auto& row : rows) {
            DrawRectangle(jx, jy - 2, 160, 30, row.color);
            DrawText(row.label, jx + 8, jy + 2, 22, Color{30, 34, 42, 255});
            DrawText(TextFormat("%d", row.count), jx + 176, jy + 2, 22, BLACK);
            jy += judge_row_h;
        }
    }

    // 統計情報（フレーム中央に配置）
    DrawRectangleRec(kStatsRect, Color{240, 242, 246, 255});
    DrawRectangleLinesEx(kStatsRect, 1.5f, Color{216, 220, 228, 255});

    {
        constexpr int stat_row_h = 40;
        constexpr int stat_count = 5;
        const int stat_content_h = stat_count * stat_row_h;
        int sy = static_cast<int>(kStatsRect.y + (kStatsRect.height - stat_content_h - 40) * 0.5f);
        const int sx = static_cast<int>(kStatsRect.x + 24);
        const int sv = static_cast<int>(kStatsRect.x + 200);
        const Color label_color = {120, 124, 132, 255};

        DrawText("Max Combo", sx, sy, 24, label_color);
        DrawText(TextFormat("%d", result_.max_combo), sv, sy, 24, BLACK);
        sy += stat_row_h;

        DrawText("Avg Offset", sx, sy, 24, label_color);
        DrawText(TextFormat("%.1f ms", result_.avg_offset), sv, sy, 24, BLACK);
        sy += stat_row_h;

        DrawText("Fast", sx, sy, 24, label_color);
        DrawText(TextFormat("%d", result_.fast_count), sv, sy, 24, Color{50, 120, 220, 255});
        sy += stat_row_h;

        DrawText("Slow", sx, sy, 24, label_color);
        DrawText(TextFormat("%d", result_.slow_count), sv, sy, 24, Color{220, 120, 50, 255});
        sy += stat_row_h;

        DrawText("Ranking", sx, sy, 24, label_color);
        DrawText(ranking_enabled_ ? "Eligible" : "Disabled", sv, sy, 24,
                 ranking_enabled_ ? Color{14, 146, 108, 255} : Color{220, 38, 38, 255});
        sy += stat_row_h + 10;

        // 操作案内
        DrawText("ENTER: Song Select    R: Retry", sx, sy, 20, Color{160, 164, 172, 255});
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
