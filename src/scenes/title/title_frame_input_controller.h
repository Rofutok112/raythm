#pragma once

#include <optional>
#include <string>

#include "shared/auth_overlay_controller.h"
#include "title/catalog_reload_coordinator.h"
#include "title/home_menu_view.h"
#include "title/title_browse_feature.h"
#include "title/title_common_update_controller.h"
#include "title/title_play_create_feature.h"

namespace title {

struct frame_input_context {
    common_mode mode = common_mode::title;
    float home_menu_anim = 0.0f;
    int& home_menu_selected_index;
    std::string& home_status_message;
    bool& suppress_home_pointer_until_release;
    title_play_create_feature& play_create_feature;
    title_browse_feature& browse_feature;
    title_catalog_reload_coordinator& catalog_reload_coordinator;
    auth_overlay::controller& auth_controller;
};

struct frame_input_result {
    bool consumed = false;
    bool enter_title = false;
    bool enter_home = false;
    bool enter_home_suppress_pointer = false;
    bool enter_settings = false;
    std::optional<title_home_view::action> selected_home_action;
};

[[nodiscard]] frame_input_result update_frame_input(frame_input_context& context);

}  // namespace title
