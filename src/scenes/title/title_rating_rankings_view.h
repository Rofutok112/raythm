#pragma once

#include <string>
#include <vector>

#include "network/auth_client.h"
#include "raylib.h"
#include "ui_draw.h"

namespace title_rating_rankings_view {

enum class command_type {
    none,
    close,
    refresh,
    previous_page,
    next_page,
    open_profile,
};

struct command {
    command_type type = command_type::none;
    std::string user_id;
};

struct model {
    float open_anim = 0.0f;
    bool loading = false;
    bool loaded_once = false;
    int page = 1;
    int page_size = 50;
    int total = 0;
    bool has_next_page = false;
    float scroll_offset = 0.0f;
    std::string avatar_base_url;
    std::string message;
    std::vector<auth::global_rating_ranking_entry> items;
};

[[nodiscard]] command handle_input(model& state, ui::draw_layer layer = ui::draw_layer::modal);
void draw(const model& state, ui::draw_layer layer = ui::draw_layer::modal, bool draw_backdrop = true);
[[nodiscard]] Rectangle modal_bounds(float open_anim);
void clamp_scroll(model& state);

}  // namespace title_rating_rankings_view
