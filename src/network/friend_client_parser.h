#pragma once

#include <optional>
#include <string>

#include "network/friend_client.h"
#include "network/http_client.h"

namespace friend_client::detail {

std::optional<friend_listing_result> parse_friend_listing_response(const network::http::response& response);
std::optional<request_listing_result> parse_request_listing_response(const network::http::response& response);
std::optional<invite_listing_result> parse_invite_listing_response(const network::http::response& response);
std::optional<request_operation_result> parse_request_operation_response(const network::http::response& response);
std::optional<invite_operation_result> parse_invite_operation_response(const network::http::response& response);
social_realtime_event parse_social_event_message(const std::string& message);

}  // namespace friend_client::detail
