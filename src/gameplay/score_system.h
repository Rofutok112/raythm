#pragma once

#include <array>

#include "data_models.h"

class score_system {
public:
    void init(int total_notes);
    void on_judge(const judge_event& event);
    int get_score() const;
    int get_combo() const;
    result_data get_result_data() const;

private:
    int score_ = 0;
    int combo_ = 0;
    int max_combo_ = 0;
    std::array<int, 5> judge_counts_ = {};
    int total_notes_ = 0;
    int fast_count_ = 0;
    int slow_count_ = 0;
    double offset_sum_ = 0.0;
    int judged_notes_ = 0;
};

class gauge {
public:
    void on_judge(judge_result result);
    float get_value() const;
    bool is_cleared() const;

private:
    float value_ = 100.0f;
};
