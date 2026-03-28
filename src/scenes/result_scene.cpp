#include "result_scene.h"

#include <memory>

#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "title_scene.h"
#include "virtual_screen.h"

result_scene::result_scene(scene_manager& manager, result_data result, bool ranking_enabled)
    : scene(manager), result_(result), ranking_enabled_(ranking_enabled) {
}

// ENTER で曲選択へ、ESC でタイトルへ遷移する。
void result_scene::update(float dt) {
    (void)dt;

    if (IsKeyPressed(KEY_ENTER)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<title_scene>(manager_));
    }
}

// スコア・ランク・達成率・判定内訳・コンボ情報・ランキング資格を表示する。
void result_scene::draw() {
    virtual_screen::begin();
    draw_scene_frame("Result", "ENTER: Retry Flow    ESC: Title", {202, 138, 4, 255});
    if (result_.failed) {
        DrawText("FAILED", 130, 228, 48, Color{220, 38, 38, 255});
    }
    DrawText(TextFormat("Score: %07d", result_.score), 130, 280, 40, BLACK);

    const char* rank_label = "F";
    switch (result_.clear_rank) {
        case rank::ss:
            rank_label = "SS";
            break;
        case rank::s:
            rank_label = "S";
            break;
        case rank::a:
            rank_label = "A";
            break;
        case rank::b:
            rank_label = "B";
            break;
        case rank::c:
            rank_label = "C";
            break;
        case rank::f:
            rank_label = "F";
            break;
    }

    DrawText(TextFormat("Rank: %s", rank_label), 130, 338, 40, BLACK);
    DrawText(TextFormat("Accuracy: %.2f%%", result_.achievement), 130, 390, 28, DARKGRAY);
    DrawText(TextFormat("Perfect %d  Great %d  Good %d  Bad %d  Miss %d", result_.judge_counts[0],
                        result_.judge_counts[1], result_.judge_counts[2], result_.judge_counts[3],
                        result_.judge_counts[4]),
             130, 438, 28, DARKGRAY);
    DrawText(TextFormat("Max Combo %d  Fast %d  Slow %d", result_.max_combo, result_.fast_count, result_.slow_count), 130,
             478, 26, GRAY);
    DrawText(ranking_enabled_ ? "Ranking: Eligible" : "Ranking: Disabled", 130, 522, 24,
             ranking_enabled_ ? Color{14, 146, 108, 255} : Color{220, 38, 38, 255});
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
