#pragma once

#include <functional>
#include <string>

#include "raylib.h"
#include "scene_manager.h"
#include "title/catalog_reload_coordinator.h"
#include "title/title_audio_controller.h"
#include "title/title_browse_feature.h"
#include "title/title_command.h"
#include "title/title_play_create_feature.h"

namespace title {

struct mode_update_context {
    scene_manager& manager;
    title_play_create_feature& play_create_feature;
    title_audio_controller& audio_controller;
    title_browse_feature& browse_feature;
    title_catalog_reload_coordinator& catalog_reload_coordinator;
    float& play_view_anim;
    Rectangle& play_entry_origin_rect;
    std::string& preferred_song_id;
    std::string& preferred_chart_id;
    title_play_create_feature::cross_callbacks cross_callbacks;
    std::function<void(bool)> enter_home_mode;
    std::function<void()> enter_play_mode;
};

[[nodiscard]] command update_play_mode(mode_update_context& context, float dt);
void update_create_mode(mode_update_context& context, float dt);
void update_online_mode(mode_update_context& context, float dt);

}  // namespace title
