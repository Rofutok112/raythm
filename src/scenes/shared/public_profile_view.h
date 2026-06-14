#pragma once

#include "network/auth_client.h"
#include "raylib.h"
#include "ui_draw.h"

#include <string>

namespace public_profile_view {

enum class command_type {
    none,
    close,
    start_relationship_action,
};

struct command {
    command_type type = command_type::none;
};

struct model {
    bool loading = false;
    bool loaded_once = false;
    bool relationship_operation_active = false;
    float open_anim = 0.0f;
    std::string avatar_base_url;
    auth::public_profile_result result;
};

[[nodiscard]] Rectangle bounds(float open_anim);
[[nodiscard]] command handle_input(const model& state, ui::draw_layer layer = ui::draw_layer::modal);
void draw(const model& state, ui::draw_layer layer = ui::draw_layer::modal);

}  // namespace public_profile_view
