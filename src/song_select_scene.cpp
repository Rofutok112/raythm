#include "song_select_scene.h"

#include <array>
#include <memory>

#include "game_settings.h"
#include "play_scene.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "title_scene.h"

namespace {
constexpr std::array<const char*, 3> kDifficulties = {"Normal", "Hyper", "Another"};
constexpr std::array<const char*, 2> kKeyModes = {"4 Keys", "6 Keys"};
}

song_select_scene::song_select_scene(scene_manager& manager) : scene(manager) {
}

// 左右キーで難易度、上下キーでキーモードを切り替え、ENTER でプレイ画面に遷移する。
void song_select_scene::update(float dt) {
    (void)dt;

    if (IsKeyPressed(KEY_LEFT)) {
        difficulty_index_ = (difficulty_index_ + static_cast<int>(kDifficulties.size()) - 1) %
                            static_cast<int>(kDifficulties.size());
    } else if (IsKeyPressed(KEY_RIGHT)) {
        difficulty_index_ = (difficulty_index_ + 1) % static_cast<int>(kDifficulties.size());
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        key_mode_index_ = (key_mode_index_ + 1) % static_cast<int>(kKeyModes.size());
    }

    if (IsKeyPressed(KEY_ENTER)) {
        g_settings.selected_key_count = key_mode_index_ == 0 ? 4 : 6;
        manager_.change_scene(std::make_unique<play_scene>(manager_, g_settings.selected_key_count));
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<title_scene>(manager_));
    }
}

// 選択中の曲名・難易度・キーモードを表示する。
void song_select_scene::draw() {
    draw_scene_frame("Song Select", "LEFT/RIGHT: Difficulty    UP/DOWN: Key Mode    ENTER: Play", {14, 146, 108, 255});
    DrawText("Selected Song: Sample Song", 130, 290, 36, BLACK);
    DrawText(TextFormat("Difficulty: %s", kDifficulties[difficulty_index_]), 130, 360, 30, DARKGRAY);
    DrawText(TextFormat("Mode: %s", kKeyModes[key_mode_index_]), 130, 405, 30, DARKGRAY);
    DrawText("ESC: Back to Title", 130, 490, 24, GRAY);
}
