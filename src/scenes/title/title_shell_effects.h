#pragma once

#include <functional>
#include <string>

#include "shared/public_profile_effects.h"
#include "title/title_friends_effects.h"

namespace title {

struct shell_effect_context {
    std::function<void(const std::string& user_id)> open_public_profile;
    std::function<void(const title_friends_effects::room_join_request& request)> start_room_join;
    std::function<void()> reload_friends;
};

struct shell_effect_result {
    bool scene_flow_changed = false;
};

shell_effect_result apply_friends_effects(const title_friends_effects::feature_effects& effects,
                                          const shell_effect_context& context);
shell_effect_result apply_public_profile_effects(const public_profile::effects& effects,
                                                 const shell_effect_context& context);

}  // namespace title
