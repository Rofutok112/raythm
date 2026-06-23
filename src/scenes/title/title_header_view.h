#pragma once

#include <string_view>

#include "network/auth_client.h"
#include "raylib.h"

namespace title_header_view {

struct draw_config {
    Rectangle closed_header_rect;
    Rectangle open_header_rect;
    Rectangle refresh_chip_rect;
    Rectangle friends_chip_rect;
    Rectangle settings_chip_rect;
    Rectangle account_chip_rect;
    float menu_t = 0.0f;
    float play_t = 0.0f;
    std::string_view account_name;
    std::string_view account_status;
    std::string_view avatar_label;
    std::string_view avatar_url;
    std::string_view avatar_base_url;
    bool logged_in = false;
    bool email_verified = false;
    auth::rating_summary rating;
    int friends_badge_count = 0;
    double now = 0.0;
};

void draw(const draw_config& config);
void draw_screen_overlay(const draw_config& config);

}  // namespace title_header_view
