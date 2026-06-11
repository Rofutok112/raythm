#pragma once

#include <optional>
#include <string>
#include <vector>

#include "data_models.h"

namespace ranking_service {

enum class source {
    local,
    online,
    friends,
};

struct entry {
    int placement = 0;
    std::string player_user_id;
    std::string player_display_name;
    std::string player_avatar_url;
    float accuracy = 0.0f;
    bool is_full_combo = false;
    int max_combo = 0;
    int score = 0;
    std::string recorded_at;
    bool verified = false;
    rank resolved_clear_rank = rank::f;

    rank clear_rank() const { return resolved_clear_rank; }
};

struct listing {
    source ranking_source = source::local;
    std::vector<entry> entries;
    std::string message;
    std::string retry_after;
    bool available = true;
    bool maintenance = false;
};

struct local_submit_result {
    bool success = false;
    bool best_updated = false;
    std::optional<entry> submitted_entry;
    std::optional<entry> previous_best;
};

struct online_submit_result {
    bool attempted = false;
    bool success = false;
    bool updated = false;
    bool maintenance = false;
    std::string message;
    std::string retry_after;
    std::optional<entry> entry;
    std::optional<ranking_service::entry> previous_entry;
};

listing load_chart_ranking(const std::string& chart_id, source ranking_source, int limit = 50);
std::optional<entry> load_chart_personal_best(const std::string& chart_id, source ranking_source);
std::string make_recorded_at_timestamp();
local_submit_result submit_local_result_detailed(const chart_meta& chart,
                                                 const result_data& result,
                                                 const std::string& recorded_at = {});
bool submit_local_result(const chart_meta& chart, const result_data& result);
bool should_attempt_online_submit(const local_submit_result& local_result);
bool should_attempt_online_submit(const chart_meta& chart, const result_data& result);
bool warm_scoring_ruleset_cache(bool force_refresh = false);
bool refresh_scoring_ruleset_cache_for_chart_start(const chart_meta& chart, bool force_refresh = true);
online_submit_result submit_online_result(const song_data& song,
                                          const std::string& chart_path,
                                          const chart_meta& chart,
                                          const result_data& result,
                                          const std::string& recorded_at);

}  // namespace ranking_service
