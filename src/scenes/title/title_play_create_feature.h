#pragma once

#include <functional>
#include <optional>
#include <string>

#include "raylib.h"
#include "scene_manager.h"
#include "song_select/song_select_state.h"
#include "title/catalog_reload_policy.h"
#include "title/create_tools_model.h"
#include "title/title_command.h"
#include "title/title_audio_controller.h"
#include "title/title_create_mode_controller.h"
#include "title/title_play_data_controller.h"
#include "title/title_play_mode_controller.h"
#include "title/title_play_transfer_controller.h"
#include "title/title_selection_media_coordinator.h"

class title_play_create_feature {
public:
    struct cross_callbacks {
        std::function<void()> stop_preview;
        std::function<void(const std::string&)> mark_online_song_removed;
        std::function<void()> reload_online_catalog;
    };

    struct play_update_result {
        title::command title_command;
    };

    struct create_update_callbacks {
        std::function<void()> enter_home;
    };

    [[nodiscard]] song_select::state& state();
    [[nodiscard]] const song_select::state& state() const;
    [[nodiscard]] title_play_transfer_controller& transfer_controller();
    [[nodiscard]] const title_play_transfer_controller& transfer_controller() const;
    [[nodiscard]] const title_create_tools_model::view_model& create_tools_model() const;

    void reset();
    void on_exit();

    void on_enter_play(bool multiplayer_chart_pick_active,
                       const std::string& multiplayer_server_url,
                       title_audio_controller& audio_controller);
    void on_enter_create(title_audio_controller& audio_controller);

    void request_catalog_reload(std::string preferred_song_id = "",
                                std::string preferred_chart_id = "",
                                title_catalog::reload_policy policy = title_catalog::policy_for(
                                    title_catalog::reload_mode::quiet_refresh));
    void poll_catalog_reload(title_audio_controller& audio_controller,
                             bool play_mode_active,
                             bool create_mode_active);
    void request_ranking_reload();
    void poll_ranking_reload();
    void request_scoring_ruleset_warm(bool force_refresh = false);
    bool poll_scoring_ruleset_warm();

    [[nodiscard]] bool catalog_loading() const;
    [[nodiscard]] load_progress catalog_progress() const;
    [[nodiscard]] bool scoring_ruleset_loading() const;
    [[nodiscard]] bool upload_in_progress() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] title_selection_media_snapshot media_snapshot(
        const title_audio_controller& audio_controller) const;

    void poll_transfer(const cross_callbacks& callbacks);
    bool poll_create_upload();
    void cancel_confirmation();
    void draw_or_apply_confirmation(title_audio_controller& audio_controller,
                                    const cross_callbacks& callbacks);

    [[nodiscard]] play_update_result update_play(scene_manager& manager,
                                                 title_audio_controller& audio_controller,
                                                 float anim_t,
                                                 Rectangle origin_rect,
                                                 float dt);
    void update_create(scene_manager& manager,
                       title_audio_controller& audio_controller,
                       float anim_t,
                       Rectangle origin_rect,
                       float dt,
                       const cross_callbacks& cross,
                       const create_update_callbacks& callbacks);

    void capture_current_selection();
    [[nodiscard]] const std::string& preferred_song_id() const;
    [[nodiscard]] const std::string& preferred_chart_id() const;
    void set_preferred_selection(std::string song_id, std::string chart_id);

private:
    [[nodiscard]] title_play_transfer_controller::catalog_callbacks make_transfer_callbacks(
        const cross_callbacks& callbacks);
    void refresh_create_tools_model(bool force_bindings = false);
    void sync_selection_media(title_audio_controller& audio_controller,
                              title_selection_media_coordinator::context active_context,
                              bool force = false);
    void draw_or_apply_upload_confirmation();

    song_select::state state_;
    title_play_data_controller data_controller_;
    title_play_transfer_controller transfer_controller_;
    title_selection_media_coordinator media_coordinator_;
    title_create_tools_model::view_model create_tools_model_;
    title_create_tools_model::bindings create_tools_bindings_;
    bool create_tools_binding_cache_valid_ = false;
    std::string create_tools_binding_server_url_;
    std::string create_tools_binding_song_id_;
    std::string create_tools_binding_chart_id_;
    std::string preferred_song_id_;
    std::string preferred_chart_id_;
};
