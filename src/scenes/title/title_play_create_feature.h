#pragma once

#include <functional>
#include <future>
#include <optional>
#include <string>

#include "raylib.h"
#include "scene_manager.h"
#include "song_select/song_select_state.h"
#include "title/create_tools_model.h"
#include "title/title_audio_controller.h"
#include "title/title_create_mode_controller.h"
#include "title/title_play_data_controller.h"
#include "title/title_play_mode_controller.h"
#include "title/title_play_transfer_controller.h"

class title_play_create_feature {
public:
    struct cross_callbacks {
        std::function<void()> stop_preview;
        std::function<void(const std::string&)> mark_online_song_removed;
        std::function<void()> reload_online_catalog;
    };

    struct play_update_callbacks {
        std::function<void()> enter_home;
        std::function<void(const std::string&, const std::string&)> open_update_catalog;
        std::function<bool()> add_selected_to_multiplayer;
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
                                bool sync_media_on_apply = false,
                                bool calculate_missing_levels = false);
    void poll_catalog_reload(title_audio_controller& audio_controller,
                             bool play_mode_active,
                             bool create_mode_active);
    void request_ranking_reload();
    void poll_ranking_reload();
    void request_scoring_ruleset_warm(bool force_refresh = false);
    bool poll_scoring_ruleset_warm();

    [[nodiscard]] bool catalog_loading() const;
    [[nodiscard]] bool scoring_ruleset_loading() const;
    [[nodiscard]] bool upload_in_progress() const;
    [[nodiscard]] bool busy() const;

    void poll_transfer(const cross_callbacks& callbacks, bool sync_media_on_reload);
    bool poll_create_upload(bool sync_media_on_apply);
    void cancel_confirmation();
    void draw_or_apply_confirmation(title_audio_controller& audio_controller,
                                    const cross_callbacks& callbacks,
                                    bool sync_media_on_reload);

    void update_play(scene_manager& manager,
                     title_audio_controller& audio_controller,
                     float anim_t,
                     Rectangle origin_rect,
                     float dt,
                     const play_update_callbacks& callbacks);
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
    void request_create_permission_refresh(const std::string& server_url,
                                           const std::optional<std::string>& current_user_id,
                                           const title_create_tools_model::bindings& bindings,
                                           const std::optional<bool>& song_permission_hint,
                                           const std::optional<bool>& chart_permission_hint);
    void poll_create_permission_refresh();
    void sync_play_media(title_audio_controller& audio_controller);
    void sync_create_preview(title_audio_controller& audio_controller);

    song_select::state state_;
    title_play_data_controller data_controller_;
    title_play_transfer_controller transfer_controller_;
    title_create_tools_model::view_model create_tools_model_;
    title_create_tools_model::bindings create_tools_bindings_;
    bool create_tools_binding_cache_valid_ = false;
    bool create_permission_refresh_in_progress_ = false;
    std::future<bool> create_permission_refresh_future_;
    std::string create_permission_refresh_key_;
    std::string create_tools_binding_server_url_;
    std::string create_tools_binding_song_id_;
    std::string create_tools_binding_chart_id_;
    std::string preferred_song_id_;
    std::string preferred_chart_id_;
};
