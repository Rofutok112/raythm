#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "editor/editor_scene_types.h"
#include "input_handler.h"
#include "judge_system.h"
#include "performance_system.h"
#include "score_system.h"
#include "song_loader.h"
#include "play_scroll_map.h"
#include "timing_engine.h"

namespace play_session_constants {

inline constexpr float kIntroDurationSeconds = 0.0f;
inline constexpr double kAudioLeadInBeforeFirstSoundMs = 3000.0;
inline constexpr float kFailureFadeDurationSeconds = 1.0f;
inline constexpr float kFailureHoldDurationSeconds = 1.0f;
inline constexpr float kFailureTransitionDurationSeconds =
    kFailureFadeDurationSeconds + kFailureHoldDurationSeconds;
inline constexpr float kResultTransitionDurationSeconds = 1.0f;
inline constexpr float kLaneJudgeEffectDurationSeconds = 0.28f;
inline constexpr double kChartEndTailMs = 2000.0;
inline constexpr unsigned int kChartEndFadeOutMs = 2000;
inline constexpr unsigned int kResultSkipFadeOutMs = 300;

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
    std::optional<float> selected_chart_level;
    std::optional<chart_data> chart_data;
    std::optional<editor_resume_state> editor_resume_state;
    int start_tick = 0;
    std::string multiplayer_room_id;
    std::string multiplayer_match_id;
};

struct play_multiplayer_score_row {
    std::string user_id;
    std::string display_name;
    int score = 0;
    int combo = 0;
    float accuracy = 0.0f;
    bool failed = false;
    bool has_result_details = false;
    std::array<int, 5> judge_counts = {};
    float rc_value = 0.0f;
    float avg_offset = 0.0f;
    int fast_count = 0;
    int slow_count = 0;
    rank clear_rank = rank::f;
    bool is_full_combo = false;
    bool is_all_perfect = false;
};

struct play_draw_window {
    float lane_start_z = 0.0f;
    float judgement_z = 0.0f;
    float lane_end_z = 0.0f;
    double visual_time_ms = 0.0;
};

struct lane_judge_effect {
    judge_result result = judge_result::miss;
    float timer = 0.0f;
    int lane_width = 1;
};

struct play_hitsound_paths {
    std::string tap;
    std::string ray_tap;
    std::string release;
    std::string ray_release;
    std::string stay;
    std::string ray_stay;

    [[nodiscard]] bool has_any() const {
        return !tap.empty() || !ray_tap.empty() || !release.empty() ||
               !ray_release.empty() || !stay.empty() || !ray_stay.empty();
    }

    [[nodiscard]] const std::string& path_for(const judge_event& event) const {
        if (event.hitsound_type == note_type::release) {
            if (event.is_ray && !ray_release.empty()) {
                return ray_release;
            }
            return release.empty() ? tap : release;
        }
        if (event.hitsound_type == note_type::stay) {
            if (event.is_ray && !ray_stay.empty()) {
                return ray_stay;
            }
            return stay.empty() ? tap : stay;
        }
        if (event.is_ray && !ray_tap.empty()) {
            return ray_tap;
        }
        return tap;
    }
};

struct play_session_state {
    int key_count = 4;
    bool initialized = false;
    bool paused = false;
    bool ranking_enabled = true;
    bool auto_paused_by_focus = false;
    float camera_angle_degrees = 45.0f;
    double chart_time_ms = 0.0;
    double paused_chart_time_ms = 0.0;
    double song_end_chart_time_ms = 0.0;
    double lane_speed = 0.045;
    int combo_display = 0;
    score_system score_system;
    performance_system performance_system;
    gauge gauge;
    input_handler input_handler;
    timing_engine timing_engine;
    play_scroll_map scroll_map;
    judge_system judge_system;
    std::optional<chart_data> chart_data;
    std::optional<song_data> song_data;
    std::optional<std::string> selected_chart_path;
    std::optional<editor_resume_state> editor_resume_state;
    std::optional<judge_event> last_judge;
    std::optional<judge_event> display_judge;
    std::array<float, judge_system::kMaxLanes> lane_hold_dim_amounts = {};
    std::array<lane_judge_effect, judge_system::kMaxLanes> lane_judge_effects = {};
    result_data final_result;
    std::string status_text;
    float judge_feedback_timer = 0.0f;
    bool intro_playing = true;
    float intro_timer = play_session_constants::kIntroDurationSeconds;
    bool failure_transition_playing = false;
    float failure_transition_timer = 0.0f;
    bool result_transition_playing = false;
    float result_transition_timer = 0.0f;
    bool chart_end_fade_started = false;
    float chart_end_hold_timer = 0.0f;
    std::string hitsound_path;
    play_hitsound_paths hitsounds;
    std::vector<float> mv_waveform;
    int start_tick = 0;
    double start_ms = 0.0;
    std::string multiplayer_room_id;
    std::string multiplayer_match_id;
    bool multiplayer_failed = false;
    std::vector<play_multiplayer_score_row> multiplayer_scores;
};
