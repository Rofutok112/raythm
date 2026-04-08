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
    static constexpr double kPerfectWindowMs = 40.0;
    static constexpr double kGreatWindowMs = 80.0;
    static constexpr double kGoodWindowMs = 120.0;
    static constexpr double kBadWindowMs = 160.0;

    void init(const std::vector<note_data>& notes, const timing_engine& engine);
    void update(double current_ms, const input_handler& input);
    std::optional<judge_event> get_last_judge() const;
    const std::vector<judge_event>& get_judge_events() const;
    const std::vector<note_state>& note_states() const;

private:
    struct judge_emit_options {
        bool play_hitsound = true;
        bool apply_gameplay_effects = true;
        bool show_feedback = true;
    };

    judge_result evaluate_offset(double offset_ms) const;
    judge_result evaluate_hold_release_offset(double offset_ms) const;
    bool is_in_judgement_window(double offset_ms) const;
    void complete_due_hold_before(int lane, double timestamp_ms);
    void handle_hold_release(const input_event& event);
    void handle_press(const input_event& event);
    void resolve_hold_completions(double current_ms);
    void resolve_auto_misses(double current_ms);
    void advance_lane_head_index(int lane);
    std::optional<size_t> find_press_candidate(int lane, double timestamp_ms);
    void complete_held_note(size_t note_index, bool emit_display_judge);
    void emit_judge(judge_result result, double offset_ms, int lane);
    void emit_judge(judge_result result, double offset_ms, int lane,
                    judge_emit_options options);

    std::vector<note_state> note_states_;
    // Each lane keeps its own ordered note list and next unresolved head index.
    std::array<std::vector<size_t>, kMaxLanes> lane_note_indices_;
    std::array<size_t, kMaxLanes> lane_head_indices_ = {};
    std::array<std::optional<size_t>, kMaxLanes> active_hold_indices_;
    std::vector<judge_event> judge_events_;
};
