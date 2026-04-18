#pragma once

#include "scene.h"
#include "shared/auth_overlay_controller.h"
#include "shared/scene_fade.h"
#include "song_select/song_select_login_dialog.h"
#include "song_select/song_select_state.h"
#include "title/title_bgm_controller.h"
#include "title/title_spectrum_visualizer.h"
#include <string>

// タイトル画面。曲選択画面・設定画面への遷移を提供する。
class title_scene final : public scene {
public:
    enum class transition_target {
        song_select,
        song_create,
    };

    explicit title_scene(scene_manager& manager);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    void start_transition(transition_target target);

    bool quitting_ = false;
    scene_fade quit_fade_{scene_fade::direction::out, 1.5f, 1.0f};
    float esc_hold_t_ = 0.0f;
    bool transitioning_to_song_select_ = false;
    scene_fade transition_fade_{scene_fade::direction::out, 0.3f, 0.65f};
    transition_target transition_target_ = transition_target::song_select;
    bool home_menu_open_ = false;
    float home_menu_anim_ = 0.0f;
    int home_menu_selected_index_ = 0;
    std::string home_status_message_;
    song_select::auth_state auth_state_;
    song_select::login_dialog_state login_dialog_;
    title_bgm_controller bgm_controller_;
    title_spectrum_visualizer spectrum_visualizer_;
    auth_overlay::controller auth_controller_;
};
