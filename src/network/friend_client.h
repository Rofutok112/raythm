#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ranking_client.h"

namespace friend_client {

struct social_user {
    std::string id;
    std::string display_name;
    std::string avatar_url;
    std::string relationship_status;
    std::string online_status;
    std::string current_room_id;
    std::string current_room_name;
};

struct friend_request {
    std::string id;
    std::string direction;
    std::string status;
    std::string created_at;
    std::string updated_at;
    social_user requester;
    social_user addressee;
};

struct room_invite {
    std::string id;
    std::string room_id;
    std::string room_name;
    social_user sender;
    std::string status;
    bool read = false;
    std::string expires_at;
    std::string created_at;
};

struct friend_listing {
    std::vector<social_user> friends;
    int pending_request_count = 0;
    int unread_invite_count = 0;
};

struct request_listing {
    std::vector<friend_request> incoming;
    std::vector<friend_request> outgoing;
};

struct invite_listing {
    std::vector<room_invite> invites;
};

struct operation_result {
    bool success = false;
    bool unauthorized = false;
    bool maintenance = false;
    std::string message;
    std::string retry_after;
};

struct friend_listing_result : operation_result {
    std::optional<friend_listing> listing;
};

struct request_listing_result : operation_result {
    std::optional<request_listing> listing;
};

struct invite_listing_result : operation_result {
    std::optional<invite_listing> listing;
};

struct request_operation_result : operation_result {
    std::optional<friend_request> request;
};

struct invite_operation_result : operation_result {
    std::optional<room_invite> invite;
    std::string join_room_id;
    std::string join_invite_id;
};

friend_listing_result fetch_friends();
request_listing_result fetch_friend_requests();
request_operation_result send_friend_request(const std::string& target_user_id);
request_operation_result accept_friend_request(const std::string& request_id);
request_operation_result decline_friend_request(const std::string& request_id);
operation_result remove_friend(const std::string& user_id);
operation_result block_user(const std::string& user_id);
operation_result unblock_user(const std::string& user_id);

invite_listing_result fetch_room_invites();
invite_operation_result send_room_invite(const std::string& room_id, const std::string& recipient_user_id);
invite_operation_result accept_room_invite(const std::string& invite_id);
invite_operation_result decline_room_invite(const std::string& invite_id);
invite_operation_result read_room_invite(const std::string& invite_id);

ranking_client::operation_result fetch_friend_chart_ranking(const std::string& chart_id, int limit = 50);

}  // namespace friend_client
