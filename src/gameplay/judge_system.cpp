#include "judge_system.h"

#include <cmath>

void judge_system::init(const std::vector<note_data>& notes, const timing_engine& engine) {
    note_states_.clear();
    note_states_.reserve(notes.size());
    last_judge_.reset();
    judge_events_.clear();

    for (const note_data& note : notes) {
        note_state state;
        state.note_ref = note;
        state.target_ms = engine.tick_to_ms(note.tick);
        state.end_target_ms = engine.tick_to_ms(note.type == note_type::hold ? note.end_tick : note.tick);
        note_states_.push_back(state);
    }
}

void judge_system::update(double current_ms, const input_handler& input) {
    last_judge_.reset();
    judge_events_.clear();

    for (const input_event& event : input.events()) {
        if (event.type != input_event_type::release) {
            continue;
        }

        for (note_state& state : note_states_) {
            if (state.note_ref.type != note_type::hold || !state.holding || state.note_ref.lane != event.lane) {
                continue;
            }

            if (event.timestamp_ms >= state.end_target_ms) {
                state.holding = false;
                continue;
            }

            const double release_offset_ms = event.timestamp_ms - state.end_target_ms;
            state.holding = false;
            state.judged = true;
            state.result = evaluate_hold_release_offset(release_offset_ms);
            emit_judge(state.result, release_offset_ms, state.note_ref.lane);
        }
    }

    for (const input_event& event : input.events()) {
        if (event.type != input_event_type::press) {
            continue;
        }

        note_state* candidate = nullptr;
        for (note_state& state : note_states_) {
            if (state.judged || state.note_ref.lane != event.lane) {
                continue;
            }

            const double offset_ms = event.timestamp_ms - state.target_ms;
            if (offset_ms < -judge_windows_[3]) {
                continue;
            }

            if (!is_in_judgement_window(offset_ms)) {
                continue;
            }

            candidate = &state;
            break;
        }

        if (candidate == nullptr) {
            continue;
        }

        const double offset_ms = event.timestamp_ms - candidate->target_ms;
        const judge_result result = evaluate_offset(offset_ms);
        candidate->judged = true;
        candidate->result = result;
        candidate->holding = candidate->note_ref.type == note_type::hold && result != judge_result::miss;
        emit_judge(result, offset_ms, event.lane);
    }

    for (note_state& state : note_states_) {
        if (state.holding && current_ms >= state.end_target_ms) {
            state.holding = false;
        }

        if (state.judged) {
            continue;
        }

        const double offset_ms = current_ms - state.target_ms;
        if (offset_ms > judge_windows_[3]) {
            state.judged = true;
            state.result = judge_result::miss;
            emit_judge(judge_result::miss, offset_ms, state.note_ref.lane);
        }
    }
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

void judge_system::emit_judge(judge_result result, double offset_ms, int lane) {
    judge_event event{result, offset_ms, lane};
    judge_events_.push_back(event);
    last_judge_ = event;
}
