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

struct submit_response {
    bool available = true;
    bool updated = false;
    std::string message;
    std::optional<ranking_service::entry> entry;
};

struct submit_operation_result {
    bool success = false;
    bool unauthorized = false;
    std::string message;
    std::optional<submit_response> submission;
};

operation_result fetch_chart_ranking(const std::string& server_url,
                                     const std::string& access_token,
                                     const std::string& chart_id,
                                     int limit = 50);

submit_operation_result submit_chart_ranking(const std::string& server_url,
                                             const std::string& access_token,
                                             const std::string& chart_id,
                                             const ranking_service::entry& entry);

}  // namespace ranking_client
