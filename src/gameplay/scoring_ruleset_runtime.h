#pragma once

#include <array>
#include <string>
#include <vector>

#include "data_models.h"

namespace scoring_ruleset_runtime {

struct rank_threshold {
    rank clear_rank = rank::f;
    float min_accuracy = 0.0f;
    bool requires_full_combo = false;
};

struct ruleset {
    bool active = true;
    std::string accepted_input = "note_results_v1";
    std::string ruleset_version = "local-default";
    std::string score_model = "combo-progress-squared";
    int max_score = 1'000'000;
    std::array<int, 5> judge_values = {1000, 800, 500, 200, 0};
    std::vector<rank_threshold> rank_thresholds;
};

struct computed_result {
    int score = 0;
    float accuracy = 0.0f;
    bool is_full_combo = false;
    int max_combo = 0;
    rank clear_rank = rank::f;
    std::array<int, 5> judge_counts = {};
};

ruleset make_default_ruleset();
ruleset current_ruleset();
void apply_server_ruleset(const ruleset& ruleset_data);

int judge_value_for(const ruleset& ruleset_data, judge_result result);
double score_multiplier_for(const ruleset& ruleset_data, int combo, int total_notes);
rank compute_rank_for(const ruleset& ruleset_data, float accuracy, bool is_full_combo);
computed_result compute_result_for(const ruleset& ruleset_data,
                                   const std::vector<note_result_entry>& note_results);

}  // namespace scoring_ruleset_runtime
