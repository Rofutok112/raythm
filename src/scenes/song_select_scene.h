#pragma once

#include <optional>
#include <string>

#include "raylib.h"
#include "scene.h"
#include "shared/auth_overlay_controller.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_command_controller.h"
#include "song_select/song_select_confirmation_dialog.h"
#include "song_select/song_select_login_dialog.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_overlay_view.h"
#include "song_select/song_select_state.h"
#include "song_select/song_transfer_controller.h"

class song_select_scene final : public scene {
public:
    explicit song_select_scene(scene_manager& manager, std::string preferred_song_id = "",
                               std::string preferred_chart_id = "",
                               std::optional<song_select::recent_result_offset> recent_result_offset = std::nullopt,
                               bool open_login_dialog_on_enter = false);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    void reload_song_library(const std::string& preferred_song_id = "",
                             const std::string& preferred_chart_id = "");
    void sync_selected_song_media();
    void apply_delete_result(const song_select::delete_result& result);
    void apply_transfer_result(const song_select::transfer_result& result);
    bool adjust_selected_song_local_offset(int delta_ms);
    bool apply_recent_result_offset();
    void reload_selected_chart_ranking();
    void refresh_auth_state();
    bool handle_song_list_pointer(Vector2 mouse, bool left_pressed, bool right_pressed);
    void open_overwrite_song_confirmation(song_select::song_import_request request);
    void open_overwrite_chart_confirmation(song_select::chart_import_request request);

    song_select::state state_;
    song_select::preview_controller preview_controller_;
    std::string preferred_song_id_;
    std::string preferred_chart_id_;
    std::optional<song_select::recent_result_offset> recent_result_offset_;
    bool open_login_dialog_on_enter_ = false;
    auth_overlay::controller auth_controller_;
    song_select::transfer::controller transfer_controller_;
};
