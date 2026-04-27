#pragma once

#include "scene.h"
#include "network/auth_client.h"
#include "ranking_service.h"
#include "shared/auth_overlay_controller.h"
#include "song_select/song_catalog_service.h"
#include "shared/scene_fade.h"
#include "settings/settings_layout.h"
#include "settings/settings_pages.h"
#include "song_select/song_select_state.h"
#include "song_select/song_transfer_controller.h"
#include "title/create_upload_client.h"
#include "title/online_download_view.h"
#include "title/profile_view.h"
#include "title/title_audio_controller.h"
#include <future>
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
    void apply_play_delete_result(const song_select::delete_result& result);
    void apply_play_transfer_result(const song_select::transfer_result& result);
    void open_overwrite_song_confirmation(std::vector<song_select::song_import_request> requests);
    void open_overwrite_chart_confirmation(std::vector<song_select::chart_import_request> requests);
    void poll_play_transfer();
    void start_song_import();
    void start_chart_import();
    void start_song_export();
    void start_chart_export();
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
    void open_profile();
    void request_profile_reload();
    void poll_profile();
    bool handle_profile_input();
    void draw_profile_modal();
    void start_profile_delete_song(std::string song_id);
    void start_profile_delete_chart(std::string chart_id);
    bool update_home_pointer_suppression();
    void update_settings_view_animation(float dt);
    bool handle_title_input(bool left_click_for_home, bool right_click_for_home);
    bool handle_home_input(bool suppress_pointer_this_frame);
    void update_current_settings_page();
    void draw_settings_view() const;
    void draw_current_settings_page() const;
    void change_settings_page(settings::page_id next_page);
    [[nodiscard]] bool current_settings_page_blocks_navigation() const;
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
    float settings_view_anim_ = 0.0f;
    bool settings_view_closing_ = false;
    settings::page_id current_settings_page_ = settings::page_id::gameplay;
    settings_runtime_applier settings_runtime_applier_;
    settings_gameplay_page settings_gameplay_page_;
    settings_audio_page settings_audio_page_;
    settings_video_page settings_video_page_;
    settings_key_config_page settings_key_config_page_;
    song_select::state play_state_;
    std::future<song_select::catalog_data> play_catalog_future_;
    bool play_catalog_loading_ = false;
    std::future<title_create_upload::upload_result> create_upload_future_;
    bool create_upload_in_progress_ = false;
    std::future<ranking_service::listing> play_ranking_future_;
    song_select::transfer::controller transfer_controller_;
    bool play_ranking_loading_ = false;
    bool play_ranking_reload_pending_ = false;
    int play_ranking_generation_ = 0;
    int play_ranking_pending_generation_ = 0;
    std::future<bool> scoring_ruleset_future_;
    bool scoring_ruleset_loading_ = false;
    title_profile_view::state profile_state_;
    std::future<title_profile_view::load_result> profile_load_future_;
    std::future<auth::operation_result> profile_delete_future_;
    bool play_catalog_reload_pending_ = false;
    bool play_catalog_sync_media_on_apply_ = false;
    bool queued_play_catalog_sync_media_on_apply_ = false;
    bool play_catalog_calculate_missing_levels_ = false;
    bool queued_play_catalog_calculate_missing_levels_ = false;
    std::string play_catalog_song_id_;
    std::string play_catalog_chart_id_;
    std::string queued_play_catalog_song_id_;
    std::string queued_play_catalog_chart_id_;
    title_online_view::state online_state_;
    title_audio_controller audio_controller_;
    auth_overlay::controller auth_controller_;
};
