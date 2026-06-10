#pragma once

#include <functional>

#include "shared/auth_overlay_controller.h"
#include "shared/public_profile_controller.h"
#include "shared/scene_fade.h"
#include "title/catalog_reload_coordinator.h"
#include "title/title_audio_controller.h"
#include "title/title_browse_feature.h"
#include "title/title_play_create_feature.h"
#include "title/title_profile_controller.h"
#include "title/title_settings_overlay.h"
#include "title/title_startup_controller.h"

namespace title {

enum class common_mode {
    title,
    home,
    play,
    multiplayer,
    online,
    create,
    settings,
};

struct common_update_context {
    common_mode mode = common_mode::title;
    common_mode settings_return_mode = common_mode::home;
    auth_overlay::controller& auth_controller;
    title_play_create_feature& play_create_feature;
    title_audio_controller& audio_controller;
    title_browse_feature& browse_feature;
    title_catalog_reload_coordinator& catalog_reload_coordinator;
    title_profile_controller& profile_controller;
    public_profile::controller& public_profile_controller;
    title_settings_overlay& settings_overlay;
    title_startup_controller::state& startup;
    scene_fade& intro_fade;
    float& intro_hold_t;
    float& home_menu_anim;
    float& play_view_anim;
    std::string& preferred_song_id;
    std::string& preferred_chart_id;
    title_play_create_feature::cross_callbacks cross_callbacks;
    std::function<void()> refresh_multiplayer_local_index;
    std::function<title_audio_policy::hub_mode()> current_audio_mode;
};

void update_common_frame(common_update_context& context, float dt);

}  // namespace title
