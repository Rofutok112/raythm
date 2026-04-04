#include "title_scene.h"

#include <filesystem>
#include <memory>

#include "audio_manager.h"
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
const std::filesystem::path kTitleIntroPath = std::filesystem::path("assets") / "audio" / "title_intro.mp3";
const std::filesystem::path kTitleLoopPath = std::filesystem::path("assets") / "audio" / "title_loop.mp3";

}  // namespace

title_scene::title_scene(scene_manager& manager) : scene(manager) {
}

void title_scene::on_enter() {
    intro_bgm_path_ = kTitleIntroPath.string();
    loop_bgm_path_ = kTitleLoopPath.string();
    start_title_bgm();
}

void title_scene::on_exit() {
    bgm_phase_ = bgm_phase::stopped;
    audio_manager::instance().stop_bgm();
}

void title_scene::start_title_bgm() {
    bgm_phase_ = bgm_phase::stopped;
    if (intro_bgm_path_.empty() || !audio_manager::instance().load_bgm(intro_bgm_path_)) {
        return;
    }

    audio_manager::instance().play_bgm(true);
    bgm_phase_ = bgm_phase::intro;
}

void title_scene::update_title_bgm() {
    if (bgm_phase_ == bgm_phase::stopped || !audio_manager::instance().is_bgm_loaded()) {
        return;
    }

    if (audio_manager::instance().is_bgm_playing()) {
        return;
    }

    if (bgm_phase_ == bgm_phase::intro) {
        if (!loop_bgm_path_.empty() && audio_manager::instance().load_bgm(loop_bgm_path_)) {
            audio_manager::instance().play_bgm(true);
            bgm_phase_ = bgm_phase::loop;
            return;
        }
        bgm_phase_ = bgm_phase::stopped;
        return;
    }

    if (bgm_phase_ == bgm_phase::loop) {
        audio_manager::instance().seek_bgm(0.0);
        audio_manager::instance().play_bgm(true);
    }
}

// ENTER で曲選択、S で設定画面へ遷移する。
void title_scene::update(float dt) {
    update_title_bgm();

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
    ui::draw_text_in_rect("raythm", 124, kTitleRect, t.text, ui::text_align::left);
    ui::draw_text_in_rect("trace the line before the beat disappears", 30, kSubtitleRect, t.text_dim, ui::text_align::left);

    Rectangle hint_rows[2];
    ui::vstack(kHintAreaRect, 22.0f, 8.0f, hint_rows);
    ui::draw_text_in_rect("ENTER: Song Select", 22, hint_rows[0], t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect("ESC: Quit", 22, hint_rows[1], t.text_muted, ui::text_align::left);
    if (transitioning_to_song_select_) {
        ui::draw_fullscreen_overlay(
            Color{0, 0, 0, static_cast<unsigned char>(std::min(0.65f, transition_fade_t_ * 0.65f) * 255.0f)});
    }
    if (quitting_) {
        ui::draw_fullscreen_overlay(
            Color{0, 0, 0, static_cast<unsigned char>(std::min(1.0f, quit_fade_t_) * 255.0f)});
    }
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
