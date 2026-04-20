#include "scoring_ruleset_runtime.h"

#include <algorithm>
#include <mutex>

namespace {

size_t judge_index(judge_result result) {
    switch (result) {
        case judge_result::perfect: return 0;
        case judge_result::great: return 1;
        case judge_result::good: return 2;
        case judge_result::bad: return 3;
        case judge_result::miss: return 4;
    }

    return 4;
}

std::mutex& ruleset_mutex() {
    static std::mutex mutex;
    return mutex;
}

scoring_ruleset_runtime::ruleset& active_ruleset() {
    static scoring_ruleset_runtime::ruleset ruleset_data =
        scoring_ruleset_runtime::make_default_ruleset();
    return ruleset_data;
}

}  // namespace

namespace scoring_ruleset_runtime {

ruleset make_default_ruleset() {
    ruleset result;
    result.rank_thresholds = {
        {.clear_rank = rank::ss, .min_accuracy = 100.0f, .requires_full_combo = true},
        {.clear_rank = rank::s, .min_accuracy = 95.0f, .requires_full_combo = true},
        {.clear_rank = rank::aa, .min_accuracy = 95.0f, .requires_full_combo = false},
        {.clear_rank = rank::a, .min_accuracy = 90.0f, .requires_full_combo = false},
        {.clear_rank = rank::b, .min_accuracy = 80.0f, .requires_full_combo = false},
        {.clear_rank = rank::c, .min_accuracy = 70.0f, .requires_full_combo = false},
        {.clear_rank = rank::f, .min_accuracy = 0.0f, .requires_full_combo = false},
    };
    return result;
}

ruleset current_ruleset() {
    std::scoped_lock lock(ruleset_mutex());
    return active_ruleset();
}

void apply_server_ruleset(const ruleset& ruleset_data) {
    std::scoped_lock lock(ruleset_mutex());
    active_ruleset() = ruleset_data;
}

int judge_value_for(const ruleset& ruleset_data, judge_result result) {
    return ruleset_data.judge_values[judge_index(result)];
}

rank compute_rank_for(const ruleset& ruleset_data, float accuracy, bool is_full_combo) {
    for (const rank_threshold& threshold : ruleset_data.rank_thresholds) {
        if (accuracy < threshold.min_accuracy) {
            continue;
        }
        if (threshold.requires_full_combo && !is_full_combo) {
            continue;
        }
        return threshold.clear_rank;
    }

    return rank::f;
}

computed_result compute_result_for(const ruleset& ruleset_data,
                                   const std::vector<note_result_entry>& note_results) {
    computed_result result;
    if (note_results.empty()) {
        return result;
    }

    const int perfect_value = judge_value_for(ruleset_data, judge_result::perfect);
    int combo = 0;
    double raw_score = 0.0;
    const int total_notes = static_cast<int>(note_results.size());

    auto combo_score_multiplier = [total_notes](int combo_value) {
        if (total_notes <= 0) {
            return 1.0;
        }
        const double progress = std::clamp(static_cast<double>(combo_value) / static_cast<double>(total_notes), 0.0, 1.0);
        return progress * progress;
    };

    double max_raw_score = 0.0;
    for (int max_combo_index = 1; max_combo_index <= total_notes; ++max_combo_index) {
        max_raw_score += static_cast<double>(perfect_value) * combo_score_multiplier(max_combo_index);
    }

    double earned_achievement_points = 0.0;
    for (const note_result_entry& note_result : note_results) {
        const size_t index = judge_index(note_result.result);
        result.judge_counts[index] += 1;

        if (note_result.result == judge_result::bad || note_result.result == judge_result::miss) {
            combo = 0;
        } else {
            combo += 1;
            result.max_combo = std::max(result.max_combo, combo);
        }

        const int judge_value = judge_value_for(ruleset_data, note_result.result);
        raw_score += static_cast<double>(judge_value) * combo_score_multiplier(combo);
        if (note_result.result != judge_result::miss) {
            earned_achievement_points += static_cast<double>(judge_value);
        }
    }

    const double max_achievement_points = static_cast<double>(total_notes * perfect_value);
    result.accuracy =
        max_achievement_points <= 0.0
            ? 0.0f
            : static_cast<float>((earned_achievement_points / max_achievement_points) * 100.0);
    result.score =
        max_raw_score <= 0.0
            ? 0
            : static_cast<int>(std::clamp(raw_score / max_raw_score, 0.0, 1.0) * ruleset_data.max_score);
    result.is_full_combo =
        result.judge_counts[judge_index(judge_result::bad)] == 0 &&
        result.judge_counts[judge_index(judge_result::miss)] == 0;
    result.clear_rank = compute_rank_for(ruleset_data, result.accuracy, result.is_full_combo);
    return result;
}

}  // namespace scoring_ruleset_runtime
