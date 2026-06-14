#include "shared/public_profile_service.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

namespace public_profile {
namespace {

template <typename Future>
bool ready(Future& future) {
    return future.valid() && future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

template <typename Result, typename Fn>
std::future<Result> start_task(Fn task) {
    std::promise<Result> promise;
    std::future<Result> future = promise.get_future();
    std::thread([promise = std::move(promise), task = std::move(task)]() mutable {
        try {
            promise.set_value(task());
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
    return future;
}

}  // namespace

void service::reset() {
    load_future_ = {};
    relationship_future_ = {};
}

bool service::request_load(std::string user_id) {
    if (user_id.empty() || loading_pending()) {
        return false;
    }
    load_future_ = start_task<auth::public_profile_result>([user_id = std::move(user_id)] {
        return auth::fetch_public_profile(user_id);
    });
    return true;
}

bool service::start_relationship_action(auth::public_profile profile) {
    if (relationship_pending() || profile.id.empty()) {
        return false;
    }
    const public_profile_state::relationship_action action =
        public_profile_state::relationship_action_for(profile, false).action;
    relationship_future_ = start_task<friend_client::operation_result>(
        [profile = std::move(profile), action]() -> friend_client::operation_result {
            if (action == public_profile_state::relationship_action::remove_friend) {
                return friend_client::remove_friend(profile.id);
            }
            if (action == public_profile_state::relationship_action::accept_request) {
                return friend_client::accept_friend_request(profile.relationship_request_id);
            }
            if (action == public_profile_state::relationship_action::unblock) {
                return friend_client::unblock_user(profile.id);
            }
            return friend_client::send_friend_request(profile.id);
        });
    return true;
}

std::vector<service::event> service::poll() {
    std::vector<event> events;
    try {
        if (ready(load_future_)) {
            events.push_back({
                .type = event_type::profile_loaded,
                .profile = load_future_.get(),
            });
        }
        if (ready(relationship_future_)) {
            events.push_back({
                .type = event_type::relationship_completed,
                .relationship = relationship_future_.get(),
            });
        }
    } catch (const std::exception& ex) {
        events.push_back({
            .type = event_type::service_error,
            .message = ex.what(),
        });
    } catch (...) {
        events.push_back({
            .type = event_type::service_error,
            .message = "Public profile operation failed.",
        });
    }
    return events;
}

bool service::loading_pending() const {
    return load_future_.valid();
}

bool service::relationship_pending() const {
    return relationship_future_.valid();
}

}  // namespace public_profile
