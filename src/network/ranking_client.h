#pragma once

#include <optional>
#include <string>
#include <vector>

#include "scoring_ruleset_runtime.h"
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

using scoring_ruleset = scoring_ruleset_runtime::ruleset;

struct scoring_ruleset_operation_result {
    bool success = false;
    std::string message;
    std::optional<scoring_ruleset> ruleset;
};

struct official_manifest {
    bool available = false;
    std::string message;
    std::string chart_id;
    std::string song_id;
    std::string song_json_sha256;
    std::string audio_sha256;
    std::string jacket_sha256;
    std::string chart_sha256;
};

struct manifest_operation_result {
    bool success = false;
    std::string message;
    std::optional<official_manifest> manifest;
};

operation_result fetch_chart_ranking(const std::string& server_url,
                                     const std::string& access_token,
                                     const std::string& chart_id,
                                     int limit = 50);

submit_operation_result submit_chart_ranking(const std::string& server_url,
                                             const std::string& access_token,
                                             const std::string& chart_id,
                                             const result_data& result,
                                             const std::string& recorded_at,
                                             const std::string& ruleset_version);

scoring_ruleset_operation_result fetch_scoring_ruleset(const std::string& server_url);

manifest_operation_result fetch_official_chart_manifest(const std::string& server_url,
                                                        const std::string& chart_id);

}  // namespace ranking_client
