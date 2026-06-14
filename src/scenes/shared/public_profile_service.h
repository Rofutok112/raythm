#pragma once

#include <future>
#include <string>
#include <vector>

#include "network/auth_client.h"
#include "network/friend_client.h"
#include "shared/public_profile_state.h"

namespace public_profile {

class service {
public:
    enum class event_type {
        profile_loaded,
        relationship_completed,
        service_error,
    };

    struct event {
        event_type type = event_type::service_error;
        auth::public_profile_result profile;
        friend_client::operation_result relationship;
        std::string message;
    };

    void reset();
    [[nodiscard]] bool request_load(std::string user_id);
    [[nodiscard]] bool start_relationship_action(auth::public_profile profile);
    [[nodiscard]] std::vector<event> poll();
    [[nodiscard]] bool loading_pending() const;
    [[nodiscard]] bool relationship_pending() const;

private:
    std::future<auth::public_profile_result> load_future_;
    std::future<friend_client::operation_result> relationship_future_;
};

}  // namespace public_profile
