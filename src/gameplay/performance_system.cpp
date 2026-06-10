#include "performance_system.h"

#include <algorithm>
#include <cmath>

#include "chart_difficulty.h"
#include "chart_rc_calculator.h"

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

}  // namespace

void performance_system::init(const chart_data& chart, const timing_engine& engine) {
    reset();

    const std::vector<chart_difficulty::event_difficulty> event_difficulties =
        chart_difficulty::calculate_event_difficulties(chart, engine);

    int max_event_index = -1;
    for (const chart_difficulty::event_difficulty& event : event_difficulties) {
        max_event_index = std::max(max_event_index, event.event_index);
    }

    if (max_event_index < 0) {
        return;
    }

    event_weights_.assign(static_cast<size_t>(max_event_index + 1), 0.0f);
    for (const chart_difficulty::event_difficulty& event : event_difficulties) {
        if (event.event_index < 0) {
            continue;
        }
        event_weights_[static_cast<size_t>(event.event_index)] =
            chart_rc::event_weight_for(event.local_difficulty);
    }

    for (float weight : event_weights_) {
        total_possible_weight_ += weight;
    }
    const size_t positive_event_count =
        static_cast<size_t>(std::count_if(event_weights_.begin(), event_weights_.end(), [](float weight) {
            return weight > 0.0f;
        }));
    chart_max_rc_ = chart_rc::max_rc_for(chart, positive_event_count);
}

void performance_system::reset() {
    event_weights_.clear();
    total_possible_weight_ = 0.0f;
    judged_possible_weight_ = 0.0f;
    earned_weight_ = 0.0f;
    chart_max_rc_ = 0.0f;
    judged_events_ = 0;
    combo_ = 0;
    max_combo_ = 0;
    judge_counts_.fill(0);
    cached_current_rc_ = 0.0f;
}

void performance_system::on_judge(const judge_event& event) {
    if (event.event_index < 0 ||
        static_cast<size_t>(event.event_index) >= event_weights_.size()) {
        return;
    }

    const float possible = event_weights_[static_cast<size_t>(event.event_index)];
    if (possible <= 0.0f) {
        return;
    }
    judged_possible_weight_ += possible;
    earned_weight_ += possible * judge_factor(event.result);
    ++judged_events_;
    ++judge_counts_[judge_index(event.result)];

    if (event.result == judge_result::bad || event.result == judge_result::miss) {
        combo_ = 0;
    } else {
        ++combo_;
        max_combo_ = std::max(max_combo_, combo_);
    }

    cached_current_rc_ = calculate_current_rc();
}

float performance_system::current_rc() const {
    return cached_current_rc_;
}

float performance_system::max_rc() const {
    return chart_max_rc_;
}

float performance_system::weighted_accuracy() const {
    if (judged_possible_weight_ <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(earned_weight_ / judged_possible_weight_, 0.0f, 1.0f);
}

float performance_system::calculate_current_rc() const {
    if (chart_max_rc_ <= 0.0f || total_possible_weight_ <= 0.0f) {
        return 0.0f;
    }

    const float strain_progress = std::clamp(earned_weight_ / total_possible_weight_, 0.0f, 1.0f);
    const float acc = weighted_accuracy();
    const float accuracy_factor = 0.55f + 0.45f * std::pow(acc, 2.8f);

    const float miss_like =
        static_cast<float>(judge_counts_[judge_index(judge_result::miss)]) +
        0.5f * static_cast<float>(judge_counts_[judge_index(judge_result::bad)]);
    const float miss_penalty = std::pow(0.965f, miss_like);

    float combo_factor = 1.0f;
    if (miss_like > 0.0f && judged_events_ > 0) {
        const float combo_ratio =
            std::clamp(static_cast<float>(max_combo_) / static_cast<float>(judged_events_), 0.0f, 1.0f);
        combo_factor = 0.78f + 0.22f * std::sqrt(combo_ratio);
    }

    return std::round(chart_max_rc_ * strain_progress * accuracy_factor * miss_penalty * combo_factor);
}

float performance_system::judge_factor(judge_result result) const {
    switch (result) {
        case judge_result::perfect:
            return 1.0f;
        case judge_result::great:
            return 0.82f;
        case judge_result::good:
            return 0.45f;
        case judge_result::bad:
            return 0.12f;
        case judge_result::miss:
            return 0.0f;
    }

    return 0.0f;
}
