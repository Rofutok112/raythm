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
    void update_auto(double current_ms, double hitsound_lead_ms = 0.0);
    std::optional<judge_event> get_last_judge() const;
    const std::vector<judge_event>& get_judge_events() const;
    const std::vector<note_state>& note_states() const;

private:
    struct judge_emit_options {
        bool play_hitsound = true;
        bool apply_gameplay_effects = true;
        bool show_feedback = true;
    };

    using input_session_id = size_t;

    struct input_session {
        input_session_id id = 0;
        int lane = 0;
        double press_ms = 0.0;
        double release_ms = 0.0;
        bool held = true;
        std::optional<size_t> armed_release_event_index;
        std::optional<size_t> armed_stay_event_index;
        std::optional<size_t> active_hold_note_index;
    };

    struct active_hold_session {
        bool active = false;
        size_t note_index = 0;
        std::array<std::optional<input_session_id>, kMaxLanes> adopted_sessions = {};
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
    void resolve_hold_heads_from_nearby_stays(double current_ms, const input_handler& input);
    void resolve_stay_notes(double current_ms, const input_handler& input);
    void resolve_hold_completions(double current_ms);
    void resolve_auto_misses(double current_ms);
    input_session* session_for_id(input_session_id session_id);
    const input_session* session_for_id(input_session_id session_id) const;
    input_session_id begin_input_session(const input_event& event);
    input_session* release_input_session(const input_event& event);
    active_hold_session* active_hold_session_for_note(size_t note_index);
    const active_hold_session* active_hold_session_for_note(size_t note_index) const;
    bool activate_hold_lane(size_t note_index, int lane, input_session_id input_id);
    void deactivate_hold_lane(size_t note_index, int lane, input_session_id input_id);
    void clear_active_hold_for_note(size_t note_index);
    bool has_active_hold_for_note(size_t note_index) const;
    bool lane_is_held_for_stay(const input_handler& input, int lane, double timestamp_ms) const;
    bool lane_was_held_at(int lane, double timestamp_ms) const;
    bool has_held_nearby_stay(const input_handler& input, const chart_judge_event& hold_head) const;
    bool mark_event_completed(size_t event_descriptor_index);
    std::optional<size_t> descriptor_index_for_event_index(int event_index) const;
    bool release_overlaps_hold_tail(const chart_judge_event& release) const;
    bool try_absorb_completed_wide_press(const input_event& event, input_session_id input_id);
    bool try_adopt_active_wide_hold_lane(const input_event& event, input_session_id input_id);
    std::vector<size_t> find_press_candidates(int lane, double timestamp_ms);
    std::vector<size_t> find_release_candidates(int lane, double timestamp_ms,
                                                std::optional<input_session_id> input_id);
    void complete_stays_stacked_with_hold_tail_release(const chart_judge_event& hold_tail,
                                                       double timestamp_ms,
                                                       judge_result hold_result);
    bool arm_release_candidate(input_session_id input_id, double timestamp_ms);
    std::optional<double> best_armable_release_abs_offset(int lane, double timestamp_ms) const;
    void arm_stay_candidate(input_session_id input_id, double timestamp_ms);
    bool complete_armed_stay_candidate(input_session* input, const input_event& event);
    bool is_standalone_release_event(size_t event_descriptor_index) const;
    bool release_event_is_armed(size_t event_descriptor_index, std::optional<input_session_id> input_id) const;
    void clear_armed_release_event(size_t event_descriptor_index);
    void clear_armed_stay_event(size_t event_descriptor_index);
    void complete_held_note(size_t note_index, bool emit_display_judge);
    void complete_auto_event(size_t event_descriptor_index);
    void complete_event(size_t event_descriptor_index, judge_result result, double offset_ms);
    void complete_event(size_t event_descriptor_index, judge_result result, double offset_ms,
                        judge_emit_options options);
    void emit_judge(judge_result result, double offset_ms, int lane, int event_index);
    void emit_judge(judge_result result, double offset_ms, int lane,
                    int event_index, judge_emit_options options);
    void emit_auto_judge(int event_index, judge_emit_options options);

    std::vector<note_state> note_states_;
    std::vector<chart_judge_event> event_descriptors_;
    std::vector<size_t> event_descriptor_indices_by_event_index_;
    std::vector<bool> standalone_release_events_;
    std::vector<bool> event_completed_;
    std::vector<bool> auto_hitsound_emitted_;
    std::vector<double> completed_wide_press_absorb_until_ms_;
    std::array<std::optional<input_session_id>, kMaxLanes> held_input_session_by_lane_;
    std::vector<input_session> input_sessions_;
    std::vector<active_hold_session> active_hold_sessions_;
    std::vector<judge_event> judge_events_;
};
