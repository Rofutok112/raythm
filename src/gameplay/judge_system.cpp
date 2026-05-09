#include "judge_system.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kSameJudgementTimeToleranceMs = 0.5;
constexpr size_t kInvalidEventDescriptorIndex = static_cast<size_t>(-1);

bool same_judgement_time(double left_ms, double right_ms) {
    return std::fabs(left_ms - right_ms) <= kSameJudgementTimeToleranceMs;
}

}  // namespace

void judge_system::init(const std::vector<note_data>& notes, const timing_engine& engine) {
    note_states_.clear();
    note_states_.reserve(notes.size());
    for (std::vector<size_t>& lane_indices : lane_note_indices_) {
        lane_indices.clear();
    }
    for (std::vector<size_t>& lane_indices : lane_event_indices_) {
        lane_indices.clear();
    }
    lane_head_indices_.fill(0);
    lane_event_head_indices_.fill(0);
    active_hold_indices_.fill(std::nullopt);
    armed_release_event_indices_.fill(std::nullopt);
    judge_events_.clear();
    event_descriptor_indices_by_event_index_.clear();

    for (size_t i = 0; i < notes.size(); ++i) {
        const note_data& note = notes[i];
        note_state state;
        state.note_ref = note;
        state.target_ms = engine.tick_to_ms(note.tick);
        state.end_target_ms = engine.tick_to_ms(note.type == note_type::hold ? note.end_tick : note.tick);
        note_states_.push_back(state);
        if ((note.type == note_type::tap || note.type == note_type::hold) &&
            note.lane >= 0 && note.lane < kMaxLanes) {
            for (int lane = note.lane; lane <= note_last_lane(note) && lane < kMaxLanes; ++lane) {
                lane_note_indices_[static_cast<size_t>(lane)].push_back(i);
            }
        }
    }

    chart_data chart;
    chart.notes = notes;
    event_descriptors_ = chart_judge_events::build(chart, engine);
    event_completed_.assign(event_descriptors_.size(), false);
    completed_wide_press_absorb_until_ms_.assign(event_descriptors_.size(), -1.0);
    event_descriptor_indices_by_event_index_.assign(event_descriptors_.size(), kInvalidEventDescriptorIndex);
    standalone_release_events_.assign(event_descriptors_.size(), false);
    for (size_t i = 0; i < event_descriptors_.size(); ++i) {
        const chart_judge_event& event = event_descriptors_[i];
        if (event.event_index >= 0 &&
            static_cast<size_t>(event.event_index) < event_descriptor_indices_by_event_index_.size()) {
            event_descriptor_indices_by_event_index_[static_cast<size_t>(event.event_index)] = i;
        }
        if (event.lane >= 0 && event.lane < kMaxLanes) {
            for (int lane = event.lane; lane < event.lane + std::max(1, event.lane_width) && lane < kMaxLanes; ++lane) {
                lane_event_indices_[static_cast<size_t>(lane)].push_back(i);
            }
        }
        if (event.note_index >= note_states_.size()) {
            continue;
        }
        note_state& state = note_states_[event.note_index];
        if (event.role == chart_judge_event_role::hold_tail) {
            state.tail_event_index = event.event_index;
        } else {
            state.head_event_index = event.event_index;
            if (state.note_ref.type != note_type::hold) {
                state.tail_event_index = event.event_index;
            }
        }
    }
    for (size_t i = 0; i < event_descriptors_.size(); ++i) {
        const chart_judge_event& event = event_descriptors_[i];
        standalone_release_events_[i] =
            event.role == chart_judge_event_role::release && !release_overlaps_hold_tail(event);
    }
}

void judge_system::update(double current_ms, const input_handler& input) {
    judge_events_.clear();

    std::vector<input_event> ordered_events(input.events().begin(), input.events().end());
    std::stable_sort(ordered_events.begin(), ordered_events.end(), [](const input_event& left,
                                                                      const input_event& right) {
        return left.timestamp_ms < right.timestamp_ms;
    });
    for (const input_event& event : ordered_events) {
        process_input_event(event);
    }

    resolve_stay_notes(current_ms, input);
    resolve_hold_completions(current_ms);
    resolve_auto_misses(current_ms);
}

std::optional<judge_event> judge_system::get_last_judge() const {
    if (judge_events_.empty()) {
        return std::nullopt;
    }
    return judge_events_.back();
}

const std::vector<judge_event>& judge_system::get_judge_events() const {
    return judge_events_;
}

const std::vector<note_state>& judge_system::note_states() const {
    return note_states_;
}

judge_result judge_system::evaluate_offset(double offset_ms) const {
    const double absolute_offset = std::fabs(offset_ms);
    if (absolute_offset <= kPerfectWindowMs) {
        return judge_result::perfect;
    }
    if (absolute_offset <= kGreatWindowMs) {
        return judge_result::great;
    }
    if (absolute_offset <= kGoodWindowMs) {
        return judge_result::good;
    }
    if (absolute_offset <= kBadWindowMs) {
        return judge_result::bad;
    }
    return judge_result::miss;
}

judge_result judge_system::evaluate_hold_release_offset(double offset_ms) const {
    if (offset_ms >= 0.0) {
        return judge_result::perfect;
    }
    return evaluate_offset(offset_ms);
}

judge_result judge_system::evaluate_release_offset(double offset_ms) const {
    return std::fabs(offset_ms) <= kBadWindowMs ? judge_result::perfect : judge_result::miss;
}

judge_result judge_system::evaluate_stay_offset(double offset_ms) const {
    const double absolute_offset = std::fabs(offset_ms);
    if (absolute_offset <= kGreatWindowMs) {
        return judge_result::perfect;
    }
    if (absolute_offset <= kGoodWindowMs) {
        return judge_result::great;
    }
    if (absolute_offset <= kBadWindowMs) {
        return judge_result::good;
    }
    return judge_result::miss;
}

bool judge_system::is_in_judgement_window(double offset_ms) const {
    return std::fabs(offset_ms) <= kBadWindowMs;
}

void judge_system::process_input_event(const input_event& event) {
    switch (event.type) {
        case input_event_type::press:
            handle_press(event);
            break;
        case input_event_type::release:
            handle_hold_release(event);
            break;
    }
}

void judge_system::complete_due_hold_before(int lane, double timestamp_ms) {
    if (lane < 0 || lane >= kMaxLanes) {
        return;
    }

    const std::optional<size_t> active_hold = active_hold_indices_[static_cast<size_t>(lane)];
    if (!active_hold.has_value()) {
        return;
    }

    const note_state& state = note_states_[*active_hold];
    // 同じレーンで tail 通過後に押された場合は、先に前の hold を完了扱いにする。
    if (state.is_holding() && timestamp_ms >= state.end_target_ms) {
        complete_held_note(*active_hold, false);
    }
}

void judge_system::handle_hold_release(const input_event& event) {
    if (event.lane < 0 || event.lane >= kMaxLanes) {
        return;
    }

    const std::vector<size_t> release_candidates = find_release_candidates(event.lane, event.timestamp_ms);
    if (!release_candidates.empty()) {
        size_t representative_candidate = 0;
        for (size_t i = 0; i < release_candidates.size(); ++i) {
            if (event_descriptors_[release_candidates[i]].role == chart_judge_event_role::release) {
                representative_candidate = i;
                break;
            }
        }
        for (size_t i = 0; i < release_candidates.size(); ++i) {
            const size_t release_candidate = release_candidates[i];
            const chart_judge_event& descriptor = event_descriptors_[release_candidate];
            const double offset_ms = event.timestamp_ms - descriptor.time_ms;
            judge_result result = judge_result::miss;
            if (descriptor.role == chart_judge_event_role::release) {
                result = evaluate_release_offset(offset_ms);
            } else {
                result = evaluate_hold_release_offset(offset_ms);
            }

            judge_emit_options options;
            options.play_hitsound = i == representative_candidate && descriptor.role == chart_judge_event_role::release;
            options.show_feedback = i == representative_candidate;
            if (descriptor.role == chart_judge_event_role::hold_tail && offset_ms >= 0.0) {
                options.show_feedback = false;
            }
            complete_event(release_candidate, result, offset_ms, options);
        }
        const std::vector<size_t> stay_candidates = find_early_release_stay_candidates(event.lane, event.timestamp_ms);
        for (size_t i = 0; i < stay_candidates.size(); ++i) {
            const size_t event_index = stay_candidates[i];
            const chart_judge_event& descriptor = event_descriptors_[event_index];
            const double offset_ms = event.timestamp_ms - descriptor.time_ms;
            judge_emit_options options;
            options.play_hitsound = false;
            options.show_feedback = false;
            complete_event(event_index, evaluate_stay_offset(offset_ms), offset_ms, options);
        }

        const chart_judge_event& descriptor = event_descriptors_[release_candidates.front()];
        const double offset_ms = event.timestamp_ms - descriptor.time_ms;
        std::optional<size_t>& active_hold = active_hold_indices_[static_cast<size_t>(event.lane)];
        if (active_hold.has_value() &&
            note_states_[*active_hold].is_holding() &&
            note_covers_lane(note_states_[*active_hold].note_ref, event.lane) &&
            event.timestamp_ms >= note_states_[*active_hold].end_target_ms - kBadWindowMs) {
            const size_t note_index = *active_hold;
            note_states_[note_index].progress = note_progress_state::completed;
            clear_active_hold_for_note(note_index);
        }
        return;
    }

    std::optional<size_t>& active_hold = active_hold_indices_[static_cast<size_t>(event.lane)];
    if (!active_hold.has_value()) {
        const std::vector<size_t> stay_candidates = find_early_release_stay_candidates(event.lane, event.timestamp_ms);
        for (size_t i = 0; i < stay_candidates.size(); ++i) {
            const size_t event_index = stay_candidates[i];
            const chart_judge_event& descriptor = event_descriptors_[event_index];
            const double offset_ms = event.timestamp_ms - descriptor.time_ms;
            judge_emit_options options;
            options.play_hitsound = i == 0;
            options.show_feedback = i == 0;
            complete_event(event_index, evaluate_stay_offset(offset_ms), offset_ms, options);
        }
        return;
    }

    note_state& state = note_states_[*active_hold];
    if (!state.is_holding()) {
        active_hold.reset();
        return;
    }

    if (event.timestamp_ms >= state.end_target_ms) {
        complete_held_note(*active_hold, false);
        return;
    }

    // tail 到達前の release は、その時点で判定付きの完了として確定する。
    const double release_offset_ms = event.timestamp_ms - state.end_target_ms;
    state.progress = note_progress_state::completed;
    state.result = evaluate_hold_release_offset(release_offset_ms);
    clear_active_hold_for_note(*active_hold);
    if (const std::optional<size_t> tail_descriptor_index =
            descriptor_index_for_event_index(state.tail_event_index); !tail_descriptor_index.has_value() ||
            !mark_event_completed(*tail_descriptor_index)) {
        return;
    }
    judge_emit_options options;
    options.play_hitsound = false;
    emit_judge(state.result, release_offset_ms, state.note_ref.lane, state.tail_event_index, options);
}

void judge_system::handle_press(const input_event& event) {
    if (event.lane < 0 || event.lane >= kMaxLanes) {
        return;
    }

    complete_due_hold_before(event.lane, event.timestamp_ms);

    if (try_absorb_completed_wide_press(event)) {
        return;
    }

    const std::vector<size_t> candidate_indices = find_press_candidates(event.lane, event.timestamp_ms);
    if (candidate_indices.empty()) {
        arm_release_candidate(event.lane, event.timestamp_ms);
        return;
    }

    for (size_t i = 0; i < candidate_indices.size(); ++i) {
        const size_t candidate_index = candidate_indices[i];
        const chart_judge_event& descriptor = event_descriptors_[candidate_index];
        note_state& candidate = note_states_[descriptor.note_index];
        const double offset_ms = event.timestamp_ms - descriptor.time_ms;
        const judge_result result = descriptor.kind == chart_judge_event_kind::stay
                                        ? evaluate_stay_offset(offset_ms)
                                        : evaluate_offset(offset_ms);
        // hold の head 判定後は、tail 完走か早離しで確定するまで進行中のままにする。
        candidate.progress =
            (candidate.note_ref.type == note_type::hold && result != judge_result::miss)
                ? note_progress_state::holding
                : note_progress_state::completed;
        candidate.result = result;
        mark_event_completed(candidate_index);
        if (descriptor.kind == chart_judge_event_kind::press &&
            descriptor.lane_width > 1 &&
            result == judge_result::perfect) {
            completed_wide_press_absorb_until_ms_[candidate_index] = descriptor.time_ms + kPerfectWindowMs;
        }
        advance_note_lane_head_indices(candidate.note_ref);
        if (candidate.is_holding()) {
            active_hold_indices_[static_cast<size_t>(event.lane)] = descriptor.note_index;
        }
        judge_emit_options options;
        options.play_hitsound = i == 0;
        options.show_feedback = i == 0;
        emit_judge(result, offset_ms, event.lane, descriptor.event_index, options);
    }
}

void judge_system::resolve_stay_notes(double current_ms, const input_handler& input) {
    for (int lane = 0; lane < kMaxLanes; ++lane) {
        const std::vector<size_t>& lane_indices = lane_event_indices_[static_cast<size_t>(lane)];
        for (size_t i = lane_event_head_indices_[static_cast<size_t>(lane)]; i < lane_indices.size(); ++i) {
            const size_t event_index = lane_indices[i];
            if (event_completed_[event_index]) {
                continue;
            }
            const chart_judge_event& descriptor = event_descriptors_[event_index];
            if (current_ms < descriptor.time_ms) {
                break;
            }
            if (descriptor.kind != chart_judge_event_kind::stay) {
                continue;
            }
            if (input.is_lane_held(lane)) {
                complete_event(event_index, judge_result::perfect, 0.0);
            }
        }
    }
}

void judge_system::resolve_hold_completions(double current_ms) {
    for (int lane = 0; lane < kMaxLanes; ++lane) {
        const std::optional<size_t> active_hold = active_hold_indices_[static_cast<size_t>(lane)];
        if (!active_hold.has_value()) {
            continue;
        }

        // hold 中のノートが tail まで到達したら完了。tail 側も独立した判定として加点する。
        if (note_states_[*active_hold].is_holding() &&
            note_states_[*active_hold].tail_event_index >= 0 &&
            current_ms >= note_states_[*active_hold].end_target_ms) {
            complete_held_note(*active_hold, true);
        }
    }
}

void judge_system::resolve_auto_misses(double current_ms) {
    for (int lane = 0; lane < kMaxLanes; ++lane) {
        const std::vector<size_t>& lane_indices = lane_note_indices_[static_cast<size_t>(lane)];
        size_t& head_index = lane_head_indices_[static_cast<size_t>(lane)];

        while (head_index < lane_indices.size()) {
            note_state& state = note_states_[lane_indices[head_index]];
            if (state.is_judged()) {
                ++head_index;
                continue;
            }

            const double offset_ms = current_ms - state.target_ms;
            if (offset_ms <= kBadWindowMs) {
                break;
            }

            // head が最大判定幅を超えたら、そのノートはもう叩けない。
            state.progress = note_progress_state::completed;
            state.result = judge_result::miss;
            if (const std::optional<size_t> head_descriptor_index =
                    descriptor_index_for_event_index(state.head_event_index); head_descriptor_index.has_value()) {
                mark_event_completed(*head_descriptor_index);
            }
            emit_judge(judge_result::miss, offset_ms, state.note_ref.lane, state.head_event_index);
            if (state.note_ref.type == note_type::hold && state.tail_event_index >= 0) {
                if (const std::optional<size_t> tail_descriptor_index =
                        descriptor_index_for_event_index(state.tail_event_index); tail_descriptor_index.has_value()) {
                    mark_event_completed(*tail_descriptor_index);
                }
                judge_emit_options options;
                options.play_hitsound = false;
                options.show_feedback = false;
                emit_judge(judge_result::miss, current_ms - state.end_target_ms, state.note_ref.lane,
                           state.tail_event_index, options);
            }
            ++head_index;
        }
    }

    for (int lane = 0; lane < kMaxLanes; ++lane) {
        const std::vector<size_t>& lane_indices = lane_event_indices_[static_cast<size_t>(lane)];
        for (size_t i = lane_event_head_indices_[static_cast<size_t>(lane)]; i < lane_indices.size(); ++i) {
            const size_t event_index = lane_indices[i];
            if (event_completed_[event_index]) {
                continue;
            }
            const chart_judge_event& descriptor = event_descriptors_[event_index];
            if (current_ms - descriptor.time_ms <= kBadWindowMs) {
                break;
            }
            if (descriptor.kind == chart_judge_event_kind::press) {
                continue;
            }
            complete_event(event_index, judge_result::miss, current_ms - descriptor.time_ms);
        }
    }
}

void judge_system::advance_lane_head_index(int lane) {
    if (lane < 0 || lane >= kMaxLanes) {
        return;
    }

    const std::vector<size_t>& lane_indices = lane_note_indices_[static_cast<size_t>(lane)];
    size_t& head_index = lane_head_indices_[static_cast<size_t>(lane)];
    while (head_index < lane_indices.size() && note_states_[lane_indices[head_index]].is_judged()) {
        ++head_index;
    }
}

void judge_system::advance_lane_event_head_index(int lane) {
    if (lane < 0 || lane >= kMaxLanes) {
        return;
    }

    const std::vector<size_t>& lane_indices = lane_event_indices_[static_cast<size_t>(lane)];
    size_t& head_index = lane_event_head_indices_[static_cast<size_t>(lane)];
    while (head_index < lane_indices.size() && event_completed_[lane_indices[head_index]]) {
        ++head_index;
    }
}

void judge_system::advance_note_lane_head_indices(const note_data& note) {
    for (int lane = note.lane; lane <= note_last_lane(note) && lane < kMaxLanes; ++lane) {
        if (lane >= 0) {
            advance_lane_head_index(lane);
        }
    }
}

void judge_system::advance_event_lane_head_indices(const chart_judge_event& event) {
    for (int lane = event.lane; lane < event.lane + std::max(1, event.lane_width) && lane < kMaxLanes; ++lane) {
        if (lane >= 0) {
            advance_lane_event_head_index(lane);
        }
    }
}

void judge_system::clear_active_hold_for_note(size_t note_index) {
    if (note_index >= note_states_.size()) {
        return;
    }
    const note_data& note = note_states_[note_index].note_ref;
    for (int lane = note.lane; lane <= note_last_lane(note) && lane < kMaxLanes; ++lane) {
        if (lane < 0) {
            continue;
        }
        std::optional<size_t>& active_hold = active_hold_indices_[static_cast<size_t>(lane)];
        if (active_hold.has_value() && *active_hold == note_index) {
            active_hold.reset();
        }
    }
}

bool judge_system::mark_event_completed(size_t event_descriptor_index) {
    if (event_descriptor_index >= event_descriptors_.size() || event_completed_[event_descriptor_index]) {
        return false;
    }

    event_completed_[event_descriptor_index] = true;
    clear_armed_release_event(event_descriptor_index);
    advance_event_lane_head_indices(event_descriptors_[event_descriptor_index]);
    return true;
}

std::optional<size_t> judge_system::descriptor_index_for_event_index(int event_index) const {
    if (event_index < 0 ||
        static_cast<size_t>(event_index) >= event_descriptor_indices_by_event_index_.size()) {
        return std::nullopt;
    }

    const size_t descriptor_index = event_descriptor_indices_by_event_index_[static_cast<size_t>(event_index)];
    if (descriptor_index == kInvalidEventDescriptorIndex) {
        return std::nullopt;
    }
    return descriptor_index;
}

bool judge_system::release_overlaps_hold_tail(const chart_judge_event& release) const {
    const int release_last_lane = release.lane + std::max(1, release.lane_width) - 1;
    for (const chart_judge_event& descriptor : event_descriptors_) {
        if (descriptor.role != chart_judge_event_role::hold_tail ||
            !same_judgement_time(descriptor.time_ms, release.time_ms)) {
            continue;
        }
        const int descriptor_last_lane = descriptor.lane + std::max(1, descriptor.lane_width) - 1;
        if (descriptor.lane <= release_last_lane && release.lane <= descriptor_last_lane) {
            return true;
        }
    }
    return false;
}

bool judge_system::try_absorb_completed_wide_press(const input_event& event) {
    if (event.lane < 0 || event.lane >= kMaxLanes) {
        return false;
    }

    std::optional<size_t> absorbed_descriptor_index;
    double best_abs_offset = kPerfectWindowMs + 1.0;
    for (size_t i = 0; i < event_descriptors_.size(); ++i) {
        if (!event_completed_[i] ||
            i >= completed_wide_press_absorb_until_ms_.size() ||
            event.timestamp_ms > completed_wide_press_absorb_until_ms_[i]) {
            continue;
        }

        const chart_judge_event& descriptor = event_descriptors_[i];
        if (descriptor.kind != chart_judge_event_kind::press ||
            descriptor.lane_width <= 1 ||
            event.lane < descriptor.lane ||
            event.lane >= descriptor.lane + descriptor.lane_width) {
            continue;
        }

        const double offset_ms = event.timestamp_ms - descriptor.time_ms;
        if (std::fabs(offset_ms) > kPerfectWindowMs) {
            continue;
        }

        const double abs_offset = std::fabs(offset_ms);
        if (abs_offset < best_abs_offset) {
            absorbed_descriptor_index = i;
            best_abs_offset = abs_offset;
        }
    }

    if (!absorbed_descriptor_index.has_value()) {
        return false;
    }

    const chart_judge_event& descriptor = event_descriptors_[*absorbed_descriptor_index];
    if (descriptor.role == chart_judge_event_role::hold_head &&
        descriptor.note_index < note_states_.size() &&
        note_states_[descriptor.note_index].is_holding()) {
        active_hold_indices_[static_cast<size_t>(event.lane)] = descriptor.note_index;
    }
    return true;
}

std::vector<size_t> judge_system::find_press_candidates(int lane, double timestamp_ms) {
    std::vector<size_t> candidates;
    if (lane < 0 || lane >= kMaxLanes) {
        return candidates;
    }

    const std::vector<size_t>& lane_indices = lane_event_indices_[static_cast<size_t>(lane)];
    std::optional<double> target_time_ms;
    double best_abs_offset = kBadWindowMs + 1.0;
    for (size_t i = lane_event_head_indices_[static_cast<size_t>(lane)]; i < lane_indices.size(); ++i) {
        const size_t descriptor_index = lane_indices[i];
        const chart_judge_event& descriptor = event_descriptors_[descriptor_index];
        if (event_completed_[descriptor_index]) {
            continue;
        }
        if (descriptor.kind != chart_judge_event_kind::press &&
            descriptor.kind != chart_judge_event_kind::stay) {
            continue;
        }

        const double offset_ms = timestamp_ms - descriptor.time_ms;
        if (descriptor.kind == chart_judge_event_kind::stay && offset_ms < 0.0) {
            continue;
        }
        if (offset_ms < -kBadWindowMs) {
            // 同レーン内は時間順なので、ここで早すぎるなら後続ノートもまだ早い。
            break;
        }
        if (!is_in_judgement_window(offset_ms)) {
            continue;
        }

        const double abs_offset = std::fabs(offset_ms);
        if (abs_offset < best_abs_offset) {
            target_time_ms = descriptor.time_ms;
            best_abs_offset = abs_offset;
            candidates.clear();
            candidates.push_back(descriptor_index);
        } else if (target_time_ms.has_value() && same_judgement_time(descriptor.time_ms, *target_time_ms)) {
            candidates.push_back(descriptor_index);
        }
    }
    return candidates;
}

std::vector<size_t> judge_system::find_release_candidates(int lane, double timestamp_ms) {
    std::vector<size_t> candidates;
    if (lane < 0 || lane >= kMaxLanes) {
        return candidates;
    }

    const std::vector<size_t>& lane_indices = lane_event_indices_[static_cast<size_t>(lane)];
    std::optional<double> target_time_ms;
    double best_abs_offset = kBadWindowMs + 1.0;
    for (size_t i = lane_event_head_indices_[static_cast<size_t>(lane)]; i < lane_indices.size(); ++i) {
        const size_t descriptor_index = lane_indices[i];
        const chart_judge_event& descriptor = event_descriptors_[descriptor_index];
        if (event_completed_[descriptor_index] ||
            descriptor.kind != chart_judge_event_kind::release) {
            continue;
        }
        if (descriptor.role == chart_judge_event_role::hold_tail) {
            const std::optional<size_t>& active_hold = active_hold_indices_[static_cast<size_t>(lane)];
            if (!active_hold.has_value() || *active_hold != descriptor.note_index ||
                !note_states_[*active_hold].is_holding()) {
                continue;
            }
        } else if (descriptor.role == chart_judge_event_role::release &&
                   is_standalone_release_event(descriptor_index) &&
                   !release_event_is_armed(descriptor_index, lane)) {
            continue;
        }

        const double offset_ms = timestamp_ms - descriptor.time_ms;
        if (offset_ms < -kBadWindowMs) {
            break;
        }
        if (!is_in_judgement_window(offset_ms)) {
            continue;
        }

        const double abs_offset = std::fabs(offset_ms);
        if (abs_offset < best_abs_offset) {
            target_time_ms = descriptor.time_ms;
            best_abs_offset = abs_offset;
            candidates.clear();
            candidates.push_back(descriptor_index);
        } else if (target_time_ms.has_value() && same_judgement_time(descriptor.time_ms, *target_time_ms)) {
            candidates.push_back(descriptor_index);
        }
    }
    return candidates;
}

std::vector<size_t> judge_system::find_early_release_stay_candidates(int lane, double timestamp_ms) {
    std::vector<size_t> candidates;
    if (lane < 0 || lane >= kMaxLanes) {
        return candidates;
    }

    std::optional<double> target_time_ms;
    double best_abs_offset = kBadWindowMs + 1.0;
    const std::vector<size_t>& lane_indices = lane_event_indices_[static_cast<size_t>(lane)];
    for (size_t i = lane_event_head_indices_[static_cast<size_t>(lane)]; i < lane_indices.size(); ++i) {
        const size_t event_index = lane_indices[i];
        if (event_completed_[event_index]) {
            continue;
        }
        const chart_judge_event& descriptor = event_descriptors_[event_index];
        if (descriptor.kind != chart_judge_event_kind::stay) {
            continue;
        }
        const double offset_ms = timestamp_ms - descriptor.time_ms;
        if (offset_ms >= 0.0 || !is_in_judgement_window(offset_ms)) {
            continue;
        }
        const double abs_offset = std::fabs(offset_ms);
        if (abs_offset < best_abs_offset) {
            target_time_ms = descriptor.time_ms;
            best_abs_offset = abs_offset;
            candidates.clear();
            candidates.push_back(event_index);
        } else if (target_time_ms.has_value() && same_judgement_time(descriptor.time_ms, *target_time_ms)) {
            candidates.push_back(event_index);
        }
    }
    return candidates;
}

void judge_system::arm_release_candidate(int lane, double timestamp_ms) {
    if (lane < 0 || lane >= kMaxLanes) {
        return;
    }

    std::optional<size_t> target_event_index;
    double best_abs_offset = kBadWindowMs + 1.0;
    const std::vector<size_t>& lane_indices = lane_event_indices_[static_cast<size_t>(lane)];
    for (size_t i = lane_event_head_indices_[static_cast<size_t>(lane)]; i < lane_indices.size(); ++i) {
        const size_t event_index = lane_indices[i];
        if (event_completed_[event_index]) {
            continue;
        }
        const chart_judge_event& descriptor = event_descriptors_[event_index];
        if (timestamp_ms < descriptor.time_ms - kBadWindowMs) {
            break;
        }
        if (descriptor.role != chart_judge_event_role::release ||
            !is_standalone_release_event(event_index)) {
            continue;
        }

        const double offset_ms = timestamp_ms - descriptor.time_ms;
        if (offset_ms > 0.0 || offset_ms < -kBadWindowMs) {
            continue;
        }

        const double abs_offset = std::fabs(offset_ms);
        if (abs_offset < best_abs_offset) {
            target_event_index = event_index;
            best_abs_offset = abs_offset;
        }
    }

    if (target_event_index.has_value()) {
        armed_release_event_indices_[static_cast<size_t>(lane)] = *target_event_index;
    }
}

bool judge_system::is_standalone_release_event(size_t event_descriptor_index) const {
    if (event_descriptor_index >= standalone_release_events_.size()) {
        return false;
    }
    return standalone_release_events_[event_descriptor_index];
}

bool judge_system::release_event_is_armed(size_t event_descriptor_index, int lane) const {
    if (lane < 0 || lane >= kMaxLanes) {
        return false;
    }
    const std::optional<size_t>& armed_event = armed_release_event_indices_[static_cast<size_t>(lane)];
    return armed_event.has_value() && *armed_event == event_descriptor_index;
}

void judge_system::clear_armed_release_event(size_t event_descriptor_index) {
    for (std::optional<size_t>& armed_event : armed_release_event_indices_) {
        if (armed_event.has_value() && *armed_event == event_descriptor_index) {
            armed_event.reset();
        }
    }
}

void judge_system::complete_held_note(size_t note_index, bool emit_display_judge) {
    note_state& state = note_states_[note_index];
    clear_active_hold_for_note(note_index);
    state.result = judge_result::perfect;
    state.progress = note_progress_state::completed;
    advance_note_lane_head_indices(state.note_ref);
    if (state.tail_event_index < 0) {
        return;
    }
    if (const std::optional<size_t> tail_descriptor_index =
            descriptor_index_for_event_index(state.tail_event_index); !tail_descriptor_index.has_value() ||
            !mark_event_completed(*tail_descriptor_index)) {
        return;
    }
    judge_emit_options options;
    options.play_hitsound = false;
    options.apply_gameplay_effects = true;
    options.show_feedback = emit_display_judge;
    emit_judge(judge_result::perfect, 0.0, state.note_ref.lane, state.tail_event_index, options);
}

void judge_system::complete_event(size_t event_descriptor_index, judge_result result, double offset_ms) {
    complete_event(event_descriptor_index, result, offset_ms, judge_emit_options{});
}

void judge_system::complete_event(size_t event_descriptor_index, judge_result result, double offset_ms,
                                  judge_emit_options options) {
    if (event_descriptor_index >= event_descriptors_.size() || event_completed_[event_descriptor_index]) {
        return;
    }

    const chart_judge_event& descriptor = event_descriptors_[event_descriptor_index];
    mark_event_completed(event_descriptor_index);
    if (descriptor.note_index < note_states_.size()) {
        note_state& state = note_states_[descriptor.note_index];
        state.result = result;
        if (state.note_ref.type != note_type::hold || descriptor.role == chart_judge_event_role::hold_tail) {
            state.progress = note_progress_state::completed;
        }
    }
    if (descriptor.role == chart_judge_event_role::hold_tail) {
        clear_active_hold_for_note(descriptor.note_index);
    }
    if (descriptor.role == chart_judge_event_role::release) {
        std::optional<size_t>& active_hold = active_hold_indices_[static_cast<size_t>(descriptor.lane)];
        if (active_hold.has_value() &&
            note_states_[*active_hold].is_holding() &&
            std::fabs(note_states_[*active_hold].end_target_ms - descriptor.time_ms) <= 0.5) {
            const size_t note_index = *active_hold;
            note_states_[note_index].progress = note_progress_state::completed;
            note_states_[note_index].result = result;
            clear_active_hold_for_note(note_index);
        }
    }
    emit_judge(result, offset_ms, descriptor.lane, descriptor.event_index, options);
}

void judge_system::emit_judge(judge_result result, double offset_ms, int lane, int event_index) {
    emit_judge(result, offset_ms, lane, event_index, judge_emit_options{});
}

void judge_system::emit_judge(judge_result result, double offset_ms, int lane,
                              int event_index,
                              judge_emit_options options) {
    bool is_ray = false;
    note_type hitsound_type = note_type::tap;
    int lane_width = 1;
    int display_lane = lane;
    if (const std::optional<size_t> descriptor_index = descriptor_index_for_event_index(event_index);
        descriptor_index.has_value()) {
        const chart_judge_event& descriptor = event_descriptors_[*descriptor_index];
        is_ray = descriptor.is_ray;
        lane_width = std::max(1, descriptor.lane_width);
        display_lane = descriptor.lane;
        if (descriptor.role == chart_judge_event_role::release) {
            hitsound_type = note_type::release;
        } else if (descriptor.role == chart_judge_event_role::stay) {
            hitsound_type = note_type::stay;
        }
    }
    judge_event event{result, offset_ms, display_lane,
                      options.play_hitsound,
                      options.apply_gameplay_effects,
                      options.show_feedback,
                      event_index,
                      hitsound_type,
                      is_ray,
                      lane_width};
    judge_events_.push_back(event);
}
