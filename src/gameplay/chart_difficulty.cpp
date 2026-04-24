#include "chart_difficulty.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

#include "timing_engine.h"

namespace {

struct note_event {
    enum class kind {
        tap,
        hold_head,
        hold_tail,
    };

    double time_ms = 0.0;
    int lane = 0;
    kind type = kind::tap;
};

struct hold_interval {
    double start_ms = 0.0;
    double end_ms = 0.0;
    int lane = 0;
};

struct transition_sample {
    double time_ms = 0.0;
    double dt_seconds = 0.0;
    float lane_distance_norm = 0.0f;
    bool same_lane = false;
};

constexpr double kDensityRadiusMs = 500.0;
constexpr double kReleaseRadiusMs = 400.0;
constexpr double kBurstRadiusMs = 300.0;
constexpr double kAverageRadiusMs = 2000.0;
constexpr double kBalanceRadiusMs = 800.0;
constexpr double kReadRadiusMs = 500.0;
constexpr double kStaminaWindowMs = 8000.0;
constexpr double kTimeEpsilonSeconds = 0.015;
constexpr double kChordMergeMs = 2.0;

constexpr float kScale = 12.0f;
constexpr float kPeakPower = 2.2f;
constexpr float kStaminaThreshold = 3.2f;

// 各局所要素の寄与。大きいほど、その要素が最終難易度へ強く効く。
// density: その瞬間の物量
// stream: 連打が続く速さ
// jump: レーン移動を伴う押しづらさ
// hold: 同時に抱えるロングノーツ量
// release: ロングノーツ終点まわりの処理負荷
// overlap: hold 中に別ノーツを処理する複合負荷
// pattern: ジャック・配置の崩れ・局所バーストなどの最大風速寄り要素
// balance: 左右寄りの偏り
// stamina: 数秒単位で高密度が続く持久力負荷
// chord: 同時押しの個数・形・押し分けづらさ
// hand: 片手へ寄る局所密度と回復不足
// hold_conflict: LN 拘束中に同じ手で別ノーツや離しを処理する負荷
// read: 見た目の詰まりや読みづらさ
constexpr float kWeightDensity = 1.50f;
constexpr float kWeightStream = 1.00f;
constexpr float kWeightJump = 0.75f;
constexpr float kWeightHold = 1.00f;
constexpr float kWeightRelease = 1.10f;
constexpr float kWeightOverlap = 1.20f;
constexpr float kWeightPattern = 1.40f;
constexpr float kWeightBalance = 0.35f;
constexpr float kWeightStamina = 0.5f;
constexpr float kWeightChord = 0.28f;
constexpr float kWeightHand = 0.34f;
constexpr float kWeightHoldConflict = 0.42f;
constexpr float kWeightRead = 1.10f;

// 各要素の非線形補正。大きいほど、その要素の「高い値」を強調しやすい。
constexpr float kGammaDensity = 1.50f;
constexpr float kGammaStream = 0.72f;
constexpr float kGammaLane = 1.10f;
constexpr float kGammaJumpTime = 0.55f;
constexpr float kGammaHold = 1.10f;
constexpr float kGammaRelease = 0.85f;
constexpr float kGammaOverlapHold = 1.20f;
constexpr float kGammaJack = 0.95f;
constexpr float kGammaBurst = 1.30f;
constexpr float kGammaBalance = 0.80f;
constexpr float kGammaStamina = 1.10f;
constexpr float kGammaChord = 1.25f;
constexpr float kGammaHand = 0.88f;
constexpr float kGammaHoldConflict = 1.05f;
constexpr float kGammaReadOverlap = 0.70f;
constexpr float kGammaReadTail = 0.60f;

constexpr float kHoldInterfere = 0.60f;
constexpr float kCrossHand = 0.35f;

// 要素どうしの掛け算ボーナス。複合配置のしんどさを少し増幅する。
constexpr float kCouplingHoldDensity = 0.18f;
constexpr float kCouplingHoldJump = 0.22f;
constexpr float kCouplingOverlapJump = 0.16f;
constexpr float kCouplingStaminaStream = 0.14f;
constexpr float kCouplingReadOverlap = 0.10f;
constexpr float kCouplingChordJump = 0.025f;
constexpr float kCouplingHandHold = 0.035f;

template <typename Predicate>
std::vector<const note_event*> collect_events_near(const std::vector<note_event>& events,
                                                   double center_ms,
                                                   double radius_ms,
                                                   Predicate predicate) {
    std::vector<const note_event*> nearby;
    for (const note_event& event : events) {
        if (std::abs(event.time_ms - center_ms) <= radius_ms && predicate(event)) {
            nearby.push_back(&event);
        }
    }
    return nearby;
}

std::vector<note_event> build_note_events(const chart_data& data, timing_engine& engine) {
    std::vector<note_event> events;
    events.reserve(data.notes.size() * 2);

    for (const note_data& note : data.notes) {
        if (note.type == note_type::tap) {
            events.push_back({engine.tick_to_ms(note.tick), note.lane, note_event::kind::tap});
            continue;
        }

        events.push_back({engine.tick_to_ms(note.tick), note.lane, note_event::kind::hold_head});
        events.push_back({engine.tick_to_ms(note.end_tick), note.lane, note_event::kind::hold_tail});
    }

    std::sort(events.begin(), events.end(), [](const note_event& left, const note_event& right) {
        if (left.time_ms != right.time_ms) {
            return left.time_ms < right.time_ms;
        }
        return left.lane < right.lane;
    });
    return events;
}

std::vector<hold_interval> build_hold_intervals(const chart_data& data, timing_engine& engine) {
    std::vector<hold_interval> holds;
    holds.reserve(data.notes.size());
    for (const note_data& note : data.notes) {
        if (note.type != note_type::hold) {
            continue;
        }
        holds.push_back({engine.tick_to_ms(note.tick), engine.tick_to_ms(note.end_tick), note.lane});
    }
    return holds;
}

std::vector<transition_sample> build_transitions(const std::vector<note_event>& events, int key_count) {
    std::vector<transition_sample> transitions;
    if (events.size() < 2 || key_count <= 1) {
        return transitions;
    }

    transitions.reserve(events.size() - 1);
    for (size_t i = 1; i < events.size(); ++i) {
        const note_event& previous = events[i - 1];
        const note_event& current = events[i];
        transitions.push_back({
            current.time_ms,
            std::max((current.time_ms - previous.time_ms) / 1000.0, 0.0),
            static_cast<float>(std::abs(current.lane - previous.lane)) / static_cast<float>(key_count - 1),
            current.lane == previous.lane,
        });
    }
    return transitions;
}

bool is_press_event(const note_event& event) {
    return event.type == note_event::kind::tap || event.type == note_event::kind::hold_head;
}

int hand_for_lane(int lane, int key_count) {
    if (key_count <= 1) {
        return 0;
    }
    return lane < key_count / 2 ? 0 : 1;
}

double total_chart_length_ms(const std::vector<note_event>& events, const std::vector<hold_interval>& holds) {
    double total = 0.0;
    if (!events.empty()) {
        total = std::max(total, events.back().time_ms);
    }
    for (const hold_interval& hold : holds) {
        total = std::max(total, hold.end_ms);
    }
    return total;
}

int active_hold_count_at(const std::vector<hold_interval>& holds, double time_ms) {
    int count = 0;
    for (const hold_interval& hold : holds) {
        if (hold.start_ms <= time_ms && time_ms < hold.end_ms) {
            ++count;
        }
    }
    return count;
}

int active_hold_count_for_hand_at(const std::vector<hold_interval>& holds, double time_ms, int hand, int key_count) {
    int count = 0;
    for (const hold_interval& hold : holds) {
        if (hold.start_ms <= time_ms && time_ms < hold.end_ms &&
            hand_for_lane(hold.lane, key_count) == hand) {
            ++count;
        }
    }
    return count;
}

float cross_lane_stress_at(const std::vector<hold_interval>& holds, double time_ms, int tap_lane, int key_count) {
    if (key_count <= 1) {
        return 0.0f;
    }

    float sum = 0.0f;
    int count = 0;
    for (const hold_interval& hold : holds) {
        if (hold.start_ms <= time_ms && time_ms < hold.end_ms) {
            sum += static_cast<float>(std::abs(hold.lane - tap_lane)) / static_cast<float>(key_count - 1);
            ++count;
        }
    }
    return count == 0 ? 0.0f : sum / static_cast<float>(count);
}

double nearest_event_gap_seconds(const std::vector<note_event>& events, const note_event& target) {
    double nearest = 999.0;
    for (const note_event& event : events) {
        if (&event == &target) {
            continue;
        }
        nearest = std::min(nearest, std::abs(event.time_ms - target.time_ms) / 1000.0);
    }
    return nearest;
}

float local_density_per_second(const std::vector<note_event>& events, double center_ms, double radius_ms) {
    float weighted_count = 0.0f;
    for (const note_event& event : events) {
        if (std::abs(event.time_ms - center_ms) > radius_ms) {
            continue;
        }
        switch (event.type) {
            case note_event::kind::tap:
            case note_event::kind::hold_head:
                weighted_count += 1.0f;
                break;
            case note_event::kind::hold_tail:
                weighted_count += 1.10f;
                break;
        }
    }
    return weighted_count / static_cast<float>((radius_ms * 2.0) / 1000.0);
}

float local_stream_factor(const std::vector<transition_sample>& transitions, double center_ms) {
    float sum = 0.0f;
    int count = 0;
    for (const transition_sample& transition : transitions) {
        if (std::abs(transition.time_ms - center_ms) > 600.0) {
            continue;
        }
        sum += std::pow(1.0 / (transition.dt_seconds + kTimeEpsilonSeconds), kGammaStream);
        ++count;
    }
    return count == 0 ? 0.0f : sum / static_cast<float>(count);
}

float local_jump_factor(const std::vector<transition_sample>& transitions, double center_ms) {
    float sum = 0.0f;
    int count = 0;
    for (const transition_sample& transition : transitions) {
        if (std::abs(transition.time_ms - center_ms) > 600.0) {
            continue;
        }
        sum += std::pow(transition.lane_distance_norm, kGammaLane) *
               std::pow(1.0 / (transition.dt_seconds + kTimeEpsilonSeconds), kGammaJumpTime);
        ++count;
    }
    return count == 0 ? 0.0f : sum / static_cast<float>(count);
}

float local_release_factor(const std::vector<note_event>& events, double center_ms) {
    float sum = 0.0f;
    int count = 0;
    for (const note_event& event : events) {
        if (event.type != note_event::kind::hold_tail || std::abs(event.time_ms - center_ms) > kReleaseRadiusMs) {
            continue;
        }
        sum += std::pow(1.0 / (nearest_event_gap_seconds(events, event) + kTimeEpsilonSeconds), kGammaRelease);
        ++count;
    }
    return count == 0 ? 0.0f : sum / static_cast<float>(count);
}

float local_overlap_factor(const std::vector<note_event>& events, const std::vector<hold_interval>& holds,
                           double center_ms, int key_count) {
    float sum = 0.0f;
    for (const note_event& event : events) {
        if (std::abs(event.time_ms - center_ms) > 400.0) {
            continue;
        }
        if (event.type == note_event::kind::hold_tail) {
            continue;
        }
        const int active_holds = active_hold_count_at(holds, event.time_ms);
        const float cross_stress = cross_lane_stress_at(holds, event.time_ms, event.lane, key_count);
        sum += std::pow(1.0f + kHoldInterfere * static_cast<float>(active_holds), kGammaOverlapHold) *
               (1.0f + kCrossHand * cross_stress);
    }
    return sum / static_cast<float>((400.0 * 2.0) / 1000.0);
}

float local_chord_factor(const std::vector<note_event>& events, double center_ms, int key_count) {
    if (key_count <= 1) {
        return 0.0f;
    }

    float sum = 0.0f;
    size_t i = 0;
    while (i < events.size()) {
        const note_event& first = events[i];
        if (!is_press_event(first)) {
            ++i;
            continue;
        }

        size_t j = i + 1;
        int count = 1;
        int min_lane = first.lane;
        int max_lane = first.lane;
        int hands[2] = {0, 0};
        ++hands[hand_for_lane(first.lane, key_count)];
        while (j < events.size() && std::abs(events[j].time_ms - first.time_ms) <= kChordMergeMs) {
            if (is_press_event(events[j])) {
                ++count;
                min_lane = std::min(min_lane, events[j].lane);
                max_lane = std::max(max_lane, events[j].lane);
                ++hands[hand_for_lane(events[j].lane, key_count)];
            }
            ++j;
        }

        if (count >= 2 && std::abs(first.time_ms - center_ms) <= kReadRadiusMs) {
            const float spread = static_cast<float>(max_lane - min_lane) / static_cast<float>(key_count - 1);
            const float one_hand_bias =
                static_cast<float>(std::max(hands[0], hands[1])) / static_cast<float>(count);
            sum += std::pow(static_cast<float>(count), kGammaChord) * (1.0f + 0.25f * spread + 0.30f * one_hand_bias);
        }
        i = j;
    }

    return sum / static_cast<float>((kReadRadiusMs * 2.0) / 1000.0);
}

float local_hand_load_factor(const std::vector<note_event>& events, const std::vector<hold_interval>& holds,
                             double center_ms, int key_count) {
    float hand_load[2] = {0.0f, 0.0f};
    for (const note_event& event : events) {
        if (!is_press_event(event) || std::abs(event.time_ms - center_ms) > kBalanceRadiusMs) {
            continue;
        }

        const int hand = hand_for_lane(event.lane, key_count);
        const int active_holds = active_hold_count_for_hand_at(holds, event.time_ms, hand, key_count);
        hand_load[hand] += 1.0f + 0.55f * static_cast<float>(active_holds);
    }

    const float seconds = static_cast<float>((kBalanceRadiusMs * 2.0) / 1000.0);
    const float left_rate = hand_load[0] / seconds;
    const float right_rate = hand_load[1] / seconds;
    const float peak = std::max(left_rate, right_rate);
    const float skew = std::abs(left_rate - right_rate);
    return std::pow(peak, kGammaHand) + 0.55f * std::pow(skew, kGammaHand);
}

float local_hold_responsibility_conflict_factor(const std::vector<note_event>& events,
                                                const std::vector<hold_interval>& holds,
                                                double center_ms,
                                                int key_count) {
    float sum = 0.0f;
    for (const note_event& event : events) {
        if (std::abs(event.time_ms - center_ms) > kReleaseRadiusMs) {
            continue;
        }

        const int hand = hand_for_lane(event.lane, key_count);
        if (event.type == note_event::kind::hold_tail) {
            for (const note_event& nearby : events) {
                if (!is_press_event(nearby) ||
                    hand_for_lane(nearby.lane, key_count) != hand ||
                    std::abs(nearby.time_ms - event.time_ms) > 140.0) {
                    continue;
                }
                const float urgency =
                    1.0f / static_cast<float>(std::abs(nearby.time_ms - event.time_ms) / 1000.0 + kTimeEpsilonSeconds);
                sum += 0.45f * std::min(urgency, 8.0f);
            }
            continue;
        }

        if (!is_press_event(event)) {
            continue;
        }
        const int same_hand_holds = active_hold_count_for_hand_at(holds, event.time_ms, hand, key_count);
        if (same_hand_holds > 0) {
            sum += 0.65f * std::pow(1.0f + static_cast<float>(same_hand_holds), kGammaHoldConflict);
        }
    }

    return sum / static_cast<float>((kReleaseRadiusMs * 2.0) / 1000.0);
}

float local_pattern_factor(const std::vector<transition_sample>& transitions,
                           double center_ms,
                           const std::vector<note_event>& events,
                           int key_count) {
    std::vector<float> alt_costs;
    std::vector<const note_event*> presses;
    float jack_sum = 0.0f;
    int jack_count = 0;

    for (size_t i = 0; i < transitions.size(); ++i) {
        const transition_sample& transition = transitions[i];
        if (std::abs(transition.time_ms - center_ms) > 700.0) {
            continue;
        }

        alt_costs.push_back(transition.lane_distance_norm *
                            static_cast<float>(1.0 / (transition.dt_seconds + kTimeEpsilonSeconds)));
        if (transition.same_lane) {
            jack_sum += std::pow(1.0 / (transition.dt_seconds + kTimeEpsilonSeconds), kGammaJack);
            ++jack_count;
        }
    }

    for (const note_event& event : events) {
        if (is_press_event(event) && std::abs(event.time_ms - center_ms) <= 700.0) {
            presses.push_back(&event);
        }
    }

    float trill = 0.0f;
    float stair = 0.0f;
    float anchor = 0.0f;
    for (size_t i = 3; i < presses.size(); ++i) {
        const note_event& a = *presses[i - 3];
        const note_event& b = *presses[i - 2];
        const note_event& c = *presses[i - 1];
        const note_event& d = *presses[i];
        const double span_seconds = std::max((d.time_ms - a.time_ms) / 1000.0, kTimeEpsilonSeconds);
        if (span_seconds > 0.9) {
            continue;
        }

        const float speed = static_cast<float>(3.0 / span_seconds);
        if (a.lane == c.lane && b.lane == d.lane && a.lane != b.lane) {
            const bool one_hand = hand_for_lane(a.lane, key_count) == hand_for_lane(b.lane, key_count);
            trill += speed * (one_hand ? 1.35f : 1.0f);
        }

        const int ab = b.lane - a.lane;
        const int bc = c.lane - b.lane;
        const int cd = d.lane - c.lane;
        if (ab != 0 && bc != 0 && cd != 0 &&
            (ab > 0) == (bc > 0) && (bc > 0) == (cd > 0)) {
            stair += speed * 0.80f;
        }

        if ((a.lane == c.lane && a.lane != b.lane) ||
            (b.lane == d.lane && b.lane != c.lane)) {
            anchor += speed * 0.65f;
        }
    }

    float irregularity = 0.0f;
    if (!alt_costs.empty()) {
        const float mean = std::accumulate(alt_costs.begin(), alt_costs.end(), 0.0f) / static_cast<float>(alt_costs.size());
        float variance = 0.0f;
        for (float value : alt_costs) {
            const float delta = value - mean;
            variance += delta * delta;
        }
        irregularity = std::sqrt(variance / static_cast<float>(alt_costs.size()));
    }

    float burst = std::max(0.0f, local_density_per_second(events, center_ms, kBurstRadiusMs) -
                                 local_density_per_second(events, center_ms, kAverageRadiusMs));
    burst = std::pow(burst, kGammaBurst);

    return 0.45f * irregularity +
           0.90f * (jack_count == 0 ? 0.0f : jack_sum / static_cast<float>(jack_count)) +
           0.60f * burst +
           0.16f * trill +
           0.10f * stair +
           0.12f * anchor;
}

float local_balance_factor(const std::vector<note_event>& events, double center_ms, int key_count) {
    if (key_count <= 1) {
        return 0.0f;
    }

    const int left_limit = key_count / 2;
    float left = 0.0f;
    float right = 0.0f;
    for (const note_event& event : events) {
        if (std::abs(event.time_ms - center_ms) > kBalanceRadiusMs) {
            continue;
        }
        if (event.lane < left_limit) {
            left += 1.0f;
        } else {
            right += 1.0f;
        }
    }
    return std::pow(std::abs(left - right), kGammaBalance);
}

float local_stamina_factor(const std::vector<note_event>& events, double center_ms) {
    float integral = 0.0f;
    constexpr double kStrideMs = 250.0;
    for (double sample_ms = std::max(0.0, center_ms - kStaminaWindowMs); sample_ms <= center_ms; sample_ms += kStrideMs) {
        const float density = local_density_per_second(events, sample_ms, kDensityRadiusMs);
        integral += std::max(0.0f, density - kStaminaThreshold) * static_cast<float>(kStrideMs / kStaminaWindowMs);
    }
    return std::pow(integral, kGammaStamina);
}

float local_read_factor(const std::vector<note_event>& events, const std::vector<hold_interval>& holds, double center_ms) {
    const float overlap = std::pow(local_density_per_second(events, center_ms, kReadRadiusMs), kGammaReadOverlap);
    const float tail_clutter = std::pow(static_cast<float>(active_hold_count_at(holds, center_ms)), kGammaReadTail);

    std::vector<double> gaps;
    const note_event* previous = nullptr;
    for (const note_event& event : events) {
        if (!is_press_event(event) || std::abs(event.time_ms - center_ms) > kReadRadiusMs) {
            continue;
        }
        if (previous != nullptr) {
            gaps.push_back((event.time_ms - previous->time_ms) / 1000.0);
        }
        previous = &event;
    }

    float rhythm_variance = 0.0f;
    if (gaps.size() >= 2) {
        const double mean = std::accumulate(gaps.begin(), gaps.end(), 0.0) / static_cast<double>(gaps.size());
        double variance = 0.0;
        for (double gap : gaps) {
            const double delta = gap - mean;
            variance += delta * delta;
        }
        rhythm_variance = static_cast<float>(std::sqrt(variance / static_cast<double>(gaps.size())) /
                                             (mean + kTimeEpsilonSeconds));
    }

    return overlap + tail_clutter + 0.55f * rhythm_variance;
}

float local_difficulty_at(const std::vector<note_event>& events, const std::vector<hold_interval>& holds,
                          const std::vector<transition_sample>& transitions, double center_ms, int key_count) {
    const float density = std::pow(local_density_per_second(events, center_ms, kDensityRadiusMs), kGammaDensity);
    const float stream = local_stream_factor(transitions, center_ms);
    const float jump = local_jump_factor(transitions, center_ms);
    const float hold = std::pow(static_cast<float>(active_hold_count_at(holds, center_ms)), kGammaHold);
    const float release = local_release_factor(events, center_ms);
    const float overlap = local_overlap_factor(events, holds, center_ms, key_count);
    const float pattern = local_pattern_factor(transitions, center_ms, events, key_count);
    const float balance = local_balance_factor(events, center_ms, key_count);
    const float stamina = local_stamina_factor(events, center_ms);
    const float chord = local_chord_factor(events, center_ms, key_count);
    const float hand = local_hand_load_factor(events, holds, center_ms, key_count);
    const float hold_conflict = local_hold_responsibility_conflict_factor(events, holds, center_ms, key_count);
    const float read = local_read_factor(events, holds, center_ms);

    const float base =
        0.35f +
        kWeightDensity * density +
        kWeightStream * stream +
        kWeightJump * jump +
        kWeightHold * hold +
        kWeightRelease * release +
        kWeightOverlap * overlap +
        kWeightPattern * pattern +
        kWeightBalance * balance +
        kWeightStamina * stamina +
        kWeightChord * chord +
        kWeightHand * hand +
        kWeightHoldConflict * hold_conflict +
        kWeightRead * read;

    const float coupling =
        1.0f +
        kCouplingHoldDensity * hold * density +
        kCouplingHoldJump * hold * jump +
        kCouplingOverlapJump * overlap * jump +
        kCouplingStaminaStream * stamina * stream +
        kCouplingReadOverlap * read * overlap +
        kCouplingChordJump * chord * jump +
        kCouplingHandHold * hand * hold_conflict;

    return base * coupling;
}

}  // namespace

namespace chart_difficulty {

float calculate_rating(const chart_data& data) {
    if (data.notes.empty() || data.meta.key_count <= 0 || data.meta.resolution <= 0) {
        return 0.0f;
    }

    timing_engine engine;
    engine.init(data.timing_events, data.meta.resolution, data.meta.offset);

    std::vector<note_event> events = build_note_events(data, engine);
    std::vector<hold_interval> holds = build_hold_intervals(data, engine);
    std::vector<transition_sample> transitions = build_transitions(events, data.meta.key_count);
    if (events.empty()) {
        return 0.0f;
    }

    const double total_ms = std::max(1.0, total_chart_length_ms(events, holds));
    double weighted_sum = 0.0;
    double total_weight_ms = 0.0;

    for (size_t i = 0; i < events.size(); ++i) {
        const double left = (i == 0) ? 0.0 : (events[i - 1].time_ms + events[i].time_ms) * 0.5;
        const double right = (i + 1 == events.size()) ? total_ms : (events[i].time_ms + events[i + 1].time_ms) * 0.5;
        const double weight_ms = std::max(1.0, right - left);
        const float local = std::max(0.0f, local_difficulty_at(events, holds, transitions, events[i].time_ms, data.meta.key_count));
        weighted_sum += std::pow(local, kPeakPower) * weight_ms;
        total_weight_ms += weight_ms;
    }

    if (total_weight_ms <= 0.0) {
        return 0.0f;
    }

    return kScale * static_cast<float>(std::pow(weighted_sum / total_weight_ms, 1.0 / kPeakPower));
}

float level_from_rating(float raw_rating) {
    if (raw_rating <= 0.0f) {
        return 0.0f;
    }

    const float calibrated = 3.0f * std::log10(raw_rating) - 6.5f;
    const float rounded = std::round(calibrated * 10.0f) / 10.0f;
    return std::clamp(rounded, 0.1f, 15.0f);
}

float calculate_level(const chart_data& data) {
    return level_from_rating(calculate_rating(data));
}

chart_data with_auto_level(chart_data data) {
    apply_auto_level(data);
    return data;
}

void apply_auto_level(chart_data& data) {
    data.meta.level = calculate_level(data);
}

}  // namespace chart_difficulty
