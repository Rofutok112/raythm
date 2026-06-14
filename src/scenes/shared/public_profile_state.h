#pragma once

#include "network/auth_client.h"

namespace public_profile_state {

enum class relationship_action {
    none,
    send_request,
    accept_request,
    remove_friend,
    unblock,
};

struct relationship_action_view {
    relationship_action action = relationship_action::none;
    const char* label = "N/A";
    bool enabled = false;
};

relationship_action_view relationship_action_for(const auth::public_profile& profile, bool active);

}  // namespace public_profile_state
