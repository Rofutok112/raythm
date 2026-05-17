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
    held_input_session_by_lane_.fill(std::nullopt);
    input_sessions_.clear();
    active_hold_sessions_.clear();
    judge_events_.clear();
    event_descriptor_indices_by_event_index_.clear();

    for (size_t i = 0; i < notes.size(); ++i) {
        const note_data& note = notes[i];
        note_state state;
        state.note_ref = note;
        state.target_ms = engine.tick_to_ms(note.tick);
        state.end_target_ms = engine.tick_to_ms(note.type == note_type::hold ? note.end_tick : note.tick);
        note_states_.push_back(state);
    }
    active_hold_sessions_.resize(note_states_.size());
    for (size_t i = 0; i < active_hold_sessions_.size(); ++i) {
        active_hold_sessions_[i].note_index = i;
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

    resolve_hold_heads_from_nearby_stays(current_ms, input);
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

    const std::optional<input_session_id> input_id = held_input_session_by_lane_[static_cast<size_t>(lane)];
    const input_session* input = input_id.has_value() ? session_for_id(*input_id) : nullptr;
    if (input == nullptr || !input->active_hold_note_index.has_value()) {
        return;
    }

    const size_t note_index = *input->active_hold_note_index;
    if (note_index >= note_states_.size()) {
        return;
    }

    const note_state& state = note_states_[note_index];
    // 同じレーンで tail 通過後に押された場合は、先に前の hold を完了扱いにする。
    if (state.is_holding() && timestamp_ms >= state.end_target_ms) {
        complete_held_note(note_index, false);
    }
}

void judge_system::handle_hold_release(const input_event& event) {
    if (event.lane < 0 || event.lane >= kMaxLanes) {
        return;
    }

    input_session* input = release_input_session(event);
    const std::optional<input_session_id> input_id = input != nullptr
                                                        ? std::optional<input_session_id>(input->id)
                                                        : std::nullopt;

    const std::vector<size_t> release_candidates = find_release_candidates(event.lane, event.timestamp_ms, input_id);
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
        const chart_judge_event& descriptor = event_descriptors_[release_candidates.front()];
        const double offset_ms = event.timestamp_ms - descriptor.time_ms;
        if (input != nullptr &&
            input->active_hold_note_index.has_value() &&
            *input->active_hold_note_index < note_states_.size() &&
            note_states_[*input->active_hold_note_index].is_holding() &&
            event.timestamp_ms >= note_states_[*input->active_hold_note_index].end_target_ms - kBadWindowMs) {
            const size_t note_index = *input->active_hold_note_index;
            note_states_[note_index].progress = note_progress_state::completed;
            clear_active_hold_for_note(note_index);
        }
        return;
    }

    if (input == nullptr || !input->active_hold_note_index.has_value()) {
        if (complete_armed_stay_candidate(input, event)) {
            return;
        }
        return;
    }

    const size_t note_index = *input->active_hold_note_index;
    note_state& state = note_states_[note_index];
    if (!state.is_holding()) {
        input->active_hold_note_index.reset();
        return;
    }

    if (event.timestamp_ms >= state.end_target_ms) {
        complete_held_note(note_index, false);
        return;
    }

    deactivate_hold_lane(note_index, event.lane, input->id);
    if (has_active_hold_for_note(note_index)) {
        return;
    }

    // tail 到達前の release は、その時点で判定付きの完了として確定する。
    const double release_offset_ms = event.timestamp_ms - state.end_target_ms;
    state.progress = note_progress_state::completed;
    state.result = evaluate_hold_release_offset(release_offset_ms);
    clear_active_hold_for_note(note_index);
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

    const input_session_id input_id = begin_input_session(event);

    complete_due_hold_before(event.lane, event.timestamp_ms);

    if (try_absorb_completed_wide_press(event, input_id)) {
        return;
    }

    const std::vector<size_t> candidate_indices = find_press_candidates(event.lane, event.timestamp_ms);
    if (candidate_indices.empty()) {
        if (try_adopt_active_wide_hold_lane(event, input_id)) {
            return;
        }
        if (!arm_release_candidate(input_id, event.timestamp_ms)) {
            arm_stay_candidate(input_id, event.timestamp_ms);
        }
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
        if (candidate.is_holding()) {
            activate_hold_lane(descriptor.note_index, event.lane, input_id);
        }
        judge_emit_options options;
        options.play_hitsound = i == 0;
        options.show_feedback = i == 0;
        emit_judge(result, offset_ms, event.lane, descriptor.event_index, options);
    }
}

void judge_system::resolve_hold_heads_from_nearby_stays(double current_ms, const input_handler& input) {
    for (size_t event_index = 0; event_index < event_descriptors_.size(); ++event_index) {
        if (event_completed_[event_index]) {
            continue;
        }

        const chart_judge_event& descriptor = event_descriptors_[event_index];
        if (descriptor.kind != chart_judge_event_kind::press ||
            descriptor.role != chart_judge_event_role::hold_head ||
            current_ms < descriptor.time_ms ||
            current_ms - descriptor.time_ms > kBadWindowMs ||
            descriptor.note_index >= note_states_.size() ||
            !has_held_nearby_stay(input, descriptor)) {
            continue;
        }

        for (int lane = descriptor.lane;
             lane < descriptor.lane + std::max(1, descriptor.lane_width) && lane < kMaxLanes;
             ++lane) {
            if (!input.is_lane_held(lane) || !lane_was_held_at(lane, descriptor.time_ms)) {
                continue;
            }

            const std::optional<input_session_id> input_id = held_input_session_by_lane_[static_cast<size_t>(lane)];
            note_state& state = note_states_[descriptor.note_index];
            state.progress = note_progress_state::holding;
            state.result = judge_result::perfect;
            mark_event_completed(event_index);
            if (input_id.has_value()) {
                activate_hold_lane(descriptor.note_index, lane, *input_id);
            }

            judge_emit_options options;
            options.play_hitsound = false;
            emit_judge(judge_result::perfect, 0.0, lane, descriptor.event_index, options);
            break;
        }
    }
}

void judge_system::resolve_stay_notes(double current_ms, const input_handler& input) {
    for (size_t event_index = 0; event_index < event_descriptors_.size(); ++event_index) {
        if (event_completed_[event_index]) {
            continue;
        }
        const chart_judge_event& descriptor = event_descriptors_[event_index];
        if (current_ms < descriptor.time_ms || descriptor.kind != chart_judge_event_kind::stay) {
            continue;
        }
        for (int lane = descriptor.lane;
             lane < descriptor.lane + std::max(1, descriptor.lane_width) && lane < kMaxLanes;
             ++lane) {
            if (lane_is_held_for_stay(input, lane, descriptor.time_ms)) {
                complete_event(event_index, judge_result::perfect, 0.0);
                break;
            }
        }
    }
}

void judge_system::resolve_hold_completions(double current_ms) {
    for (const active_hold_session& session : active_hold_sessions_) {
        if (!session.active || session.note_index >= note_states_.size()) {
            continue;
        }

        // hold 中のノートが tail まで到達したら完了。tail 側も独立した判定として加点する。
        if (note_states_[session.note_index].is_holding() &&
            note_states_[session.note_index].tail_event_index >= 0 &&
            current_ms >= note_states_[session.note_index].end_target_ms) {
            complete_held_note(session.note_index, true);
        }
    }
}

void judge_system::resolve_auto_misses(double current_ms) {
    for (note_state& state : note_states_) {
        if (state.is_judged()) {
            continue;
        }

        const double offset_ms = current_ms - state.target_ms;
        if (offset_ms <= kBadWindowMs) {
            continue;
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
    }

    for (size_t event_index = 0; event_index < event_descriptors_.size(); ++event_index) {
        if (event_completed_[event_index]) {
            continue;
        }
        const chart_judge_event& descriptor = event_descriptors_[event_index];
        if (current_ms - descriptor.time_ms <= kBadWindowMs ||
            descriptor.kind == chart_judge_event_kind::press) {
            continue;
        }
        complete_event(event_index, judge_result::miss, current_ms - descriptor.time_ms);
    }
}

judge_system::input_session* judge_system::session_for_id(input_session_id session_id) {
    if (session_id >= input_sessions_.size()) {
        return nullptr;
    }
    return &input_sessions_[session_id];
}

const judge_system::input_session* judge_system::session_for_id(input_session_id session_id) const {
    if (session_id >= input_sessions_.size()) {
        return nullptr;
    }
    return &input_sessions_[session_id];
}

judge_system::input_session_id judge_system::begin_input_session(const input_event& event) {
    const input_session_id session_id = input_sessions_.size();
    input_session session;
    session.id = session_id;
    session.lane = event.lane;
    session.press_ms = event.timestamp_ms;
    input_sessions_.push_back(session);
    held_input_session_by_lane_[static_cast<size_t>(event.lane)] = session_id;
    return session_id;
}

judge_system::input_session* judge_system::release_input_session(const input_event& event) {
    const std::optional<input_session_id> session_id = held_input_session_by_lane_[static_cast<size_t>(event.lane)];
    if (!session_id.has_value()) {
        return nullptr;
    }

    input_session* session = session_for_id(*session_id);
    held_input_session_by_lane_[static_cast<size_t>(event.lane)].reset();
    if (session == nullptr || !session->held) {
        return nullptr;
    }
    session->held = false;
    session->release_ms = event.timestamp_ms;
    return session;
}

judge_system::active_hold_session* judge_system::active_hold_session_for_note(size_t note_index) {
    if (note_index >= active_hold_sessions_.size()) {
        return nullptr;
    }
    active_hold_session& session = active_hold_sessions_[note_index];
    return session.active ? &session : nullptr;
}

const judge_system::active_hold_session* judge_system::active_hold_session_for_note(size_t note_index) const {
    if (note_index >= active_hold_sessions_.size()) {
        return nullptr;
    }
    const active_hold_session& session = active_hold_sessions_[note_index];
    return session.active ? &session : nullptr;
}

bool judge_system::activate_hold_lane(size_t note_index, int lane, input_session_id input_id) {
    input_session* input = session_for_id(input_id);
    if (note_index >= note_states_.size() ||
        note_index >= active_hold_sessions_.size() ||
        lane < 0 ||
        lane >= kMaxLanes ||
        input == nullptr ||
        !input->held ||
        input->lane != lane ||
        !note_states_[note_index].is_holding() ||
        !note_covers_lane(note_states_[note_index].note_ref, lane)) {
        return false;
    }

    active_hold_session& session = active_hold_sessions_[note_index];
    session.active = true;
    session.note_index = note_index;
    session.adopted_sessions[static_cast<size_t>(lane)] = input_id;
    input->active_hold_note_index = note_index;
    return true;
}

void judge_system::deactivate_hold_lane(size_t note_index, int lane, input_session_id input_id) {
    active_hold_session* session = active_hold_session_for_note(note_index);
    if (session == nullptr || lane < 0 || lane >= kMaxLanes) {
        return;
    }

    std::optional<input_session_id>& adopted_session = session->adopted_sessions[static_cast<size_t>(lane)];
    if (!adopted_session.has_value() || *adopted_session != input_id) {
        return;
    }

    adopted_session.reset();
    if (input_session* input = session_for_id(input_id); input != nullptr &&
        input->active_hold_note_index.has_value() &&
        *input->active_hold_note_index == note_index) {
        input->active_hold_note_index.reset();
    }
    session->active = has_active_hold_for_note(note_index);
}

void judge_system::clear_active_hold_for_note(size_t note_index) {
    if (note_index >= note_states_.size()) {
        return;
    }
    if (note_index < active_hold_sessions_.size()) {
        for (const std::optional<input_session_id>& input_id : active_hold_sessions_[note_index].adopted_sessions) {
            if (input_id.has_value()) {
                if (input_session* input = session_for_id(*input_id); input != nullptr &&
                    input->active_hold_note_index.has_value() &&
                    *input->active_hold_note_index == note_index) {
                    input->active_hold_note_index.reset();
                }
            }
        }
        active_hold_sessions_[note_index].active = false;
        active_hold_sessions_[note_index].adopted_sessions.fill(std::nullopt);
    }
}

bool judge_system::has_active_hold_for_note(size_t note_index) const {
    const active_hold_session* session = active_hold_session_for_note(note_index);
    if (session == nullptr) {
        return false;
    }
    for (const std::optional<input_session_id>& input_id : session->adopted_sessions) {
        if (!input_id.has_value()) {
            continue;
        }
        const input_session* input = session_for_id(*input_id);
        if (input != nullptr && input->held) {
            return true;
        }
    }
    return false;
}

bool judge_system::lane_is_held_for_stay(const input_handler& input, int lane, double timestamp_ms) const {
    if (lane < 0 || lane >= kMaxLanes) {
        return false;
    }

    return input.is_lane_held(lane) || lane_was_held_at(lane, timestamp_ms);
}

bool judge_system::lane_was_held_at(int lane, double timestamp_ms) const {
    if (lane < 0 || lane >= kMaxLanes) {
        return false;
    }

    for (const input_session& input : input_sessions_) {
        if (input.lane != lane || input.press_ms > timestamp_ms) {
            continue;
        }
        if (input.held || input.release_ms >= timestamp_ms) {
            return true;
        }
    }
    return false;
}

bool judge_system::has_held_nearby_stay(const input_handler& input, const chart_judge_event& hold_head) const {
    for (const chart_judge_event& descriptor : event_descriptors_) {
        if (descriptor.kind != chart_judge_event_kind::stay) {
            continue;
        }
        if (std::fabs(descriptor.time_ms - hold_head.time_ms) > kBadWindowMs) {
            continue;
        }
        if (hold_head.lane >= descriptor.lane + std::max(1, descriptor.lane_width) ||
            descriptor.lane >= hold_head.lane + std::max(1, hold_head.lane_width)) {
            continue;
        }

        for (int lane = std::max(hold_head.lane, descriptor.lane);
             lane < std::min(hold_head.lane + std::max(1, hold_head.lane_width),
                             descriptor.lane + std::max(1, descriptor.lane_width)) &&
             lane < kMaxLanes;
             ++lane) {
            if (input.is_lane_held(lane) && lane_was_held_at(lane, descriptor.time_ms)) {
                return true;
            }
        }
    }
    return false;
}

bool judge_system::mark_event_completed(size_t event_descriptor_index) {
    if (event_descriptor_index >= event_descriptors_.size() || event_completed_[event_descriptor_index]) {
        return false;
    }

    event_completed_[event_descriptor_index] = true;
    clear_armed_release_event(event_descriptor_index);
    clear_armed_stay_event(event_descriptor_index);
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

bool judge_system::try_absorb_completed_wide_press(const input_event& event, input_session_id input_id) {
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
        activate_hold_lane(descriptor.note_index, event.lane, input_id);
    }
    return true;
}

bool judge_system::try_adopt_active_wide_hold_lane(const input_event& event, input_session_id input_id) {
    const input_session* input = session_for_id(input_id);
    if (event.lane < 0 || event.lane >= kMaxLanes ||
        input == nullptr ||
        input->active_hold_note_index.has_value()) {
        return false;
    }

    std::optional<size_t> adopted_note_index;
    double adopted_end_ms = 0.0;
    for (const active_hold_session& session : active_hold_sessions_) {
        if (!session.active || session.note_index >= note_states_.size()) {
            continue;
        }

        const note_state& state = note_states_[session.note_index];
        if (!state.is_holding() ||
            state.note_ref.type != note_type::hold ||
            note_lane_width(state.note_ref) <= 1 ||
            !note_covers_lane(state.note_ref, event.lane) ||
            event.timestamp_ms < state.target_ms ||
            event.timestamp_ms >= state.end_target_ms) {
            continue;
        }

        if (!adopted_note_index.has_value() || state.end_target_ms < adopted_end_ms) {
            adopted_note_index = session.note_index;
            adopted_end_ms = state.end_target_ms;
        }
    }

    if (!adopted_note_index.has_value()) {
        return false;
    }

    return activate_hold_lane(*adopted_note_index, event.lane, input_id);
}

std::vector<size_t> judge_system::find_press_candidates(int lane, double timestamp_ms) {
    std::vector<size_t> candidates;
    if (lane < 0 || lane >= kMaxLanes) {
        return candidates;
    }

    std::optional<double> press_target_time_ms;
    double best_press_abs_offset = kBadWindowMs + 1.0;
    for (size_t descriptor_index = 0; descriptor_index < event_descriptors_.size(); ++descriptor_index) {
        const chart_judge_event& descriptor = event_descriptors_[descriptor_index];
        if (event_completed_[descriptor_index]) {
            continue;
        }
        if (lane < descriptor.lane || lane >= descriptor.lane + std::max(1, descriptor.lane_width)) {
            continue;
        }
        if (descriptor.kind != chart_judge_event_kind::press) {
            continue;
        }

        const double offset_ms = timestamp_ms - descriptor.time_ms;
        if (!is_in_judgement_window(offset_ms)) {
            continue;
        }

        const double abs_offset = std::fabs(offset_ms);
        if (abs_offset < best_press_abs_offset) {
            press_target_time_ms = descriptor.time_ms;
            best_press_abs_offset = abs_offset;
        }
    }

    if (press_target_time_ms.has_value()) {
        for (size_t descriptor_index = 0; descriptor_index < event_descriptors_.size(); ++descriptor_index) {
            const chart_judge_event& descriptor = event_descriptors_[descriptor_index];
            if (event_completed_[descriptor_index]) {
                continue;
            }
            if (lane < descriptor.lane || lane >= descriptor.lane + std::max(1, descriptor.lane_width)) {
                continue;
            }
            if (descriptor.kind != chart_judge_event_kind::press &&
                descriptor.kind != chart_judge_event_kind::stay) {
                continue;
            }
            if (!same_judgement_time(descriptor.time_ms, *press_target_time_ms)) {
                continue;
            }

            const double offset_ms = timestamp_ms - descriptor.time_ms;
            if (descriptor.kind == chart_judge_event_kind::stay && offset_ms < 0.0) {
                continue;
            }
            if (!is_in_judgement_window(offset_ms)) {
                continue;
            }

            candidates.push_back(descriptor_index);
        }
        return candidates;
    }

    std::optional<double> stay_target_time_ms;
    double best_stay_abs_offset = kBadWindowMs + 1.0;
    for (size_t descriptor_index = 0; descriptor_index < event_descriptors_.size(); ++descriptor_index) {
        const chart_judge_event& descriptor = event_descriptors_[descriptor_index];
        if (event_completed_[descriptor_index]) {
            continue;
        }
        if (lane < descriptor.lane || lane >= descriptor.lane + std::max(1, descriptor.lane_width)) {
            continue;
        }
        if (descriptor.kind != chart_judge_event_kind::stay) {
            continue;
        }
        const double offset_ms = timestamp_ms - descriptor.time_ms;
        if (offset_ms < 0.0 || !is_in_judgement_window(offset_ms)) {
            continue;
        }

        const double abs_offset = std::fabs(offset_ms);
        if (abs_offset < best_stay_abs_offset) {
            stay_target_time_ms = descriptor.time_ms;
            best_stay_abs_offset = abs_offset;
            candidates.clear();
            candidates.push_back(descriptor_index);
        } else if (stay_target_time_ms.has_value() && same_judgement_time(descriptor.time_ms, *stay_target_time_ms)) {
            candidates.push_back(descriptor_index);
        }
    }
    return candidates;
}

std::vector<size_t> judge_system::find_release_candidates(int lane, double timestamp_ms,
                                                          std::optional<input_session_id> input_id) {
    std::vector<size_t> candidates;
    if (lane < 0 || lane >= kMaxLanes) {
        return candidates;
    }

    std::optional<double> target_time_ms;
    double best_abs_offset = kBadWindowMs + 1.0;
    for (size_t descriptor_index = 0; descriptor_index < event_descriptors_.size(); ++descriptor_index) {
        const chart_judge_event& descriptor = event_descriptors_[descriptor_index];
        if (event_completed_[descriptor_index] ||
            descriptor.kind != chart_judge_event_kind::release) {
            continue;
        }
        if (lane < descriptor.lane || lane >= descriptor.lane + std::max(1, descriptor.lane_width)) {
            continue;
        }
        if (descriptor.role == chart_judge_event_role::hold_tail) {
            const input_session* input = input_id.has_value() ? session_for_id(*input_id) : nullptr;
            if (input == nullptr ||
                !input->active_hold_note_index.has_value() ||
                *input->active_hold_note_index != descriptor.note_index ||
                !note_states_[descriptor.note_index].is_holding()) {
                continue;
            }
        } else if (descriptor.role == chart_judge_event_role::release &&
                   is_standalone_release_event(descriptor_index) &&
                   !release_event_is_armed(descriptor_index, input_id)) {
            continue;
        }

        const double offset_ms = timestamp_ms - descriptor.time_ms;
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

bool judge_system::arm_release_candidate(input_session_id input_id, double timestamp_ms) {
    input_session* input = session_for_id(input_id);
    if (input == nullptr || input->lane < 0 || input->lane >= kMaxLanes) {
        return false;
    }

    std::optional<size_t> target_event_index;
    double best_abs_offset = kBadWindowMs + 1.0;
    for (size_t event_index = 0; event_index < event_descriptors_.size(); ++event_index) {
        if (event_completed_[event_index]) {
            continue;
        }
        const chart_judge_event& descriptor = event_descriptors_[event_index];
        if (input->lane < descriptor.lane ||
            input->lane >= descriptor.lane + std::max(1, descriptor.lane_width)) {
            continue;
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
        input->armed_release_event_index = *target_event_index;
        return true;
    }
    return false;
}

void judge_system::arm_stay_candidate(input_session_id input_id, double timestamp_ms) {
    input_session* input = session_for_id(input_id);
    if (input == nullptr || input->lane < 0 || input->lane >= kMaxLanes) {
        return;
    }

    std::optional<size_t> target_event_index;
    double best_abs_offset = kBadWindowMs + 1.0;
    for (size_t event_index = 0; event_index < event_descriptors_.size(); ++event_index) {
        if (event_completed_[event_index]) {
            continue;
        }
        const chart_judge_event& descriptor = event_descriptors_[event_index];
        if (input->lane < descriptor.lane ||
            input->lane >= descriptor.lane + std::max(1, descriptor.lane_width)) {
            continue;
        }
        if (descriptor.kind != chart_judge_event_kind::stay) {
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
        input->armed_stay_event_index = *target_event_index;
    }
}

bool judge_system::complete_armed_stay_candidate(input_session* input, const input_event& event) {
    if (input == nullptr || !input->armed_stay_event_index.has_value()) {
        return false;
    }

    const size_t event_index = *input->armed_stay_event_index;
    if (event_index >= event_descriptors_.size() || event_completed_[event_index]) {
        input->armed_stay_event_index.reset();
        return false;
    }

    const chart_judge_event& descriptor = event_descriptors_[event_index];
    if (descriptor.kind != chart_judge_event_kind::stay ||
        event.lane < descriptor.lane ||
        event.lane >= descriptor.lane + std::max(1, descriptor.lane_width)) {
        return false;
    }

    const double offset_ms = event.timestamp_ms - descriptor.time_ms;
    if (offset_ms < -kBadWindowMs) {
        return false;
    }

    const double judged_offset_ms = offset_ms < 0.0 ? offset_ms : 0.0;
    const judge_result result = offset_ms < 0.0 ? evaluate_stay_offset(offset_ms) : judge_result::perfect;
    complete_event(event_index, result, judged_offset_ms);
    return true;
}

bool judge_system::is_standalone_release_event(size_t event_descriptor_index) const {
    if (event_descriptor_index >= standalone_release_events_.size()) {
        return false;
    }
    return standalone_release_events_[event_descriptor_index];
}

bool judge_system::release_event_is_armed(size_t event_descriptor_index,
                                          std::optional<input_session_id> input_id) const {
    if (!input_id.has_value()) {
        return false;
    }
    const input_session* input = session_for_id(*input_id);
    return input != nullptr &&
           input->armed_release_event_index.has_value() &&
           *input->armed_release_event_index == event_descriptor_index;
}

void judge_system::clear_armed_release_event(size_t event_descriptor_index) {
    for (input_session& input : input_sessions_) {
        if (input.armed_release_event_index.has_value() &&
            *input.armed_release_event_index == event_descriptor_index) {
            input.armed_release_event_index.reset();
        }
    }
}

void judge_system::clear_armed_stay_event(size_t event_descriptor_index) {
    for (input_session& input : input_sessions_) {
        if (input.armed_stay_event_index.has_value() &&
            *input.armed_stay_event_index == event_descriptor_index) {
            input.armed_stay_event_index.reset();
        }
    }
}

void judge_system::complete_held_note(size_t note_index, bool emit_display_judge) {
    note_state& state = note_states_[note_index];
    clear_active_hold_for_note(note_index);
    state.result = judge_result::perfect;
    state.progress = note_progress_state::completed;
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
        for (active_hold_session& session : active_hold_sessions_) {
            if (!session.active || session.note_index >= note_states_.size()) {
                continue;
            }
            note_state& hold_state = note_states_[session.note_index];
            if (!hold_state.is_holding() ||
                !note_covers_lane(hold_state.note_ref, descriptor.lane) ||
                std::fabs(hold_state.end_target_ms - descriptor.time_ms) > 0.5) {
                continue;
            }
            hold_state.progress = note_progress_state::completed;
            hold_state.result = result;
            clear_active_hold_for_note(session.note_index);
            break;
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
