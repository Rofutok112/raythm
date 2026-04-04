#include "play_flow_controller.h"

#include <algorithm>

namespace {

play_navigation_request navigate_after_transition(const play_session_state& state) {
    if (state.editor_resume_state.has_value() && state.song_data.has_value()) {
        return {play_navigation_target::editor};
    }
    return {play_navigation_target::result};
}

bool is_no_fail_playtest(const play_session_state& state) {
    return state.editor_resume_state.has_value();
}

}  // namespace

play_update_result play_flow_controller::update(play_session_state& state, play_note_draw_queue& draw_queue,
                                                const play_update_context& context) {
    play_update_result result;
    state.judge_feedback_timer = std::max(0.0f, state.judge_feedback_timer - context.dt);

    if (!state.initialized) {
        if (context.escape_pressed) {
            result.navigation = state.editor_resume_state.has_value() && state.song_data.has_value()
                                    ? play_navigation_request{play_navigation_target::editor}
                                    : play_navigation_request{play_navigation_target::song_select};
        }
        return result;
    }

    if (!context.window_focused && !state.paused) {
        state.paused = true;
        state.auto_paused_by_focus = true;
        state.ranking_enabled = false;
        state.paused_ms = state.current_ms;
        result.request_pause_bgm = true;
    }

    if (state.editor_resume_state.has_value() && context.escape_pressed) {
        result.navigation = {play_navigation_target::editor};
        return result;
    }

    if (context.escape_pressed) {
        state.paused = !state.paused;
        state.paused_ms = state.current_ms;
        if (state.paused) {
            state.ranking_enabled = false;
            result.request_pause_bgm = true;
        } else if (context.bgm_loaded && !state.intro_playing) {
            state.auto_paused_by_focus = false;
            result.request_play_bgm = true;
        }
    }

    if (state.paused) {
        if (context.pause_resume_clicked) {
            state.paused = false;
            state.auto_paused_by_focus = false;
            if (context.bgm_loaded && !state.intro_playing) {
                result.request_play_bgm = true;
            }
            return result;
        }

        if (context.pause_restart_clicked) {
            result.navigation = {play_navigation_target::restart};
            return result;
        }

        if (context.pause_song_select_clicked) {
            result.navigation = {play_navigation_target::song_select};
            return result;
        }

        return result;
    }

    if (state.failure_transition_playing) {
        state.failure_transition_timer = std::max(0.0f, state.failure_transition_timer - context.dt);
        if (state.failure_transition_timer <= 0.0f) {
            result.navigation = navigate_after_transition(state);
        }
        return result;
    }

    if (state.result_transition_playing) {
        state.result_transition_timer = std::min(play_session_constants::kResultTransitionDurationSeconds,
                                                 state.result_transition_timer + context.dt);
        if (state.result_transition_timer >= play_session_constants::kResultTransitionDurationSeconds) {
            result.navigation = navigate_after_transition(state);
        }
        return result;
    }

    if (state.intro_playing) {
        state.intro_timer = std::max(0.0f, state.intro_timer - context.dt);
        state.input_handler.update(state.current_ms);
        if (context.draw_window.has_value()) {
            draw_queue.update_visible_window(state.judge_system.note_states(), static_cast<float>(state.lane_speed),
                                             context.draw_window->judgement_z, context.draw_window->lane_start_z,
                                             context.draw_window->lane_end_z, context.draw_window->visual_ms);
        }

        if (state.intro_timer <= 0.0f) {
            state.intro_playing = false;
            if (context.bgm_loaded) {
                result.request_play_bgm = true;
            }
        }
        return result;
    }

    state.current_ms = context.bgm_audio_time_ms.has_value()
                           ? *context.bgm_audio_time_ms
                           : state.current_ms + static_cast<double>(context.dt) * 1000.0;
    state.input_handler.update(state.current_ms);
    state.judge_system.update(state.current_ms, state.input_handler);
    const std::vector<judge_event>& judge_events = state.judge_system.get_judge_events();
    state.last_judge = state.judge_system.get_last_judge();
    for (const judge_event& event : judge_events) {
        if (event.apply_gameplay_effects) {
            state.score_system.on_judge(event);
            state.gauge.on_judge(event.result);
        }
        if (!state.hitsound_path.empty() && event.play_hitsound && event.result != judge_result::miss) {
            ++result.hitsound_count;
        }
        state.display_judge = event;
        state.judge_feedback_timer = 1.0f;
    }
    state.combo_display = state.score_system.get_combo();

    if (!is_no_fail_playtest(state) && state.gauge.get_value() <= 0.0f) {
        state.final_result = state.score_system.get_result_data();
        state.final_result.failed = true;
        state.ranking_enabled = false;
        state.failure_transition_playing = true;
        state.failure_transition_timer = play_session_constants::kFailureTransitionDurationSeconds;
        result.request_pause_bgm = true;
        return result;
    }

    if (context.draw_window.has_value()) {
        draw_queue.update_visible_window(state.judge_system.note_states(), static_cast<float>(state.lane_speed),
                                         context.draw_window->judgement_z, context.draw_window->lane_start_z,
                                         context.draw_window->lane_end_z, context.draw_window->visual_ms);
    }

    const std::vector<note_state>& note_states = state.judge_system.note_states();
    const bool chart_finished = !note_states.empty() &&
        std::all_of(note_states.begin(), note_states.end(), [](const note_state& note_state) {
            return note_state.judged;
        });

    if (chart_finished && (context.enter_pressed || context.left_click_pressed)) {
        state.final_result = state.score_system.get_result_data();
        state.result_transition_playing = true;
        state.result_transition_timer = 0.0f;
        result.request_pause_bgm = true;
        return result;
    }

    if (state.current_ms >= state.song_end_ms) {
        state.final_result = state.score_system.get_result_data();
        state.result_transition_playing = true;
        state.result_transition_timer = 0.0f;
    } else if (context.backspace_pressed) {
        result.navigation = {play_navigation_target::song_select};
    }

    return result;
}
