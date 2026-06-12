#include "shared/public_profile_state.h"

namespace public_profile_state {

relationship_action_view relationship_action_for(const auth::public_profile& profile, bool active) {
    if (active) {
        return {
            .action = relationship_action::none,
            .label = "WORKING",
            .enabled = false,
        };
    }
    if (profile.relationship_status == "none") {
        return {
            .action = relationship_action::send_request,
            .label = "ADD",
            .enabled = true,
        };
    }
    if (profile.relationship_status == "pending_incoming") {
        const bool can_accept = !profile.relationship_request_id.empty();
        return {
            .action = can_accept ? relationship_action::accept_request : relationship_action::none,
            .label = "ACCEPT",
            .enabled = can_accept,
        };
    }
    if (profile.relationship_status == "pending_outgoing") {
        return {
            .action = relationship_action::none,
            .label = "PENDING",
            .enabled = false,
        };
    }
    if (profile.relationship_status == "accepted") {
        return {
            .action = relationship_action::remove_friend,
            .label = "REMOVE",
            .enabled = true,
        };
    }
    if (profile.relationship_status == "blocked") {
        return {
            .action = relationship_action::unblock,
            .label = "UNBLOCK",
            .enabled = true,
        };
    }
    if (profile.relationship_status == "self") {
        return {
            .action = relationship_action::none,
            .label = "SELF",
            .enabled = false,
        };
    }
    if (profile.relationship_status == "unavailable") {
        return {
            .action = relationship_action::none,
            .label = "UNAVAILABLE",
            .enabled = false,
        };
    }
    return {};
}

}  // namespace public_profile_state
