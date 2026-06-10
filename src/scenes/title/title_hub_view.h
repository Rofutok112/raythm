#pragma once

#include "shared/auth_overlay_controller.h"
#include "multiplayer/multiplayer_state.h"
#include "shared/scene_fade.h"
#include "song_select/song_select_state.h"
#include "title/title_browse_feature.h"
#include "title/title_audio_controller.h"
#include "title/title_play_create_feature.h"
#include "title/title_profile_controller.h"
#include "title/title_settings_overlay.h"
#include "title/title_startup_controller.h"

namespace title_hub_view {

enum class mode {
    title,
    home,
    play,
    multiplayer,
    online,
    create,
    settings,
};

struct model {
    mode current_mode = mode::title;
    bool transitioning_to_song_select = false;
    bool quitting = false;
    bool intro_hold_active = false;
    float home_menu_anim = 0.0f;
    float play_view_anim = 0.0f;
    int home_menu_selected_index = 0;
    std::string_view home_status_message;
    Rectangle play_entry_origin_rect{};
};

struct draw_context {
    model view;
    title_play_create_feature& play_create_feature;
    multiplayer::state& multiplayer_state;
    title_browse_feature& browse_feature;
    title_startup_controller::state& startup;
    title_audio_controller& audio_controller;
    title_settings_overlay& settings_overlay;
    title_profile_controller& profile_controller;
    auth_overlay::controller& auth_controller;
    const title_play_create_feature::cross_callbacks& play_cross_callbacks;
    scene_fade& intro_fade;
    scene_fade& transition_fade;
    scene_fade& quit_fade;
};

struct draw_result {
    song_select::login_dialog_command login_command = song_select::login_dialog_command::none;
    bool close_login_dialog = false;
    bool open_profile = false;
};

draw_result draw(draw_context context);

}  // namespace title_hub_view
