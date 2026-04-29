#pragma once

#include "scene.h"
#include "network/auth_client.h"
#include "shared/auth_overlay_controller.h"
#include "shared/scene_fade.h"
#include "song_select/song_select_state.h"
#include "title/online_download_view.h"
#include "title/title_audio_controller.h"
#include "title/title_play_data_controller.h"
#include "title/title_play_transfer_controller.h"
#include "title/title_profile_controller.h"
#include "title/title_settings_overlay.h"
#include <optional>
#include <string>
#include <vector>

// タイトル画面。曲選択画面・設定画面への遷移を提供する。
class title_scene final : public scene {
public:
    enum class hub_mode {
        title,
        home,
        play,
        online,
        create,
        settings,
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
    void enter_settings_mode();
    void close_settings_mode();
    void update_play_mode(float dt);
    void update_online_mode(float dt);
    void update_create_mode(float dt);
    void update_settings_mode(float dt);
    void update_common_animation(float dt);
    [[nodiscard]] title_play_transfer_controller::catalog_callbacks play_transfer_callbacks();
    bool handle_account_input();
    bool handle_settings_button_input();
    bool handle_refresh_button_input();
    bool handle_login_dialog_input();
    void request_play_catalog_reload(std::string preferred_song_id = "",
                                     std::string preferred_chart_id = "",
                                     bool sync_media_on_apply = false,
                                     bool calculate_missing_levels = false);
    void poll_play_catalog_reload();
    void capture_current_play_selection();
    void sync_play_media();
    void request_play_ranking_reload();
    void poll_play_ranking_reload();
    void request_scoring_ruleset_warm(bool force_refresh = false);
    void poll_scoring_ruleset_warm();
    void start_song_upload(const song_select::song_entry& song);
    void start_chart_upload(const song_select::song_entry& song,
                            const song_select::chart_option& chart);
    void poll_create_upload();
    bool handle_profile_input();
    bool update_home_pointer_suppression();
    bool handle_title_input(bool left_click_for_home, bool right_click_for_home);
    bool handle_home_input(bool suppress_pointer_this_frame);
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
    hub_mode settings_return_mode_ = hub_mode::home;
    float home_menu_anim_ = 0.0f;
    int home_menu_selected_index_ = 0;
    std::string home_status_message_;
    float play_view_anim_ = 0.0f;
    Rectangle play_entry_origin_rect_{};
    title_settings_overlay settings_overlay_;
    song_select::state play_state_;
    title_play_data_controller play_data_controller_;
    title_play_transfer_controller play_transfer_controller_;
    title_profile_controller profile_controller_;
    title_online_view::state online_state_;
    title_audio_controller audio_controller_;
    auth_overlay::controller auth_controller_;
};
