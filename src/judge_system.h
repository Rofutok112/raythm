#pragma once

#include <array>
#include <optional>
#include <vector>

#include "data_models.h"
#include "input_handler.h"
#include "timing_engine.h"

class judge_system {
public:
    void init(const std::vector<note_data>& notes, const timing_engine& engine);
    void update(double current_ms, const input_handler& input);
    std::optional<judge_event> get_last_judge() const;
    std::vector<note_state> get_note_states() const;

private:
    judge_result evaluate_offset(double offset_ms) const;
    bool is_in_judgement_window(double offset_ms) const;
    void emit_judge(judge_result result, double offset_ms, int lane);

    std::array<double, 5> judge_windows_ = {25.0, 50.0, 90.0, 130.0, 130.0};
    std::vector<note_state> note_states_;
    std::optional<judge_event> last_judge_;
};
