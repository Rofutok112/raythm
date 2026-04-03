#include "score_system.h"

#include <algorithm>

namespace {
constexpr int kMaxScore = 1000000;
constexpr int kPerfectBase = 1000;
constexpr int kGreatBase = 800;
constexpr int kGoodBase = 500;
constexpr int kBadBase = 200;
constexpr int kMissBase = 0;

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

int base_score(judge_result result) {
    switch (result) {
        case judge_result::perfect:
            return kPerfectBase;
        case judge_result::great:
            return kGreatBase;
        case judge_result::good:
            return kGoodBase;
        case judge_result::bad:
            return kBadBase;
        case judge_result::miss:
            return kMissBase;
    }

    return 0;
}
}

void score_system::init(int total_notes) {
    total_notes_ = std::max(total_notes, 0);
    score_ = 0;
    combo_ = 0;
    max_combo_ = 0;
    judge_counts_.fill(0);
    fast_count_ = 0;
    slow_count_ = 0;
    offset_sum_ = 0.0;
    judged_notes_ = 0;
}

void score_system::on_judge(const judge_event& event) {
    ++judge_counts_[judge_index(event.result)];
    ++judged_notes_;

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

    const int raw = base_score(event.result);
    const double combo_bonus = 1.0 + std::min(combo_, 100) * 0.002;
    score_ += static_cast<int>(raw * combo_bonus);
}

int score_system::get_score() const {
    return score_;
}

int score_system::get_combo() const {
    return combo_;
}

float score_system::get_live_accuracy() const {
    if (judged_notes_ <= 0) {
        return 0.0f;
    }

    const double max_achievement_points = static_cast<double>(judged_notes_ * kPerfectBase);
    const double earned_achievement_points =
        judge_counts_[judge_index(judge_result::perfect)] * kPerfectBase +
        judge_counts_[judge_index(judge_result::great)] * kGreatBase +
        judge_counts_[judge_index(judge_result::good)] * kGoodBase +
        judge_counts_[judge_index(judge_result::bad)] * kBadBase;
    return static_cast<float>((earned_achievement_points / max_achievement_points) * 100.0);
}

result_data score_system::get_result_data() const {
    result_data result;
    result.score = score_;
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

    if (result.accuracy >= 99.0f) {
        result.clear_rank = rank::ss;
    } else if (result.accuracy >= 95.0f) {
        result.clear_rank = rank::s;
    } else if (result.accuracy >= 85.0f) {
        result.clear_rank = rank::a;
    } else if (result.accuracy >= 70.0f) {
        result.clear_rank = rank::b;
    } else if (result.accuracy >= 50.0f) {
        result.clear_rank = rank::c;
    } else {
        result.clear_rank = rank::f;
    }

    if (total_notes_ > 0) {
        const double normalized = static_cast<double>(score_) /
                                  static_cast<double>(total_notes_ * static_cast<int>(kPerfectBase * 1.2));
        result.score = static_cast<int>(std::clamp(normalized, 0.0, 1.0) * kMaxScore);
    }

    return result;
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
