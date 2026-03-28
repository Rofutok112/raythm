#include "title_scene.h"

#include <memory>

#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "settings_scene.h"
#include "song_select_scene.h"
#include "virtual_screen.h"

title_scene::title_scene(scene_manager& manager) : scene(manager) {
}

// ENTER で曲選択、S で設定画面へ遷移する。
void title_scene::update(float dt) {
    (void)dt;

    if (IsKeyPressed(KEY_ENTER)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
    } else if (IsKeyPressed(KEY_S)) {
        manager_.change_scene(std::make_unique<settings_scene>(manager_));
    }
}

// タイトルロゴと操作案内を描画する。
void title_scene::draw() {
    virtual_screen::begin();
    draw_scene_frame("Title", "ENTER: Song Select    S: Settings", {32, 129, 226, 255});
    DrawText("raythm", 130, 300, 92, BLACK);
    DrawText("Scene management bootstrap", 136, 400, 28, GRAY);
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
