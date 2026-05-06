#include "performance_system.h"

#include <algorithm>
#include <cmath>

#include "chart_difficulty.h"

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

float event_weight_for(float local_difficulty) {
    return std::pow(std::max(local_difficulty, 0.001f), 1.12f);
}

float chart_max_pp_for(const chart_data& chart, size_t event_count) {
    const float level = chart.meta.level > 0.0f
                            ? chart.meta.level
                            : chart_difficulty::calculate_level(chart);
    if (level <= 0.0f || event_count == 0) {
        return 0.0f;
    }

    const float event_ratio = static_cast<float>(event_count) / 600.0f;
    const float length_bonus =
        0.92f +
        0.08f * std::min(1.0f, event_ratio) +
        (event_count > 600 ? std::min(0.12f, std::log10(event_ratio) * 0.06f) : 0.0f);
    return std::pow(level, 2.25f) * 2.2f * length_bonus;
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
            event_weight_for(event.local_difficulty);
    }

    for (float weight : event_weights_) {
        total_possible_weight_ += weight;
    }
    chart_max_pp_ = chart_max_pp_for(chart, event_weights_.size());
}

void performance_system::reset() {
    event_weights_.clear();
    total_possible_weight_ = 0.0f;
    judged_possible_weight_ = 0.0f;
    earned_weight_ = 0.0f;
    chart_max_pp_ = 0.0f;
    judged_events_ = 0;
    combo_ = 0;
    max_combo_ = 0;
    judge_counts_.fill(0);
    cached_current_pp_ = 0.0f;
}

void performance_system::on_judge(const judge_event& event) {
    if (event.event_index < 0 ||
        static_cast<size_t>(event.event_index) >= event_weights_.size()) {
        return;
    }

    const float possible = event_weights_[static_cast<size_t>(event.event_index)];
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

    cached_current_pp_ = calculate_current_pp();
}

float performance_system::current_pp() const {
    return cached_current_pp_;
}

float performance_system::max_pp() const {
    return chart_max_pp_;
}

float performance_system::weighted_accuracy() const {
    if (judged_possible_weight_ <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(earned_weight_ / judged_possible_weight_, 0.0f, 1.0f);
}

float performance_system::calculate_current_pp() const {
    if (chart_max_pp_ <= 0.0f || total_possible_weight_ <= 0.0f) {
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

    return chart_max_pp_ * strain_progress * accuracy_factor * miss_penalty * combo_factor;
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
