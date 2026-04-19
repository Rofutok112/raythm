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
    judge_events_.clear();

    int next_event_index = 0;
    for (size_t i = 0; i < notes.size(); ++i) {
        const note_data& note = notes[i];
        note_state state;
        state.note_ref = note;
        state.target_ms = engine.tick_to_ms(note.tick);
        state.end_target_ms = engine.tick_to_ms(note.type == note_type::hold ? note.end_tick : note.tick);
        state.head_event_index = next_event_index++;
        state.tail_event_index = note.type == note_type::hold ? next_event_index++ : state.head_event_index;
        note_states_.push_back(state);
        if (note.lane >= 0 && note.lane < kMaxLanes) {
            lane_note_indices_[static_cast<size_t>(note.lane)].push_back(i);
        }
    }
}

void judge_system::update(double current_ms, const input_handler& input) {
    judge_events_.clear();

    // release を先に処理して、終了済み hold が次の press より前にレーンを空けられるようにする。
    for (const input_event& event : input.events()) {
        if (event.type == input_event_type::release) {
            handle_hold_release(event);
        }
    }

    // press はそのあとで、各レーンの次に判定可能なノートへ割り当てる。
    for (const input_event& event : input.events()) {
        if (event.type == input_event_type::press) {
            handle_press(event);
        }
    }

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

bool judge_system::is_in_judgement_window(double offset_ms) const {
    return std::fabs(offset_ms) <= kBadWindowMs;
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

    std::optional<size_t>& active_hold = active_hold_indices_[static_cast<size_t>(event.lane)];
    if (!active_hold.has_value()) {
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
    active_hold.reset();
    judge_emit_options options;
    options.play_hitsound = false;
    emit_judge(state.result, release_offset_ms, state.note_ref.lane, state.tail_event_index, options);
}

void judge_system::handle_press(const input_event& event) {
    if (event.lane < 0 || event.lane >= kMaxLanes) {
        return;
    }

    complete_due_hold_before(event.lane, event.timestamp_ms);

    const std::optional<size_t> candidate_index = find_press_candidate(event.lane, event.timestamp_ms);
    if (!candidate_index.has_value()) {
        return;
    }

    note_state& candidate = note_states_[*candidate_index];
    const double offset_ms = event.timestamp_ms - candidate.target_ms;
    const judge_result result = evaluate_offset(offset_ms);
    // hold の head 判定後は、tail 完走か早離しで確定するまで進行中のままにする。
    candidate.progress =
        (candidate.note_ref.type == note_type::hold && result != judge_result::miss)
            ? note_progress_state::holding
            : note_progress_state::completed;
    candidate.result = result;
    advance_lane_head_index(event.lane);
    if (candidate.is_holding()) {
        active_hold_indices_[static_cast<size_t>(event.lane)] = *candidate_index;
    }
    emit_judge(result, offset_ms, event.lane, candidate.head_event_index);
}

void judge_system::resolve_hold_completions(double current_ms) {
    for (int lane = 0; lane < kMaxLanes; ++lane) {
        const std::optional<size_t> active_hold = active_hold_indices_[static_cast<size_t>(lane)];
        if (!active_hold.has_value()) {
            continue;
        }

        // hold 中のノートが tail まで到達したら完了。tail 側も独立した判定として加点する。
        if (note_states_[*active_hold].is_holding() && current_ms >= note_states_[*active_hold].end_target_ms) {
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
            emit_judge(judge_result::miss, offset_ms, state.note_ref.lane, state.head_event_index);
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
    while (head_index < lane_indices.size() && note_states_[lane_indices[head_index]].is_judged()) {
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
        if (state.is_judged()) {
            continue;
        }

        const double offset_ms = timestamp_ms - state.target_ms;
        if (offset_ms < -kBadWindowMs) {
            // 同レーン内は時間順なので、ここで早すぎるなら後続ノートもまだ早い。
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
    std::optional<size_t>& active_hold = active_hold_indices_[static_cast<size_t>(state.note_ref.lane)];
    if (active_hold.has_value() && *active_hold == note_index) {
        active_hold.reset();
    }
    state.result = judge_result::perfect;
    state.progress = note_progress_state::completed;
    advance_lane_head_index(state.note_ref.lane);
    judge_emit_options options;
    options.play_hitsound = false;
    options.apply_gameplay_effects = true;
    options.show_feedback = emit_display_judge;
    emit_judge(judge_result::perfect, 0.0, state.note_ref.lane, state.tail_event_index, options);
}

void judge_system::emit_judge(judge_result result, double offset_ms, int lane, int event_index) {
    emit_judge(result, offset_ms, lane, event_index, judge_emit_options{});
}

void judge_system::emit_judge(judge_result result, double offset_ms, int lane,
                              int event_index,
                              judge_emit_options options) {
    judge_event event{result, offset_ms, lane,
                      options.play_hitsound,
                      options.apply_gameplay_effects,
                      options.show_feedback,
                      event_index};
    judge_events_.push_back(event);
}
