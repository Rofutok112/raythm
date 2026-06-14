#include "title/title_friends_service.h"

#include <chrono>
#include <exception>
#include <thread>
#include <utility>

#include "network/auth_client.h"

namespace {

constexpr float kSocialReconnectSeconds = 8.0f;
constexpr float kSocialPingSeconds = 25.0f;

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

void title_friends_service::reset() {
    friends_future_ = {};
    requests_future_ = {};
    invites_future_ = {};
    operation_future_ = {};
    invite_operation_future_ = {};
    if (social_realtime_) {
        social_realtime_->close();
    }
    social_realtime_.reset();
    social_reconnect_t_ = 0.0f;
    social_ping_t_ = 0.0f;
}

void title_friends_service::tick(float dt) {
    const auth::session_summary session = auth::load_session_summary();
    if (!session.logged_in) {
        if (social_realtime_) {
            social_realtime_->close();
            social_realtime_.reset();
        }
        social_reconnect_t_ = 0.0f;
        social_ping_t_ = 0.0f;
        return;
    }

    social_reconnect_t_ += dt;
    social_ping_t_ += dt;
    if (!social_realtime_ || !social_realtime_->connected()) {
        if (!social_realtime_ || social_reconnect_t_ >= kSocialReconnectSeconds) {
            ensure_social_realtime();
        }
    } else if (social_ping_t_ >= kSocialPingSeconds) {
        social_ping_t_ = 0.0f;
        (void)social_realtime_->send_ping();
    }
}

std::vector<title_friends_service::event> title_friends_service::poll() {
    std::vector<event> events;
    poll_social_realtime(events);
    try {
        if (ready(friends_future_)) {
            events.push_back({
                .type = event_type::friend_listing_loaded,
                .friends = friends_future_.get(),
            });
        }
        if (ready(requests_future_)) {
            events.push_back({
                .type = event_type::request_listing_loaded,
                .requests = requests_future_.get(),
            });
        }
        if (ready(invites_future_)) {
            events.push_back({
                .type = event_type::invite_listing_loaded,
                .invites = invites_future_.get(),
            });
        }
        if (ready(operation_future_)) {
            events.push_back({
                .type = event_type::operation_completed,
                .operation = operation_future_.get(),
            });
        }
        if (ready(invite_operation_future_)) {
            events.push_back({
                .type = event_type::invite_operation_completed,
                .invite_operation = invite_operation_future_.get(),
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
            .message = "Friends operation failed.",
        });
    }
    return events;
}

bool title_friends_service::request_reload() {
    if (loading_pending()) {
        return false;
    }
    friends_future_ = start_task<friend_client::friend_listing_result>([] {
        return friend_client::fetch_friends();
    });
    requests_future_ = start_task<friend_client::request_listing_result>([] {
        return friend_client::fetch_friend_requests();
    });
    invites_future_ = start_task<friend_client::invite_listing_result>([] {
        return friend_client::fetch_room_invites();
    });
    return true;
}

bool title_friends_service::accept_request(std::string request_id) {
    if (operation_pending()) {
        return false;
    }
    return start_operation(start_task<friend_client::operation_result>([request_id = std::move(request_id)] {
        return friend_client::accept_friend_request(request_id);
    }));
}

bool title_friends_service::decline_request(std::string request_id) {
    if (operation_pending()) {
        return false;
    }
    return start_operation(start_task<friend_client::operation_result>([request_id = std::move(request_id)] {
        return friend_client::decline_friend_request(request_id);
    }));
}

bool title_friends_service::remove_friend(std::string user_id) {
    if (user_id.empty()) {
        return false;
    }
    if (operation_pending()) {
        return false;
    }
    return start_operation(start_task<friend_client::operation_result>([user_id = std::move(user_id)] {
        return friend_client::remove_friend(user_id);
    }));
}

bool title_friends_service::block_user(std::string user_id) {
    if (user_id.empty()) {
        return false;
    }
    if (operation_pending()) {
        return false;
    }
    return start_operation(start_task<friend_client::operation_result>([user_id = std::move(user_id)] {
        return friend_client::block_user(user_id);
    }));
}

bool title_friends_service::accept_invite(std::string invite_id) {
    if (operation_pending()) {
        return false;
    }
    return start_invite_operation(
        start_task<friend_client::invite_operation_result>([invite_id = std::move(invite_id)] {
            return friend_client::accept_room_invite(invite_id);
        }));
}

bool title_friends_service::read_invite(std::string invite_id) {
    if (operation_pending()) {
        return false;
    }
    return start_operation(start_task<friend_client::operation_result>([invite_id = std::move(invite_id)] {
        return friend_client::read_room_invite(invite_id);
    }));
}

bool title_friends_service::decline_invite(std::string invite_id) {
    if (operation_pending()) {
        return false;
    }
    return start_operation(start_task<friend_client::operation_result>([invite_id = std::move(invite_id)] {
        return friend_client::decline_room_invite(invite_id);
    }));
}

bool title_friends_service::loading_pending() const {
    return friends_future_.valid() || requests_future_.valid() || invites_future_.valid();
}

bool title_friends_service::operation_pending() const {
    return operation_future_.valid() || invite_operation_future_.valid();
}

void title_friends_service::ensure_social_realtime() {
    social_reconnect_t_ = 0.0f;
    if (!social_realtime_) {
        social_realtime_ = std::make_unique<friend_client::social_realtime_client>();
    }
    if (!social_realtime_->connected()) {
        (void)social_realtime_->connect();
    }
}

void title_friends_service::poll_social_realtime(std::vector<event>& events) {
    if (!social_realtime_ || !social_realtime_->connected()) {
        return;
    }
    for (const friend_client::social_realtime_event& social_event : social_realtime_->poll_events()) {
        events.push_back({
            .type = event_type::social_event,
            .social = social_event,
        });
    }
}

bool title_friends_service::start_operation(std::future<friend_client::operation_result> future) {
    if (operation_pending()) {
        return false;
    }
    operation_future_ = std::move(future);
    return true;
}

bool title_friends_service::start_invite_operation(std::future<friend_client::invite_operation_result> future) {
    if (operation_pending()) {
        return false;
    }
    invite_operation_future_ = std::move(future);
    return true;
}
