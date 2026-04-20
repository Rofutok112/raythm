#pragma once

#include <array>
#include <vector>

#include "data_models.h"
#include "scoring_ruleset_runtime.h"

class score_system {
public:
    void init(int total_notes);
    void on_judge(const judge_event& event);
    int get_score() const;
    int get_combo() const;
    float get_live_accuracy() const;
    result_data get_result_data() const;

private:
    int normalized_score() const;

    double raw_score_ = 0.0;
    int combo_ = 0;
    int max_combo_ = 0;
    std::array<int, 5> judge_counts_ = {};
    int total_notes_ = 0;
    int fast_count_ = 0;
    int slow_count_ = 0;
    double offset_sum_ = 0.0;
    int judged_notes_ = 0;
    std::vector<note_result_entry> note_results_;
    scoring_ruleset_runtime::ruleset ruleset_ = scoring_ruleset_runtime::make_default_ruleset();
};

class gauge {
public:
    void on_judge(judge_result result);
    float get_value() const;
    bool is_cleared() const;

private:
    float value_ = 100.0f;
};
