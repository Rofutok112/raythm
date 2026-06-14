#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ui_notice.h"

namespace title_friends_effects {

struct notice_request {
    std::string message;
    ui::notice_tone tone = ui::notice_tone::info;
    float seconds = 2.0f;
};

struct room_join_request {
    std::string room_id;
    std::string invite_id;
};

struct feature_effects {
    std::vector<notice_request> notices;
    std::optional<std::string> profile_user_id;
    std::optional<room_join_request> room_join;
};

void apply_notice(const notice_request& notice);
void apply_effects(const feature_effects& effects);

}  // namespace title_friends_effects
