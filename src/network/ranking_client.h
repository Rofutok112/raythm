#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ranking_service.h"

namespace ranking_client {

struct listing_response {
    bool available = true;
    std::vector<ranking_service::entry> entries;
    std::string message;
};

struct operation_result {
    bool success = false;
    bool unauthorized = false;
    std::string message;
    std::optional<listing_response> listing;
};

operation_result fetch_chart_ranking(const std::string& server_url,
                                     const std::string& access_token,
                                     const std::string& chart_id,
                                     int limit = 50);

}  // namespace ranking_client
