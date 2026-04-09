#include "title_scene.h"

#include <memory>

#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "theme.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kTitleHeaderRect = ui::place(kScreenRect, 760.0f, 170.0f,
                                                 ui::anchor::top_left, ui::anchor::top_left,
                                                 {72.0f, 84.0f});
constexpr Rectangle kTitleRect = {kTitleHeaderRect.x, kTitleHeaderRect.y, kTitleHeaderRect.width, 124.0f};
constexpr Rectangle kSubtitleRect = {kTitleHeaderRect.x + 10.0f, kTitleHeaderRect.y + 128.0f,
                                     kTitleHeaderRect.width - 10.0f, 30.0f};
constexpr Rectangle kHintAreaRect = ui::place(kScreenRect, 320.0f, 52.0f,
                                              ui::anchor::bottom_left, ui::anchor::bottom_left,
                                              {82.0f, -46.0f});
constexpr Rectangle kSpectrumRect = ui::place(kScreenRect, 760.0f, 150.0f,
                                              ui::anchor::bottom_right, ui::anchor::bottom_right,
                                              {-82.0f, -54.0f});
constexpr const char* kTitleIntroPath = "assets/audio/title_intro.mp3";
constexpr const char* kTitleLoopPath = "assets/audio/title_loop.mp3";

}  // namespace

title_scene::title_scene(scene_manager& manager) : scene(manager) {
}

void title_scene::start_song_select_transition() {
    if (transitioning_to_song_select_) {
        return;
    }
    transitioning_to_song_select_ = true;
    transition_fade_.restart(scene_fade::direction::out, 0.3f, 0.65f);
}

void title_scene::on_enter() {
    bgm_controller_.configure(kTitleIntroPath, kTitleLoopPath);
    spectrum_visualizer_.reset();
    bgm_controller_.on_enter();
}

void title_scene::on_exit() {
    bgm_controller_.on_exit();
    spectrum_visualizer_.reset();
}

// ENTER で曲選択、S で設定画面へ遷移する。
void title_scene::update(float dt) {
    ui::begin_hit_regions();
    bgm_controller_.update();
    spectrum_visualizer_.update();

    if (transitioning_to_song_select_) {
        transition_fade_.update(dt);
        if (transition_fade_.complete()) {
            manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        }
        return;
    }

    if (quitting_) {
        quit_fade_.update(dt);
        if (quit_fade_.complete()) {
            CloseWindow();
        }
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        start_song_select_transition();
        return;
    }

    if (IsKeyDown(KEY_ESCAPE)) {
        esc_hold_t_ += dt;
        if (esc_hold_t_ >= 0.3f) {
            esc_hold_t_ = 0.0f;
            quitting_ = true;
            quit_fade_.restart(scene_fade::direction::out, 1.5f, 1.0f);
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
    spectrum_visualizer_.draw(kSpectrumRect);
    ui::draw_text_in_rect("raythm", 124, kTitleRect, t.text, ui::text_align::left);
    ui::draw_text_in_rect("trace the line before the beat disappears", 30, kSubtitleRect, t.text_dim, ui::text_align::left);

    Rectangle hint_rows[2];
    ui::vstack(kHintAreaRect, 22.0f, 8.0f, hint_rows);
    ui::draw_text_in_rect("LEFT CLICK: Song Select", 22, hint_rows[0], t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect("ESC: Quit", 22, hint_rows[1], t.text_muted, ui::text_align::left);
    if (transitioning_to_song_select_) {
        transition_fade_.draw();
    }
    if (quitting_) {
        quit_fade_.draw();
    }
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
