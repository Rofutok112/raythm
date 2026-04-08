#pragma once

#include "scene.h"
#include "shared/scene_fade.h"
#include "title/title_bgm_controller.h"
#include "title/title_spectrum_visualizer.h"

// タイトル画面。曲選択画面・設定画面への遷移を提供する。
class title_scene final : public scene {
public:
    explicit title_scene(scene_manager& manager);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    void start_song_select_transition();

    bool quitting_ = false;
    scene_fade quit_fade_{scene_fade::direction::out, 1.5f, 1.0f};
    float esc_hold_t_ = 0.0f;
    bool transitioning_to_song_select_ = false;
    scene_fade transition_fade_{scene_fade::direction::out, 0.3f, 0.65f};
    title_bgm_controller bgm_controller_;
    title_spectrum_visualizer spectrum_visualizer_;
};
