#pragma once

#include <string>
#include <vector>

#include "ui_notice.h"

namespace public_profile {

struct notice_request {
    std::string message;
    ui::notice_tone tone = ui::notice_tone::info;
    float seconds = 2.0f;
};

struct effects {
    std::vector<notice_request> notices;
    bool reload_friends = false;
};

void apply_notice(const notice_request& notice);
void apply_effects(const effects& effects);

}  // namespace public_profile
