#pragma once

#include <string>

#include "network/friend_client.h"
#include "title/title_friends_state.h"

namespace title_friends_reducer {

struct apply_result {
    bool applied = false;
    std::string message;
};

apply_result apply_friend_listing(title_friends_state::social_state& state,
                                  const friend_client::friend_listing_result& result);
apply_result apply_request_listing(title_friends_state::social_state& state,
                                   const friend_client::request_listing_result& result);
apply_result apply_invite_listing(title_friends_state::social_state& state,
                                  const friend_client::invite_listing_result& result);

}  // namespace title_friends_reducer
