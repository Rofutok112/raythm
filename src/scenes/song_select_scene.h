#pragma once

#include <future>
#include <optional>
#include <string>

#include "raylib.h"
#include "scene.h"
#include "song_select/song_import_export_service.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_overlay_view.h"
#include "song_select/song_select_state.h"

class song_select_scene final : public scene {
public:
    explicit song_select_scene(scene_manager& manager, std::string preferred_song_id = "",
                               std::string preferred_chart_id = "",
                               std::optional<song_select::recent_result_offset> recent_result_offset = std::nullopt);

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
    void poll_background_transfer();
    void start_song_export(song_select::song_export_request request);
    void start_song_import(song_select::song_import_request request);
    bool adjust_selected_song_local_offset(int delta_ms);
    bool apply_recent_result_offset();
    bool handle_song_list_pointer(Vector2 mouse, bool left_pressed, bool right_pressed);
    void apply_context_menu_command(song_select::context_menu_command command);
    void apply_confirmation_command(song_select::confirmation_command command);

    song_select::state state_;
    song_select::preview_controller preview_controller_;
    std::string preferred_song_id_;
    std::string preferred_chart_id_;
    std::optional<song_select::recent_result_offset> recent_result_offset_;
    std::future<song_select::transfer_result> background_transfer_;
    bool background_transfer_active_ = false;
    std::string background_transfer_label_;
};
