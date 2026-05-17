#include "chart_difficulty.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

#include "chart_judge_events.h"
#include "timing_engine.h"

namespace {

struct note_event {
    enum class kind {
        tap,
        hold_head,
        hold_tail,
        release,
        stay,
    };

    double time_ms = 0.0;
    int lane = 0;
    kind type = kind::tap;
    float hold_effort_weight = 1.0f;
};

struct hold_interval {
    double start_ms = 0.0;
    double end_ms = 0.0;
    int lane = 0;
    float effort_weight = 1.0f;
};

struct transition_sample {
    double time_ms = 0.0;
    double dt_seconds = 0.0;
    float lane_distance_norm = 0.0f;
    float keyboard_motion_cost_norm = 0.0f;
    float effort_weight = 1.0f;
    bool same_lane = false;
};

struct local_difficulty_components {
    float density = 0.0f;
    float stream = 0.0f;
    float jump = 0.0f;
    float hold = 0.0f;
    float release = 0.0f;
    float overlap = 0.0f;
    float pattern = 0.0f;
    float balance = 0.0f;
    float stamina = 0.0f;
    float chord = 0.0f;
    float hand = 0.0f;
    float hold_conflict = 0.0f;
    float read = 0.0f;
    float rhythm = 0.0f;
    float base = 0.0f;
    float coupling = 1.0f;
    float total = 0.0f;
};

struct difficulty_context {
    std::vector<hold_interval> holds;
    std::vector<note_event> events;
    std::vector<transition_sample> transitions;
    float tempo_pressure = 0.0f;
};

constexpr double kDensityRadiusMs = 500.0;
constexpr double kReleaseRadiusMs = 400.0;
constexpr double kBurstRadiusMs = 300.0;
constexpr double kAverageRadiusMs = 2000.0;
constexpr double kBalanceRadiusMs = 800.0;
constexpr double kReadRadiusMs = 500.0;
constexpr double kRhythmRadiusMs = 1200.0;
constexpr double kRhythmMinGapMs = 35.0;
constexpr double kRhythmMaxGapMs = 900.0;
constexpr float kRhythmDisplayScale = 5.0f;
constexpr double kStaminaWindowMs = 8000.0;
constexpr double kTimeEpsilonSeconds = 0.015;
constexpr double kChordMergeMs = 2.0;

constexpr float kScale = 12.0f;
constexpr float kPeakPower = 1.7f;
constexpr float kStaminaThreshold = 3.2f;
constexpr float kTempoPressureBaseBpm = 130.0f;

// 各局所要素の寄与。大きいほど、その要素が最終難易度へ強く効く。
// density: その瞬間の物量
// stream: 連打が続く速さ
// jump: 同じ手の中で短時間に移動する押しづらさ
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
// rhythm: 間隔の揺れ・短長混在・リズム認識の難しさ
constexpr float kWeightDensity = 1.50f;
constexpr float kWeightStream = 1.20f;
constexpr float kWeightJump = 0.75f;
constexpr float kWeightHold = 1.00f;
constexpr float kWeightRelease = 0.70f;
constexpr float kWeightOverlap = 1.00f;
constexpr float kWeightPattern = 0.35f;
constexpr float kWeightBalance = 0.35f;
constexpr float kWeightStamina = 2.00f;
constexpr float kWeightChord = 0.18f;
constexpr float kWeightHand = 0.70f;
constexpr float kWeightHoldConflict = 0.32f;
constexpr float kWeightRead = 1.20f;
constexpr float kWeightRhythm = 0.75f;

// 各要素の非線形補正。大きいほど、その要素の「高い値」を強調しやすい。
constexpr float kGammaDensity = 1.25f;
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

constexpr float kHoldInterfere = 0.40f;
constexpr float kCrossHand = 0.35f;
constexpr float kShortHoldMinMs = 180.0f;
constexpr float kFullHoldEffortMs = 900.0f;
constexpr float kMinShortHoldEffort = 0.35f;
constexpr float kHeldStayEffort = 0.10f;
constexpr float kFreeStayEffort = 0.25f;
constexpr double kHandSustainWindowMs = 4200.0;
constexpr float kHandSustainThreshold = 3.00f;
constexpr float kHandSustainSkewThreshold = 0.90f;
constexpr double kLongStreamWindowMs = 4200.0;
constexpr float kLongStreamThreshold = 4.40f;

// 要素どうしの掛け算ボーナス。複合配置のしんどさを少し増幅する。
constexpr float kCouplingHoldDensity = 0.10f;
constexpr float kCouplingHoldJump = 0.12f;
constexpr float kCouplingOverlapJump = 0.10f;
constexpr float kCouplingStaminaStream = 0.20f;
constexpr float kCouplingReadOverlap = 0.06f;
constexpr float kCouplingChordJump = 0.025f;
constexpr float kCouplingHandHold = 0.020f;

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

float hold_duration_effort(double start_ms, double end_ms) {
    const float duration_ms = static_cast<float>(std::max(0.0, end_ms - start_ms));
    const float t = std::clamp((duration_ms - kShortHoldMinMs) / (kFullHoldEffortMs - kShortHoldMinMs), 0.0f, 1.0f);
    return kMinShortHoldEffort + (1.0f - kMinShortHoldEffort) * t;
}

float hold_head_effort(float hold_effort) {
    return 0.55f + 0.45f * hold_effort;
}

float active_hold_load_at(const std::vector<hold_interval>& holds, double time_ms) {
    float load = 0.0f;
    for (const hold_interval& hold : holds) {
        if (hold.start_ms <= time_ms && time_ms < hold.end_ms) {
            load += hold.effort_weight;
        }
    }
    return load;
}

float chart_tempo_pressure(const chart_data& data, const std::vector<note_event>& events,
                           const std::vector<hold_interval>& holds) {
    float max_bpm = 0.0f;
    for (const timing_event& event : data.timing_events) {
        if (event.type == timing_event_type::bpm) {
            max_bpm = std::max(max_bpm, event.bpm);
        }
    }
    if (max_bpm <= kTempoPressureBaseBpm) {
        return 0.0f;
    }
    const float normalized = std::min(2.0f, (max_bpm - kTempoPressureBaseBpm) / 30.0f);
    const float high_tempo = max_bpm <= 180.0f
                                 ? 0.0f
                                 : 0.6f * std::pow(std::min(2.0f, (max_bpm - 180.0f) / 45.0f), 1.05f);

    float real_press_count = 0.0f;
    for (const note_event& event : events) {
        if (event.type == note_event::kind::tap || event.type == note_event::kind::hold_head) {
            real_press_count += event.hold_effort_weight;
        }
    }
    double total_ms = 1.0;
    for (const note_event& event : events) {
        total_ms = std::max(total_ms, event.time_ms);
    }
    for (const hold_interval& hold : holds) {
        total_ms = std::max(total_ms, hold.end_ms);
    }
    const float real_press_density = real_press_count / static_cast<float>(std::max(1.0, total_ms) / 1000.0);
    const float density_multiplier = std::clamp((real_press_density - 4.5f) / 0.7f, 0.0f, 1.0f);
    return (4.5f * std::pow(normalized, 1.10f) + high_tempo) * density_multiplier;
}

std::vector<note_event> build_note_events(const chart_data& data, const timing_engine& engine,
                                          const std::vector<hold_interval>& holds) {
    std::vector<note_event> events;
    events.reserve(data.notes.size() * 2);

    for (const note_data& note : data.notes) {
        const int representative_lane = note.lane + note_lane_width(note) / 2;
        const double start_ms = engine.tick_to_ms(note.tick);
        switch (note.type) {
            case note_type::tap:
                events.push_back({start_ms, representative_lane, note_event::kind::tap, 1.0f});
                break;
            case note_type::hold: {
                const double end_ms = engine.tick_to_ms(note.end_tick);
                const float effort = hold_duration_effort(start_ms, end_ms);
                events.push_back({start_ms, representative_lane, note_event::kind::hold_head, hold_head_effort(effort)});
                events.push_back({end_ms, representative_lane, note_event::kind::hold_tail, effort});
                break;
            }
            case note_type::release:
                events.push_back({start_ms, representative_lane, note_event::kind::tap, 1.0f});
                break;
            case note_type::stay:
                events.push_back({start_ms, representative_lane, note_event::kind::stay,
                                  active_hold_load_at(holds, start_ms) > 0.0f ? kHeldStayEffort : kFreeStayEffort});
                break;
        }
    }

    std::sort(events.begin(), events.end(), [](const note_event& left, const note_event& right) {
        if (left.time_ms != right.time_ms) {
            return left.time_ms < right.time_ms;
        }
        return left.lane < right.lane;
    });
    return events;
}

std::vector<hold_interval> build_hold_intervals(const chart_data& data, const timing_engine& engine) {
    std::vector<hold_interval> holds;
    holds.reserve(data.notes.size());
    for (const note_data& note : data.notes) {
        if (note.type != note_type::hold) {
            continue;
        }
        const double start_ms = engine.tick_to_ms(note.tick);
        const double end_ms = engine.tick_to_ms(note.end_tick);
        holds.push_back({start_ms, end_ms, note.lane + note_lane_width(note) / 2,
                         hold_duration_effort(start_ms, end_ms)});
    }
    return holds;
}

float event_effort_weight(const note_event& event) {
    return event.hold_effort_weight;
}

float keyboard_motion_cost_norm(int previous_lane, int current_lane, int key_count);

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
            keyboard_motion_cost_norm(previous.lane, current.lane, key_count),
            std::sqrt(event_effort_weight(previous) * event_effort_weight(current)),
            current.lane == previous.lane,
        });
    }
    return transitions;
}

difficulty_context build_difficulty_context(const chart_data& data, const timing_engine& engine) {
    difficulty_context context;
    context.holds = build_hold_intervals(data, engine);
    context.events = build_note_events(data, engine, context.holds);
    context.transitions = build_transitions(context.events, data.meta.key_count);
    context.tempo_pressure = chart_tempo_pressure(data, context.events, context.holds);
    return context;
}

bool is_press_event(const note_event& event) {
    return event.type == note_event::kind::tap ||
           event.type == note_event::kind::hold_head ||
           event.type == note_event::kind::stay;
}

int hand_for_lane(int lane, int key_count) {
    if (key_count <= 1) {
        return 0;
    }
    return lane < key_count / 2 ? 0 : 1;
}

int hand_lane_count(int hand, int key_count) {
    if (key_count <= 1) {
        return 1;
    }
    const int left = key_count / 2;
    const int right = key_count - left;
    return hand == 0 ? left : right;
}

float keyboard_motion_cost_norm(int previous_lane, int current_lane, int key_count) {
    if (key_count <= 1 || previous_lane == current_lane) {
        return 0.0f;
    }

    const int previous_hand = hand_for_lane(previous_lane, key_count);
    const int current_hand = hand_for_lane(current_lane, key_count);
    if (previous_hand != current_hand) {
        return 0.0f;
    }

    const int lanes_for_hand = hand_lane_count(current_hand, key_count);
    const float max_distance = static_cast<float>(std::max(1, lanes_for_hand - 1));
    return std::clamp(static_cast<float>(std::abs(current_lane - previous_lane)) / max_distance, 0.0f, 1.0f);
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

float active_hold_load_for_hand_at(const std::vector<hold_interval>& holds, double time_ms, int hand, int key_count) {
    float load = 0.0f;
    for (const hold_interval& hold : holds) {
        if (hold.start_ms <= time_ms && time_ms < hold.end_ms &&
            hand_for_lane(hold.lane, key_count) == hand) {
            load += hold.effort_weight;
        }
    }
    return load;
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
                weighted_count += event_effort_weight(event);
                break;
            case note_event::kind::stay:
                weighted_count += event_effort_weight(event);
                break;
            case note_event::kind::hold_tail:
                weighted_count += 1.10f * event_effort_weight(event);
                break;
            case note_event::kind::release:
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
        sum += transition.effort_weight *
               std::pow(1.0 / (transition.dt_seconds + kTimeEpsilonSeconds), kGammaStream);
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
        sum += transition.effort_weight *
               std::pow(transition.keyboard_motion_cost_norm, kGammaLane) *
               std::pow(1.0 / (transition.dt_seconds + kTimeEpsilonSeconds), kGammaJumpTime);
        ++count;
    }
    return count == 0 ? 0.0f : sum / static_cast<float>(count);
}

float local_release_factor(const std::vector<note_event>& events, double center_ms) {
    float sum = 0.0f;
    int count = 0;
    for (const note_event& event : events) {
        if ((event.type != note_event::kind::hold_tail && event.type != note_event::kind::release) ||
            std::abs(event.time_ms - center_ms) > kReleaseRadiusMs) {
            continue;
        }
        sum += event_effort_weight(event) *
               std::pow(1.0 / (nearest_event_gap_seconds(events, event) + kTimeEpsilonSeconds), kGammaRelease);
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
        if (event.type == note_event::kind::hold_tail || event.type == note_event::kind::release) {
            continue;
        }
        const float active_holds = active_hold_load_at(holds, event.time_ms);
        const float cross_stress = cross_lane_stress_at(holds, event.time_ms, event.lane, key_count);
        sum += event_effort_weight(event) *
               std::pow(1.0f + kHoldInterfere * active_holds, kGammaOverlapHold) *
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
        const float active_holds = active_hold_load_for_hand_at(holds, event.time_ms, hand, key_count);
        hand_load[hand] += event_effort_weight(event) * (1.0f + 0.55f * active_holds);
    }

    const float seconds = static_cast<float>((kBalanceRadiusMs * 2.0) / 1000.0);
    const float left_rate = hand_load[0] / seconds;
    const float right_rate = hand_load[1] / seconds;
    const float peak = std::max(left_rate, right_rate);
    const float skew = std::abs(left_rate - right_rate);

    float sustained_load[2] = {0.0f, 0.0f};
    for (const note_event& event : events) {
        if ((event.type != note_event::kind::tap && event.type != note_event::kind::hold_head) ||
            event.time_ms < center_ms - kHandSustainWindowMs || event.time_ms > center_ms) {
            continue;
        }

        const int hand = hand_for_lane(event.lane, key_count);
        sustained_load[hand] += event_effort_weight(event);
    }

    const float sustain_seconds = static_cast<float>(kHandSustainWindowMs / 1000.0);
    const float sustained_left_rate = sustained_load[0] / sustain_seconds;
    const float sustained_right_rate = sustained_load[1] / sustain_seconds;
    const float sustained_peak = std::max(sustained_left_rate, sustained_right_rate);
    const float sustained_skew = std::abs(sustained_left_rate - sustained_right_rate);
    const float sustained =
        0.72f * std::pow(std::max(0.0f, sustained_peak - kHandSustainThreshold), 1.08f) +
        0.36f * std::pow(std::max(0.0f, sustained_skew - kHandSustainSkewThreshold), 1.02f);

    return std::pow(peak, kGammaHand) + 0.55f * std::pow(skew, kGammaHand) + sustained;
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
                sum += 0.45f * event_effort_weight(nearby) * std::min(urgency, 8.0f);
            }
            continue;
        }

        if (!is_press_event(event)) {
            continue;
        }
        const float same_hand_holds = active_hold_load_for_hand_at(holds, event.time_ms, hand, key_count);
        if (same_hand_holds > 0.0f) {
            sum += event_effort_weight(event) * 0.65f * std::pow(1.0f + same_hand_holds, kGammaHoldConflict);
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
    float jack_weight_sum = 0.0f;

    for (size_t i = 0; i < transitions.size(); ++i) {
        const transition_sample& transition = transitions[i];
        if (std::abs(transition.time_ms - center_ms) > 700.0) {
            continue;
        }

        alt_costs.push_back(transition.keyboard_motion_cost_norm *
                            static_cast<float>(1.0 / (transition.dt_seconds + kTimeEpsilonSeconds)) *
                            transition.effort_weight);
        if (transition.same_lane) {
            jack_sum += transition.effort_weight *
                        std::pow(1.0 / (transition.dt_seconds + kTimeEpsilonSeconds), kGammaJack);
            jack_weight_sum += transition.effort_weight;
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
        const float sequence_effort = 0.25f * (event_effort_weight(a) + event_effort_weight(b) +
                                               event_effort_weight(c) + event_effort_weight(d));
        if (a.lane == c.lane && b.lane == d.lane && a.lane != b.lane) {
            const bool one_hand = hand_for_lane(a.lane, key_count) == hand_for_lane(b.lane, key_count);
            trill += sequence_effort * speed * (one_hand ? 1.35f : 1.0f);
        }

        const int ab = b.lane - a.lane;
        const int bc = c.lane - b.lane;
        const int cd = d.lane - c.lane;
        if (ab != 0 && bc != 0 && cd != 0 &&
            (ab > 0) == (bc > 0) && (bc > 0) == (cd > 0)) {
            stair += sequence_effort * speed * 0.80f;
        }

        if ((a.lane == c.lane && a.lane != b.lane) ||
            (b.lane == d.lane && b.lane != c.lane)) {
            anchor += sequence_effort * speed * 0.65f;
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
           0.90f * (jack_weight_sum <= 0.0f ? 0.0f : jack_sum / jack_weight_sum) +
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

float local_stamina_factor(const std::vector<note_event>& events, const std::vector<hold_interval>& holds,
                           double center_ms) {
    float integral = 0.0f;
    constexpr double kStrideMs = 250.0;
    for (double sample_ms = std::max(0.0, center_ms - kStaminaWindowMs); sample_ms <= center_ms; sample_ms += kStrideMs) {
        const float density = local_density_per_second(events, sample_ms, kDensityRadiusMs);
        integral += std::max(0.0f, density - kStaminaThreshold) * static_cast<float>(kStrideMs / kStaminaWindowMs);
    }
    const double total_ms = std::max(1.0, events.empty() ? 1.0 : events.back().time_ms - events.front().time_ms);
    float weighted_events = 0.0f;
    for (const note_event& event : events) {
        switch (event.type) {
            case note_event::kind::tap:
            case note_event::kind::hold_head:
                weighted_events += event_effort_weight(event);
                break;
            case note_event::kind::hold_tail:
                weighted_events += 1.10f * event_effort_weight(event);
                break;
            case note_event::kind::release:
                weighted_events += 1.10f;
                break;
            case note_event::kind::stay:
                weighted_events += event_effort_weight(event);
                break;
        }
    }
    const float global_density = weighted_events / static_cast<float>(total_ms / 1000.0);
    const float sustained_pressure = 2.0f * std::pow(std::max(0.0f, global_density - 4.8f), 1.10f);

    float long_stream_events = 0.0f;
    for (const note_event& event : events) {
        if ((event.type != note_event::kind::tap && event.type != note_event::kind::hold_head) ||
            event.time_ms < center_ms - kLongStreamWindowMs || event.time_ms > center_ms) {
            continue;
        }
        long_stream_events += event_effort_weight(event);
    }
    const float long_stream_rate = long_stream_events / static_cast<float>(kLongStreamWindowMs / 1000.0);
    const float long_stream_pressure =
        0.90f * std::pow(std::max(0.0f, long_stream_rate - kLongStreamThreshold), 1.08f);

    return std::pow(integral, kGammaStamina) + sustained_pressure + long_stream_pressure;
}

float trimmed_average(std::vector<float> values) {
    if (values.empty()) {
        return 0.0f;
    }

    std::sort(values.begin(), values.end());
    const size_t trim = values.size() >= 6 ? values.size() / 6 : 0;
    double sum = 0.0;
    size_t count = 0;
    for (size_t i = trim; i + trim < values.size(); ++i) {
        sum += values[i];
        ++count;
    }
    return count > 0 ? static_cast<float>(sum / static_cast<double>(count)) : values[values.size() / 2];
}

float local_rhythm_factor(const std::vector<note_event>& events, double center_ms) {
    std::vector<double> press_times;
    for (const note_event& event : events) {
        if (!is_press_event(event) || std::abs(event.time_ms - center_ms) > kRhythmRadiusMs) {
            continue;
        }
        if (!press_times.empty() && std::abs(event.time_ms - press_times.back()) <= kChordMergeMs) {
            continue;
        }
        press_times.push_back(event.time_ms);
    }

    std::vector<double> gaps_ms;
    for (size_t i = 1; i < press_times.size(); ++i) {
        const double gap_ms = press_times[i] - press_times[i - 1];
        if (gap_ms >= kRhythmMinGapMs && gap_ms <= kRhythmMaxGapMs) {
            gaps_ms.push_back(gap_ms);
        }
    }

    if (gaps_ms.size() < 3) {
        return 0.0f;
    }

    std::vector<double> sorted_gaps = gaps_ms;
    std::sort(sorted_gaps.begin(), sorted_gaps.end());
    const double median_gap = sorted_gaps[sorted_gaps.size() / 2];
    if (median_gap <= 0.0) {
        return 0.0f;
    }

    std::vector<float> absolute_deviations;
    std::vector<float> ratio_changes;
    std::vector<float> alternations;
    absolute_deviations.reserve(gaps_ms.size());
    ratio_changes.reserve(gaps_ms.size() - 1);
    alternations.reserve(gaps_ms.size() - 1);

    for (double gap_ms : gaps_ms) {
        absolute_deviations.push_back(static_cast<float>(std::abs(gap_ms - median_gap) / median_gap));
    }
    for (size_t i = 1; i < gaps_ms.size(); ++i) {
        const double previous = std::clamp(gaps_ms[i - 1] / median_gap, 0.25, 4.0);
        const double current = std::clamp(gaps_ms[i] / median_gap, 0.25, 4.0);
        ratio_changes.push_back(static_cast<float>(std::abs(std::log2(current / previous))));
        alternations.push_back(static_cast<float>(std::abs(gaps_ms[i] - gaps_ms[i - 1]) /
                                                  (std::max(gaps_ms[i], gaps_ms[i - 1]) + 1.0)));
    }

    const float spread = trimmed_average(std::move(absolute_deviations));
    const float ratio_shift = trimmed_average(std::move(ratio_changes));
    const float short_long = trimmed_average(std::move(alternations));
    const float raw = 0.95f * spread + 0.75f * ratio_shift + 0.50f * short_long;
    return std::min(10.0f, kRhythmDisplayScale * std::pow(std::clamp(raw, 0.0f, 3.5f), 0.82f));
}

float local_read_factor(const std::vector<note_event>& events, const std::vector<hold_interval>& holds, double center_ms) {
    const float overlap = std::pow(local_density_per_second(events, center_ms, kReadRadiusMs), kGammaReadOverlap);
    const float tail_clutter = std::pow(active_hold_load_at(holds, center_ms), kGammaReadTail);
    return overlap + tail_clutter;
}

local_difficulty_components local_difficulty_components_at(const std::vector<note_event>& events,
                                                           const std::vector<hold_interval>& holds,
                                                           const std::vector<transition_sample>& transitions,
                                                           double center_ms,
                                                           int key_count,
                                                           float tempo_pressure) {
    local_difficulty_components components;
    components.density = std::pow(local_density_per_second(events, center_ms, kDensityRadiusMs), kGammaDensity);
    components.stream = local_stream_factor(transitions, center_ms);
    components.jump = local_jump_factor(transitions, center_ms);
    components.hold = std::pow(active_hold_load_at(holds, center_ms), kGammaHold);
    components.release = local_release_factor(events, center_ms);
    components.overlap = local_overlap_factor(events, holds, center_ms, key_count);
    components.pattern = local_pattern_factor(transitions, center_ms, events, key_count);
    components.balance = local_balance_factor(events, center_ms, key_count);
    components.stamina = local_stamina_factor(events, holds, center_ms) + tempo_pressure;
    components.chord = local_chord_factor(events, center_ms, key_count);
    components.hand = local_hand_load_factor(events, holds, center_ms, key_count);
    components.hold_conflict = local_hold_responsibility_conflict_factor(events, holds, center_ms, key_count);
    components.read = local_read_factor(events, holds, center_ms);
    components.rhythm = local_rhythm_factor(events, center_ms);

    components.base =
        0.35f +
        kWeightDensity * components.density +
        kWeightStream * components.stream +
        kWeightJump * components.jump +
        kWeightHold * components.hold +
        kWeightRelease * components.release +
        kWeightOverlap * components.overlap +
        kWeightPattern * components.pattern +
        kWeightBalance * components.balance +
        kWeightStamina * components.stamina +
        kWeightChord * components.chord +
        kWeightHand * components.hand +
        kWeightHoldConflict * components.hold_conflict +
        kWeightRead * components.read +
        kWeightRhythm * components.rhythm;

    components.coupling =
        1.0f +
        kCouplingHoldDensity * components.hold * components.density +
        kCouplingHoldJump * components.hold * components.jump +
        kCouplingOverlapJump * components.overlap * components.jump +
        kCouplingStaminaStream * components.stamina * components.stream +
        kCouplingReadOverlap * components.read * components.overlap +
        kCouplingChordJump * components.chord * components.jump +
        kCouplingHandHold * components.hand * components.hold_conflict;

    components.total = components.base * components.coupling;
    return components;
}

float local_difficulty_at(const std::vector<note_event>& events, const std::vector<hold_interval>& holds,
                          const std::vector<transition_sample>& transitions, double center_ms, int key_count,
                          float tempo_pressure) {
    return local_difficulty_components_at(events, holds, transitions, center_ms, key_count, tempo_pressure).total;
}

}  // namespace

namespace chart_difficulty {

float calculate_rating(const chart_data& data) {
    if (data.notes.empty() || data.meta.key_count <= 0 || data.meta.resolution <= 0) {
        return 0.0f;
    }

    timing_engine engine;
    engine.init(data.timing_events, data.meta.resolution, data.meta.offset);

    const difficulty_context context = build_difficulty_context(data, engine);
    if (context.events.empty()) {
        return 0.0f;
    }

    const double total_ms = std::max(1.0, total_chart_length_ms(context.events, context.holds));
    double weighted_sum = 0.0;
    double total_weight_ms = 0.0;

    for (size_t i = 0; i < context.events.size(); ++i) {
        const double left = (i == 0) ? 0.0 : (context.events[i - 1].time_ms + context.events[i].time_ms) * 0.5;
        const double right = (i + 1 == context.events.size()) ? total_ms
                                                              : (context.events[i].time_ms + context.events[i + 1].time_ms) * 0.5;
        const double weight_ms = std::max(1.0, right - left);
        const float local =
            std::max(0.0f, local_difficulty_at(context.events, context.holds, context.transitions,
                                               context.events[i].time_ms, data.meta.key_count,
                                               context.tempo_pressure));
        weighted_sum += std::pow(local, kPeakPower) * weight_ms;
        total_weight_ms += weight_ms;
    }

    if (total_weight_ms <= 0.0) {
        return 0.0f;
    }

    return kScale * static_cast<float>(std::pow(weighted_sum / total_weight_ms, 1.0 / kPeakPower));
}

difficulty_breakdown calculate_breakdown(const chart_data& data) {
    difficulty_breakdown breakdown;
    if (data.notes.empty() || data.meta.key_count <= 0 || data.meta.resolution <= 0) {
        return breakdown;
    }

    timing_engine engine;
    engine.init(data.timing_events, data.meta.resolution, data.meta.offset);

    const difficulty_context context = build_difficulty_context(data, engine);
    if (context.events.empty()) {
        return breakdown;
    }

    struct factor_accumulator {
        const char* name = "";
        float weight = 0.0f;
        double value_sum = 0.0;
        double contribution_sum = 0.0;
    };

    std::array<factor_accumulator, 14> factors = {{
        {"density", kWeightDensity},
        {"stream", kWeightStream},
        {"jump", kWeightJump},
        {"hold", kWeightHold},
        {"release", kWeightRelease},
        {"overlap", kWeightOverlap},
        {"pattern", kWeightPattern},
        {"balance", kWeightBalance},
        {"stamina", kWeightStamina},
        {"chord", kWeightChord},
        {"hand", kWeightHand},
        {"hold_conflict", kWeightHoldConflict},
        {"read", kWeightRead},
        {"rhythm", kWeightRhythm},
    }};

    const auto add_factor_sample = [&](size_t index, float value, double weight_ms) {
        factors[index].value_sum += static_cast<double>(value) * weight_ms;
        factors[index].contribution_sum += static_cast<double>(value) * factors[index].weight * weight_ms;
    };

    const double total_ms = std::max(1.0, total_chart_length_ms(context.events, context.holds));
    double total_weight_ms = 0.0;
    double local_sum = 0.0;
    double coupling_sum = 0.0;
    double peak_sum = 0.0;

    for (size_t i = 0; i < context.events.size(); ++i) {
        const double left = (i == 0) ? 0.0 : (context.events[i - 1].time_ms + context.events[i].time_ms) * 0.5;
        const double right = (i + 1 == context.events.size()) ? total_ms
                                                              : (context.events[i].time_ms + context.events[i + 1].time_ms) * 0.5;
        const double weight_ms = std::max(1.0, right - left);
        const local_difficulty_components components =
            local_difficulty_components_at(context.events, context.holds, context.transitions, context.events[i].time_ms,
                                           data.meta.key_count, context.tempo_pressure);
        const float local = std::max(0.0f, components.total);

        add_factor_sample(0, components.density, weight_ms);
        add_factor_sample(1, components.stream, weight_ms);
        add_factor_sample(2, components.jump, weight_ms);
        add_factor_sample(3, components.hold, weight_ms);
        add_factor_sample(4, components.release, weight_ms);
        add_factor_sample(5, components.overlap, weight_ms);
        add_factor_sample(6, components.pattern, weight_ms);
        add_factor_sample(7, components.balance, weight_ms);
        add_factor_sample(8, components.stamina, weight_ms);
        add_factor_sample(9, components.chord, weight_ms);
        add_factor_sample(10, components.hand, weight_ms);
        add_factor_sample(11, components.hold_conflict, weight_ms);
        add_factor_sample(12, components.read, weight_ms);
        add_factor_sample(13, components.rhythm, weight_ms);

        local_sum += static_cast<double>(local) * weight_ms;
        coupling_sum += static_cast<double>(components.coupling) * weight_ms;
        peak_sum += std::pow(local, kPeakPower) * weight_ms;
        total_weight_ms += weight_ms;
    }

    if (total_weight_ms <= 0.0) {
        return breakdown;
    }

    breakdown.raw_rating = kScale * static_cast<float>(std::pow(peak_sum / total_weight_ms, 1.0 / kPeakPower));
    breakdown.level = level_from_rating(breakdown.raw_rating);
    breakdown.average_local_difficulty = static_cast<float>(local_sum / total_weight_ms);
    breakdown.average_coupling = static_cast<float>(coupling_sum / total_weight_ms);
    breakdown.factors.reserve(factors.size());
    for (const factor_accumulator& factor : factors) {
        breakdown.factors.push_back({
            factor.name,
            static_cast<float>(factor.value_sum / total_weight_ms),
            static_cast<float>(factor.contribution_sum / total_weight_ms),
        });
    }
    std::sort(breakdown.factors.begin(), breakdown.factors.end(),
              [](const difficulty_factor_breakdown& left, const difficulty_factor_breakdown& right) {
                  return left.average_contribution > right.average_contribution;
              });
    return breakdown;
}

std::vector<event_difficulty> calculate_event_difficulties(const chart_data& data,
                                                           const timing_engine& engine) {
    std::vector<event_difficulty> difficulties;
    if (data.notes.empty() || data.meta.key_count <= 0) {
        return difficulties;
    }

    const difficulty_context context = build_difficulty_context(data, engine);
    if (context.events.empty()) {
        return difficulties;
    }

    difficulties.reserve(data.notes.size() * 2);
    const std::vector<chart_judge_event> judge_events = chart_judge_events::build(data, engine);
    for (const chart_judge_event& event : judge_events) {
        const double head_ms = event.time_ms;
        const float local_difficulty = event.role == chart_judge_event_role::stay
                                           ? 0.0f
                                           : std::max(0.0f, local_difficulty_at(context.events, context.holds,
                                                                               context.transitions,
                                                                               head_ms, data.meta.key_count,
                                                                               context.tempo_pressure));
        difficulties.push_back(event_difficulty{
            event.event_index,
            head_ms,
            local_difficulty,
        });
    }

    return difficulties;
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
