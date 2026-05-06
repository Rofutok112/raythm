#pragma once

#include <array>
#include <vector>

#include "data_models.h"
#include "timing_engine.h"

class performance_system {
public:
    void init(const chart_data& chart, const timing_engine& engine);
    void reset();
    void on_judge(const judge_event& event);

    [[nodiscard]] float current_pp() const;
    [[nodiscard]] float max_pp() const;
    [[nodiscard]] float weighted_accuracy() const;

private:
    [[nodiscard]] float calculate_current_pp() const;
    [[nodiscard]] float judge_factor(judge_result result) const;

    std::vector<float> event_weights_;
    float total_possible_weight_ = 0.0f;
    float judged_possible_weight_ = 0.0f;
    float earned_weight_ = 0.0f;
    float chart_max_pp_ = 0.0f;
    int judged_events_ = 0;
    int combo_ = 0;
    int max_combo_ = 0;
    std::array<int, 5> judge_counts_ = {};
    float cached_current_pp_ = 0.0f;
};
