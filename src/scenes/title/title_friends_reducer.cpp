#include "title/title_friends_reducer.h"

namespace title_friends_reducer {

apply_result apply_friend_listing(title_friends_state::social_state& state,
                                  const friend_client::friend_listing_result& result) {
    if (result.success && result.listing.has_value()) {
        state.friends = *result.listing;
        return {.applied = true};
    }
    return {.message = result.message};
}

apply_result apply_request_listing(title_friends_state::social_state& state,
                                   const friend_client::request_listing_result& result) {
    if (result.success && result.listing.has_value()) {
        state.requests = *result.listing;
        return {.applied = true};
    }
    return {.message = result.message};
}

apply_result apply_invite_listing(title_friends_state::social_state& state,
                                  const friend_client::invite_listing_result& result) {
    if (result.success && result.listing.has_value()) {
        state.invites = *result.listing;
        return {.applied = true};
    }
    return {.message = result.message};
}

}  // namespace title_friends_reducer
