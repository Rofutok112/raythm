#pragma once

#include <string>

#include "scene.h"

// タイトル画面。曲選択画面・設定画面への遷移を提供する。
class title_scene final : public scene {
public:
    explicit title_scene(scene_manager& manager);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    enum class bgm_phase {
        stopped,
        intro,
        loop,
    };

    void start_title_bgm();
    void update_title_bgm();

    bool quitting_ = false;
    float quit_fade_t_ = 0.0f;
    float esc_hold_t_ = 0.0f;
    bool transitioning_to_song_select_ = false;
    float transition_fade_t_ = 0.0f;
    bgm_phase bgm_phase_ = bgm_phase::stopped;
    std::string intro_bgm_path_;
    std::string loop_bgm_path_;
};
