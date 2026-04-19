#pragma once

#include <memory>

#include "scene.h"
#include "song_select/song_select_state.h"

class scene_manager;

namespace song_select {

std::unique_ptr<scene> make_title_scene(scene_manager& manager, bool start_with_home_open = false);
std::unique_ptr<scene> make_seamless_song_select_scene(
    scene_manager& manager,
    std::string preferred_song_id = "",
    std::string preferred_chart_id = "",
    std::optional<recent_result_offset> recent_result_offset = std::nullopt);
std::unique_ptr<scene> make_seamless_create_scene(
    scene_manager& manager,
    std::string preferred_song_id = "",
    std::string preferred_chart_id = "");
std::unique_ptr<scene> make_legacy_song_select_scene(
    scene_manager& manager,
    std::string preferred_song_id = "",
    std::string preferred_chart_id = "",
    std::optional<recent_result_offset> recent_result_offset = std::nullopt,
    bool open_login_dialog_on_enter = false);
std::unique_ptr<scene> make_settings_scene(scene_manager& manager);
std::unique_ptr<scene> make_song_create_scene(scene_manager& manager);
std::unique_ptr<scene> make_edit_song_scene(scene_manager& manager, const song_entry& song);
std::unique_ptr<scene> make_new_chart_scene(scene_manager& manager, const song_entry& song, int difficulty_index);
std::unique_ptr<scene> make_edit_chart_scene(scene_manager& manager, const song_entry& song, const chart_option& chart);
std::unique_ptr<scene> make_play_scene(scene_manager& manager, const song_entry& song, const chart_option& chart);
std::unique_ptr<scene> make_mv_editor_scene(scene_manager& manager, const song_entry& song);

}  // namespace song_select
