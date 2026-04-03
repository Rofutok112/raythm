#pragma once

#include <optional>
#include <string>

#include "editor/editor_scene_types.h"
#include "input_handler.h"
#include "judge_system.h"
#include "score_system.h"
#include "song_loader.h"
#include "timing_engine.h"

namespace play_session_constants {

inline constexpr float kIntroDurationSeconds = 2.0f;
inline constexpr float kFailureFadeDurationSeconds = 1.0f;
inline constexpr float kFailureHoldDurationSeconds = 1.0f;
inline constexpr float kFailureTransitionDurationSeconds =
    kFailureFadeDurationSeconds + kFailureHoldDurationSeconds;
inline constexpr float kResultTransitionDurationSeconds = 1.0f;

}  // namespace play_session_constants

enum class play_navigation_target {
    none,
    song_select,
    result,
    editor,
    restart,
};

struct play_navigation_request {
    play_navigation_target target = play_navigation_target::none;

    [[nodiscard]] bool has_value() const {
        return target != play_navigation_target::none;
    }
};

struct play_start_request {
    int key_count = 4;
    std::optional<song_data> song_data;
    std::optional<std::string> selected_chart_path;
    std::optional<chart_data> chart_data;
    std::optional<editor_resume_state> editor_resume_state;
    int start_tick = 0;
};

struct play_draw_window {
    float lane_start_z = 0.0f;
    float judgement_z = 0.0f;
    float lane_end_z = 0.0f;
    double visual_ms = 0.0;
};

struct play_session_state {
    int key_count = 4;
    bool initialized = false;
    bool paused = false;
    bool ranking_enabled = true;
    bool auto_paused_by_focus = false;
    float camera_angle_degrees = 45.0f;
    double current_ms = 0.0;
    double paused_ms = 0.0;
    double song_end_ms = 0.0;
    double lane_speed = 0.045;
    int combo_display = 0;
    score_system score_system;
    gauge gauge;
    input_handler input_handler;
    timing_engine timing_engine;
    judge_system judge_system;
    std::optional<chart_data> chart_data;
    std::optional<song_data> song_data;
    std::optional<std::string> selected_chart_path;
    std::optional<editor_resume_state> editor_resume_state;
    std::optional<judge_event> last_judge;
    std::optional<judge_event> display_judge;
    result_data final_result;
    std::string status_text;
    float judge_feedback_timer = 0.0f;
    bool intro_playing = true;
    float intro_timer = play_session_constants::kIntroDurationSeconds;
    bool failure_transition_playing = false;
    float failure_transition_timer = 0.0f;
    bool result_transition_playing = false;
    float result_transition_timer = 0.0f;
    std::string hitsound_path;
    int start_tick = 0;
    double start_ms = 0.0;
};
