#include "title_scene.h"

#include <memory>

#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "theme.h"
#include "virtual_screen.h"

title_scene::title_scene(scene_manager& manager) : scene(manager) {
}

// ENTER で曲選択、S で設定画面へ遷移する。
void title_scene::update(float dt) {
    if (transitioning_to_song_select_) {
        transition_fade_t_ = std::min(1.0f, transition_fade_t_ + dt / 0.3f);
        if (transition_fade_t_ >= 1.0f) {
            manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        }
        return;
    }

    if (quitting_) {
        quit_fade_t_ = std::min(1.0f, quit_fade_t_ + dt / 1.5f);
        if (quit_fade_t_ >= 1.0f) {
            CloseWindow();
        }
        return;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        transitioning_to_song_select_ = true;
        transition_fade_t_ = 0.0f;
        return;
    }

    if (IsKeyDown(KEY_ESCAPE)) {
        esc_hold_t_ += dt;
        if (esc_hold_t_ >= 0.3f) {
            esc_hold_t_ = 0.0f;
            quitting_ = true;
            quit_fade_t_ = 0.0f;
        }
    } else {
        esc_hold_t_ = 0.0f;
    }
}

// タイトルロゴと操作案内を描画する。
void title_scene::draw() {
    const auto& t = *g_theme;
    virtual_screen::begin();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);
    DrawText("raythm", 72, 84, 124, t.text);
    DrawText("trace the line before the beat disappears", 82, 212, 30, t.text_dim);
    DrawText("ENTER: Song Select", 82, 644, 22, t.text_muted);
    DrawText("ESC: Quit", 82, 674, 22, t.text_muted);
    if (transitioning_to_song_select_) {
        DrawRectangle(0, 0, kScreenWidth, kScreenHeight,
                      Color{0, 0, 0, static_cast<unsigned char>(std::min(0.65f, transition_fade_t_ * 0.65f) * 255.0f)});
    }
    if (quitting_) {
        DrawRectangle(0, 0, kScreenWidth, kScreenHeight,
                      Color{0, 0, 0, static_cast<unsigned char>(std::min(1.0f, quit_fade_t_) * 255.0f)});
    }
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
