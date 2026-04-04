#pragma once

#include <array>
#include <optional>
#include <vector>

#include "data_models.h"
#include "input_handler.h"
#include "timing_engine.h"

class judge_system {
public:
    static constexpr int kMaxLanes = 6;

    void init(const std::vector<note_data>& notes, const timing_engine& engine);
    void update(double current_ms, const input_handler& input);
    std::optional<judge_event> get_last_judge() const;
    const std::vector<judge_event>& get_judge_events() const;
    std::vector<note_state> get_note_states() const;
    const std::vector<note_state>& note_states() const;

private:
    judge_result evaluate_offset(double offset_ms) const;
    judge_result evaluate_hold_release_offset(double offset_ms) const;
    bool is_in_judgement_window(double offset_ms) const;
    void handle_hold_release(const input_event& event);
    void handle_press(const input_event& event);
    void resolve_hold_completions(double current_ms);
    void resolve_auto_misses(double current_ms);
    void advance_lane_head_index(int lane);
    std::optional<size_t> find_press_candidate(int lane, double timestamp_ms);
    void complete_held_note(size_t note_index, bool emit_display_judge);
    void emit_judge(judge_result result, double offset_ms, int lane,
                    bool play_hitsound = true, bool apply_gameplay_effects = true);

    std::array<double, 5> judge_windows_ = {25.0, 50.0, 90.0, 130.0, 130.0};
    std::vector<note_state> note_states_;
    // Each lane keeps its own ordered note list and next unresolved head index.
    std::array<std::vector<size_t>, kMaxLanes> lane_note_indices_;
    std::array<size_t, kMaxLanes> lane_head_indices_ = {};
    std::array<std::optional<size_t>, kMaxLanes> active_hold_indices_;
    std::optional<judge_event> last_judge_;
    std::vector<judge_event> judge_events_;
};
