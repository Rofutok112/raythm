#pragma once

#include "network/auth_client.h"
#include "raylib.h"
#include "ui_draw.h"

#include <string>

namespace public_profile_view {

enum class tab {
    overview,
    best_rc,
    links,
    social,
};

enum class command_type {
    none,
    close,
    start_relationship_action,
    select_tab,
};

struct command {
    command_type type = command_type::none;
    tab selected_tab = tab::overview;
};

struct scroll_offsets {
    float link_scroll = 0.0f;
    float best_rating_scroll = 0.0f;
};

struct input_result {
    command action;
    bool scroll_changed = false;
    scroll_offsets scroll;
};

struct model {
    bool loading = false;
    bool loaded_once = false;
    bool relationship_operation_active = false;
    float open_anim = 0.0f;
    float link_scroll = 0.0f;
    float best_rating_scroll = 0.0f;
    tab selected_tab = tab::overview;
    std::string avatar_base_url;
    auth::public_profile_result result;
};

[[nodiscard]] Rectangle bounds(float open_anim);
[[nodiscard]] scroll_offsets clamped_scroll_offsets(const model& state, ui::draw_layer layer = ui::draw_layer::modal);
[[nodiscard]] input_result handle_input(const model& state, ui::draw_layer layer = ui::draw_layer::modal);
void draw(const model& state, ui::draw_layer layer = ui::draw_layer::modal, bool draw_backdrop = true);

}  // namespace public_profile_view
