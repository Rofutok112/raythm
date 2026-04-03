#pragma once

#include <string>

#include "raylib.h"
#include "scene.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_preview_controller.h"
#include "song_select/song_select_overlay_view.h"
#include "song_select/song_select_state.h"

class song_select_scene final : public scene {
public:
    explicit song_select_scene(scene_manager& manager, std::string preferred_song_id = "");

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    void reload_song_library(const std::string& preferred_song_id = "",
                             const std::string& preferred_chart_id = "");
    void sync_selected_song_media();
    void apply_delete_result(const song_select::delete_result& result);
    bool handle_song_list_pointer(Vector2 mouse, bool left_pressed, bool right_pressed);
    void apply_context_menu_command(song_select::context_menu_command command);
    void apply_confirmation_command(song_select::confirmation_command command);

    song_select::state state_;
    song_select::preview_controller preview_controller_;
    std::string preferred_song_id_;
};
