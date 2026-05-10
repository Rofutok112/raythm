#pragma once

#include <array>
#include <optional>
#include <vector>

#include "chart_judge_events.h"
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

    struct active_hold_session {
        bool active = false;
        size_t note_index = 0;
        std::array<bool, kMaxLanes> adopted_lanes = {};
    };

    judge_result evaluate_offset(double offset_ms) const;
    judge_result evaluate_hold_release_offset(double offset_ms) const;
    judge_result evaluate_release_offset(double offset_ms) const;
    judge_result evaluate_stay_offset(double offset_ms) const;
    bool is_in_judgement_window(double offset_ms) const;
    void complete_due_hold_before(int lane, double timestamp_ms);
    void process_input_event(const input_event& event);
    void handle_hold_release(const input_event& event);
    void handle_press(const input_event& event);
    void resolve_stay_notes(double current_ms, const input_handler& input);
    void resolve_hold_completions(double current_ms);
    void resolve_auto_misses(double current_ms);
    void advance_lane_head_index(int lane);
    void advance_lane_event_head_index(int lane);
    void advance_note_lane_head_indices(const note_data& note);
    void advance_event_lane_head_indices(const chart_judge_event& event);
    active_hold_session* active_hold_session_for_note(size_t note_index);
    const active_hold_session* active_hold_session_for_note(size_t note_index) const;
    bool activate_hold_lane(size_t note_index, int lane);
    void deactivate_hold_lane(size_t note_index, int lane);
    void clear_active_hold_for_note(size_t note_index);
    bool has_active_hold_for_note(size_t note_index) const;
    bool mark_event_completed(size_t event_descriptor_index);
    std::optional<size_t> descriptor_index_for_event_index(int event_index) const;
    bool release_overlaps_hold_tail(const chart_judge_event& release) const;
    bool try_absorb_completed_wide_press(const input_event& event);
    bool try_adopt_active_wide_hold_lane(const input_event& event);
    std::vector<size_t> find_press_candidates(int lane, double timestamp_ms);
    std::vector<size_t> find_release_candidates(int lane, double timestamp_ms);
    std::vector<size_t> find_early_release_stay_candidates(int lane, double timestamp_ms);
    void arm_release_candidate(int lane, double timestamp_ms);
    bool is_standalone_release_event(size_t event_descriptor_index) const;
    bool release_event_is_armed(size_t event_descriptor_index, int lane) const;
    void clear_armed_release_event(size_t event_descriptor_index);
    void complete_held_note(size_t note_index, bool emit_display_judge);
    void complete_event(size_t event_descriptor_index, judge_result result, double offset_ms);
    void complete_event(size_t event_descriptor_index, judge_result result, double offset_ms,
                        judge_emit_options options);
    void emit_judge(judge_result result, double offset_ms, int lane, int event_index);
    void emit_judge(judge_result result, double offset_ms, int lane,
                    int event_index, judge_emit_options options);

    std::vector<note_state> note_states_;
    std::vector<chart_judge_event> event_descriptors_;
    std::vector<size_t> event_descriptor_indices_by_event_index_;
    std::vector<bool> standalone_release_events_;
    std::vector<bool> event_completed_;
    std::vector<double> completed_wide_press_absorb_until_ms_;
    // Each lane keeps its own ordered note list and next unresolved head index.
    std::array<std::vector<size_t>, kMaxLanes> lane_note_indices_;
    std::array<std::vector<size_t>, kMaxLanes> lane_event_indices_;
    std::array<size_t, kMaxLanes> lane_head_indices_ = {};
    std::array<size_t, kMaxLanes> lane_event_head_indices_ = {};
    std::array<std::optional<size_t>, kMaxLanes> active_hold_indices_;
    std::vector<active_hold_session> active_hold_sessions_;
    std::array<std::optional<size_t>, kMaxLanes> armed_release_event_indices_;
    std::vector<judge_event> judge_events_;
};
