#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "network/friend_client.h"

class title_friends_service {
public:
    enum class event_type {
        friend_listing_loaded,
        request_listing_loaded,
        invite_listing_loaded,
        operation_completed,
        invite_operation_completed,
        social_event,
        service_error,
    };

    struct event {
        event_type type = event_type::service_error;
        friend_client::friend_listing_result friends;
        friend_client::request_listing_result requests;
        friend_client::invite_listing_result invites;
        friend_client::operation_result operation;
        friend_client::invite_operation_result invite_operation;
        friend_client::social_realtime_event social;
        std::string message;
    };

    void reset();
    void tick(float dt);
    [[nodiscard]] std::vector<event> poll();

    [[nodiscard]] bool request_reload();
    [[nodiscard]] bool accept_request(std::string request_id);
    [[nodiscard]] bool decline_request(std::string request_id);
    [[nodiscard]] bool remove_friend(std::string user_id);
    [[nodiscard]] bool block_user(std::string user_id);
    [[nodiscard]] bool accept_invite(std::string invite_id);
    [[nodiscard]] bool read_invite(std::string invite_id);
    [[nodiscard]] bool decline_invite(std::string invite_id);

    [[nodiscard]] bool loading_pending() const;
    [[nodiscard]] bool operation_pending() const;

private:
    void ensure_social_realtime();
    void poll_social_realtime(std::vector<event>& events);
    [[nodiscard]] bool start_operation(std::future<friend_client::operation_result> future);
    [[nodiscard]] bool start_invite_operation(std::future<friend_client::invite_operation_result> future);

    std::future<friend_client::friend_listing_result> friends_future_;
    std::future<friend_client::request_listing_result> requests_future_;
    std::future<friend_client::invite_listing_result> invites_future_;
    std::future<friend_client::operation_result> operation_future_;
    std::future<friend_client::invite_operation_result> invite_operation_future_;
    std::unique_ptr<friend_client::social_realtime_client> social_realtime_;
    float social_reconnect_t_ = 0.0f;
    float social_ping_t_ = 0.0f;
};
