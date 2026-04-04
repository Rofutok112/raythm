#include "judge_system.h"

#include <cmath>

void judge_system::init(const std::vector<note_data>& notes, const timing_engine& engine) {
    note_states_.clear();
    note_states_.reserve(notes.size());
    for (std::vector<size_t>& lane_indices : lane_note_indices_) {
        lane_indices.clear();
    }
    lane_head_indices_.fill(0);
    active_hold_indices_.fill(std::nullopt);
    last_judge_.reset();
    judge_events_.clear();

    for (size_t i = 0; i < notes.size(); ++i) {
        const note_data& note = notes[i];
        note_state state;
        state.note_ref = note;
        state.target_ms = engine.tick_to_ms(note.tick);
        state.end_target_ms = engine.tick_to_ms(note.type == note_type::hold ? note.end_tick : note.tick);
        note_states_.push_back(state);
        if (note.lane >= 0 && note.lane < kMaxLanes) {
            lane_note_indices_[static_cast<size_t>(note.lane)].push_back(i);
        }
    }
}

void judge_system::update(double current_ms, const input_handler& input) {
    last_judge_.reset();
    judge_events_.clear();

    for (const input_event& event : input.events()) {
        if (event.type == input_event_type::release) {
            handle_hold_release(event);
        }
    }

    for (const input_event& event : input.events()) {
        if (event.type == input_event_type::press) {
            handle_press(event);
        }
    }

    resolve_hold_completions(current_ms);
    resolve_auto_misses(current_ms);
}

std::optional<judge_event> judge_system::get_last_judge() const {
    return last_judge_;
}

const std::vector<judge_event>& judge_system::get_judge_events() const {
    return judge_events_;
}

std::vector<note_state> judge_system::get_note_states() const {
    return note_states_;
}

const std::vector<note_state>& judge_system::note_states() const {
    return note_states_;
}

judge_result judge_system::evaluate_offset(double offset_ms) const {
    const double absolute_offset = std::fabs(offset_ms);
    if (absolute_offset <= judge_windows_[0]) {
        return judge_result::perfect;
    }
    if (absolute_offset <= judge_windows_[1]) {
        return judge_result::great;
    }
    if (absolute_offset <= judge_windows_[2]) {
        return judge_result::good;
    }
    if (absolute_offset <= judge_windows_[3]) {
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

bool judge_system::is_in_judgement_window(double offset_ms) const {
    return std::fabs(offset_ms) <= judge_windows_[3];
}

void judge_system::handle_hold_release(const input_event& event) {
    if (event.lane < 0 || event.lane >= kMaxLanes) {
        return;
    }

    std::optional<size_t>& active_hold = active_hold_indices_[static_cast<size_t>(event.lane)];
    if (!active_hold.has_value()) {
        return;
    }

    note_state& state = note_states_[*active_hold];
    if (!state.holding) {
        active_hold.reset();
        return;
    }

    if (event.timestamp_ms >= state.end_target_ms) {
        complete_held_note(*active_hold, false);
        return;
    }

    const double release_offset_ms = event.timestamp_ms - state.end_target_ms;
    state.holding = false;
    state.completed = true;
    state.result = evaluate_hold_release_offset(release_offset_ms);
    active_hold.reset();
    emit_judge(state.result, release_offset_ms, state.note_ref.lane, false);
}

void judge_system::handle_press(const input_event& event) {
    if (event.lane < 0 || event.lane >= kMaxLanes) {
        return;
    }

    const std::optional<size_t> candidate_index = find_press_candidate(event.lane, event.timestamp_ms);
    if (!candidate_index.has_value()) {
        return;
    }

    note_state& candidate = note_states_[*candidate_index];
    const double offset_ms = event.timestamp_ms - candidate.target_ms;
    const judge_result result = evaluate_offset(offset_ms);
    candidate.judged = true;
    candidate.completed = candidate.note_ref.type != note_type::hold || result == judge_result::miss;
    candidate.result = result;
    candidate.holding = candidate.note_ref.type == note_type::hold && result != judge_result::miss;
    advance_lane_head_index(event.lane);
    if (candidate.holding) {
        active_hold_indices_[static_cast<size_t>(event.lane)] = *candidate_index;
    }
    emit_judge(result, offset_ms, event.lane);
}

void judge_system::resolve_hold_completions(double current_ms) {
    for (int lane = 0; lane < kMaxLanes; ++lane) {
        const std::optional<size_t> active_hold = active_hold_indices_[static_cast<size_t>(lane)];
        if (!active_hold.has_value()) {
            continue;
        }

        if (note_states_[*active_hold].holding && current_ms >= note_states_[*active_hold].end_target_ms) {
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
            if (state.judged) {
                ++head_index;
                continue;
            }

            const double offset_ms = current_ms - state.target_ms;
            if (offset_ms <= judge_windows_[3]) {
                break;
            }

            state.judged = true;
            state.completed = true;
            state.result = judge_result::miss;
            emit_judge(judge_result::miss, offset_ms, state.note_ref.lane);
            ++head_index;
        }
    }
}

void judge_system::advance_lane_head_index(int lane) {
    if (lane < 0 || lane >= kMaxLanes) {
        return;
    }

    const std::vector<size_t>& lane_indices = lane_note_indices_[static_cast<size_t>(lane)];
    size_t& head_index = lane_head_indices_[static_cast<size_t>(lane)];
    while (head_index < lane_indices.size() && note_states_[lane_indices[head_index]].judged) {
        ++head_index;
    }
}

std::optional<size_t> judge_system::find_press_candidate(int lane, double timestamp_ms) {
    if (lane < 0 || lane >= kMaxLanes) {
        return std::nullopt;
    }

    advance_lane_head_index(lane);
    const std::vector<size_t>& lane_indices = lane_note_indices_[static_cast<size_t>(lane)];
    for (size_t i = lane_head_indices_[static_cast<size_t>(lane)]; i < lane_indices.size(); ++i) {
        note_state& state = note_states_[lane_indices[i]];
        if (state.judged) {
            continue;
        }

        const double offset_ms = timestamp_ms - state.target_ms;
        if (offset_ms < -judge_windows_[3]) {
            break;
        }
        if (!is_in_judgement_window(offset_ms)) {
            continue;
        }

        return lane_indices[i];
    }
    return std::nullopt;
}

void judge_system::complete_held_note(size_t note_index, bool emit_display_judge) {
    note_state& state = note_states_[note_index];
    active_hold_indices_[static_cast<size_t>(state.note_ref.lane)].reset();
    state.result = judge_result::perfect;
    state.holding = false;
    state.completed = true;
    advance_lane_head_index(state.note_ref.lane);
    if (emit_display_judge) {
        emit_judge(judge_result::perfect, 0.0, state.note_ref.lane, false, false);
    }
}

void judge_system::emit_judge(judge_result result, double offset_ms, int lane,
                              bool play_hitsound, bool apply_gameplay_effects) {
    judge_event event{result, offset_ms, lane, play_hitsound, apply_gameplay_effects};
    judge_events_.push_back(event);
    last_judge_ = event;
}
