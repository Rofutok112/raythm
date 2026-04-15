#pragma once

#include <string>
#include <vector>

#include "data_models.h"

namespace ranking_service {

enum class source {
    local,
    online,
};

struct entry {
    int placement = 0;
    std::string player_display_name;
    float accuracy = 0.0f;
    bool is_full_combo = false;
    int max_combo = 0;
    int score = 0;
    std::string recorded_at;
    bool verified = false;

    rank clear_rank() const { return compute_rank(accuracy, is_full_combo); }
};

struct listing {
    source ranking_source = source::local;
    std::vector<entry> entries;
    std::string message;
    bool available = true;
};

listing load_chart_ranking(const std::string& chart_id, source ranking_source, int limit = 50);
bool submit_local_result(const chart_meta& chart, const result_data& result);

}  // namespace ranking_service
