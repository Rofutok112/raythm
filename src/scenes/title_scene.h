#pragma once

#include "scene.h"
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
    bool quitting_ = false;
    float quit_fade_t_ = 0.0f;
    float esc_hold_t_ = 0.0f;
    bool transitioning_to_song_select_ = false;
    float transition_fade_t_ = 0.0f;
    title_bgm_controller bgm_controller_;
    title_spectrum_visualizer spectrum_visualizer_;
};
