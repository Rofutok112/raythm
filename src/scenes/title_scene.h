#pragma once

#include "scene.h"
#include "shared/auth_overlay_controller.h"
#include "shared/scene_fade.h"
#include "song_select/song_select_state.h"
#include "title/online_download_view.h"
#include "title/title_audio_controller.h"
#include <optional>
#include <string>

// タイトル画面。曲選択画面・設定画面への遷移を提供する。
class title_scene final : public scene {
public:
    enum class hub_mode {
        title,
        home,
        play,
        online,
        create,
    };

    enum class transition_target {
        song_select,
        online_download,
        create_tools,
    };

    explicit title_scene(scene_manager& manager,
                         bool start_with_home_open = false,
                         bool play_intro_fade = true,
                         std::string preferred_song_id = "",
                         std::string preferred_chart_id = "",
                         std::optional<song_select::recent_result_offset> recent_result_offset = std::nullopt,
                         bool start_in_play_view = false,
                         bool start_in_create_view = false);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    void start_transition(transition_target target);
    void enter_title_mode();
    void enter_home_mode(bool suppress_pointer = false);
    void enter_play_mode();
    void enter_online_mode();
    void enter_create_mode();
    void update_play_mode(float dt);
    void update_online_mode(float dt);
    void update_create_mode(float dt);
    void update_common_animation(float dt);
    bool handle_account_input();
    bool handle_login_dialog_input();
    void update_home_pointer_suppression();
    bool handle_title_input(bool left_click_for_home, bool right_click_for_home);
    bool handle_home_input();
    void update_title_quit(float dt);
    [[nodiscard]] title_audio_policy::hub_mode current_audio_mode() const;

    bool quitting_ = false;
    scene_fade quit_fade_{scene_fade::direction::out, 1.5f, 1.0f};
    float esc_hold_t_ = 0.0f;
    bool transitioning_to_song_select_ = false;
    scene_fade transition_fade_{scene_fade::direction::out, 0.3f, 0.65f};
    scene_fade intro_fade_{scene_fade::direction::in, 0.45f, 1.0f};
    float intro_hold_t_ = 0.0f;
    transition_target transition_target_ = transition_target::song_select;
    bool start_with_home_open_ = false;
    bool play_intro_fade_ = true;
    std::string preferred_song_id_;
    std::string preferred_chart_id_;
    std::optional<song_select::recent_result_offset> recent_result_offset_;
    bool start_in_play_view_ = false;
    bool start_in_create_view_ = false;
    bool suppress_home_pointer_until_release_ = false;
    hub_mode mode_ = hub_mode::title;
    float home_menu_anim_ = 0.0f;
    int home_menu_selected_index_ = 0;
    std::string home_status_message_;
    float play_view_anim_ = 0.0f;
    Rectangle play_entry_origin_rect_{};
    song_select::state play_state_;
    title_online_view::state online_state_;
    title_audio_controller audio_controller_;
    auth_overlay::controller auth_controller_;
};
