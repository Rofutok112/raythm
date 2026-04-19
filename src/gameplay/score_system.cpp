#include "score_system.h"

#include <algorithm>

namespace {
size_t judge_index(judge_result result) {
    switch (result) {
        case judge_result::perfect:
            return 0;
        case judge_result::great:
            return 1;
        case judge_result::good:
            return 2;
        case judge_result::bad:
            return 3;
        case judge_result::miss:
            return 4;
    }

    return 4;
}

double combo_score_multiplier(int combo, int total_notes) {
    if (total_notes <= 0) {
        return 1.0;
    }

    const double progress = std::clamp(static_cast<double>(combo) / static_cast<double>(total_notes), 0.0, 1.0);
    return progress * progress;
}

double max_raw_score_for_total_notes(const scoring_ruleset_runtime::ruleset& ruleset, int total_notes) {
    if (total_notes <= 0) {
        return 0.0;
    }

    double max_score = 0.0;
    for (int combo = 1; combo <= total_notes; ++combo) {
        max_score += static_cast<double>(
            scoring_ruleset_runtime::judge_value_for(ruleset, judge_result::perfect)) *
            combo_score_multiplier(combo, total_notes);
    }
    return max_score;
}
}

void score_system::init(int total_notes) {
    ruleset_ = scoring_ruleset_runtime::current_ruleset();
    total_notes_ = std::max(total_notes, 0);
    raw_score_ = 0.0;
    combo_ = 0;
    max_combo_ = 0;
    judge_counts_.fill(0);
    fast_count_ = 0;
    slow_count_ = 0;
    offset_sum_ = 0.0;
    judged_notes_ = 0;
    note_results_.clear();
    note_results_.reserve(static_cast<size_t>(total_notes_));
}

void score_system::on_judge(const judge_event& event) {
    ++judge_counts_[judge_index(event.result)];
    ++judged_notes_;
    if (event.event_index >= 0) {
        note_results_.push_back(note_result_entry{
            .event_index = event.event_index,
            .result = event.result,
            .offset_ms = event.offset_ms,
        });
    }

    if (event.offset_ms < 0.0) {
        ++fast_count_;
    } else if (event.offset_ms > 0.0) {
        ++slow_count_;
    }
    offset_sum_ += event.offset_ms;

    if (event.result == judge_result::bad || event.result == judge_result::miss) {
        combo_ = 0;
    } else {
        ++combo_;
        max_combo_ = std::max(max_combo_, combo_);
    }

    const int raw = scoring_ruleset_runtime::judge_value_for(ruleset_, event.result);
    raw_score_ += static_cast<double>(raw) * combo_score_multiplier(combo_, total_notes_);
}

int score_system::get_score() const {
    return normalized_score();
}

int score_system::get_combo() const {
    return combo_;
}

float score_system::get_live_accuracy() const {
    if (judged_notes_ <= 0) {
        return 0.0f;
    }

    const int perfect_value = scoring_ruleset_runtime::judge_value_for(ruleset_, judge_result::perfect);
    const double max_achievement_points = static_cast<double>(judged_notes_ * perfect_value);
    const double earned_achievement_points =
        judge_counts_[judge_index(judge_result::perfect)] * scoring_ruleset_runtime::judge_value_for(ruleset_, judge_result::perfect) +
        judge_counts_[judge_index(judge_result::great)] * scoring_ruleset_runtime::judge_value_for(ruleset_, judge_result::great) +
        judge_counts_[judge_index(judge_result::good)] * scoring_ruleset_runtime::judge_value_for(ruleset_, judge_result::good) +
        judge_counts_[judge_index(judge_result::bad)] * scoring_ruleset_runtime::judge_value_for(ruleset_, judge_result::bad);
    return static_cast<float>((earned_achievement_points / max_achievement_points) * 100.0);
}

result_data score_system::get_result_data() const {
    result_data result;
    result.score = normalized_score();
    result.judge_counts = judge_counts_;
    result.max_combo = max_combo_;
    result.avg_offset = judged_notes_ > 0 ? static_cast<float>(offset_sum_ / static_cast<double>(judged_notes_)) : 0.0f;
    result.fast_count = fast_count_;
    result.slow_count = slow_count_;
    result.is_full_combo = judge_counts_[judge_index(judge_result::bad)] == 0 &&
                           judge_counts_[judge_index(judge_result::miss)] == 0;
    result.is_all_perfect = judged_notes_ > 0 &&
                            judge_counts_[judge_index(judge_result::perfect)] == judged_notes_;
    result.accuracy = get_live_accuracy();
    result.note_results = note_results_;

    result.clear_rank = scoring_ruleset_runtime::compute_rank_for(ruleset_, result.accuracy, result.is_full_combo);

    return result;
}

int score_system::normalized_score() const {
    const double max_raw_score = max_raw_score_for_total_notes(ruleset_, total_notes_);
    if (max_raw_score <= 0.0) {
        return 0;
    }

    const double normalized = raw_score_ / max_raw_score;
    return static_cast<int>(std::clamp(normalized, 0.0, 1.0) * ruleset_.max_score);
}

void gauge::on_judge(judge_result result) {
    switch (result) {
        case judge_result::perfect:
            value_ += 2.0f;
            break;
        case judge_result::great:
            value_ += 1.0f;
            break;
        case judge_result::good:
            break;
        case judge_result::bad:
            value_ -= 6.0f;
            break;
        case judge_result::miss:
            value_ -= 10.0f;
            break;
    }

    value_ = std::clamp(value_, 0.0f, 100.0f);
}

float gauge::get_value() const {
    return value_;
}

bool gauge::is_cleared() const {
    return value_ >= 70.0f;
}
