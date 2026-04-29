#include <cstdlib>
#include <iostream>
#include <vector>

#include "play/play_flow_controller.h"

namespace {

play_session_state make_initialized_state() {
    play_session_state state;
    state.initialized = true;
    state.key_count = 4;
    state.input_handler.set_key_count(4);
    state.song_end_ms = 5000.0;
    state.current_ms = 1000.0;
    state.start_ms = 0.0;
    state.intro_playing = false;
    return state;
}

timing_engine make_basic_timing_engine() {
    timing_engine engine;
    engine.init({timing_event{timing_event_type::bpm, 0, 120.0f, 4, 4}}, 480);
    return engine;
}

}  // namespace

int main() {
    {
        play_session_state state = make_initialized_state();
        play_note_draw_queue draw_queue;
        play_update_context context;
        context.dt = 0.016f;
        context.window_focused = false;

        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (!state.paused || !state.auto_paused_by_focus || state.ranking_enabled ||
            state.paused_ms != 1000.0 || !result.request_pause_bgm) {
            std::cerr << "Auto pause flow failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        play_session_state state = make_initialized_state();
        state.intro_playing = true;
        state.intro_timer = 0.01f;
        play_note_draw_queue draw_queue;
        play_update_context context;
        context.dt = 0.02f;
        context.bgm_loaded = true;
        context.draw_window = play_draw_window{0.0f, 10.0f, 100.0f, 0.0};

        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (state.intro_playing || !result.request_play_bgm) {
            std::cerr << "Intro completion flow failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        play_session_state state = make_initialized_state();
        state.score_system.init(1);
        for (int i = 0; i < 10; ++i) {
            state.gauge.on_judge(judge_result::miss);
        }

        play_note_draw_queue draw_queue;
        play_update_context context;
        context.dt = 0.0f;
        context.bgm_loaded = true;
        context.bgm_audio_time_ms = 1200.0;

        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (!state.failure_transition_playing || !state.final_result.failed || state.ranking_enabled ||
            !result.request_pause_bgm) {
            std::cerr << "Failure transition flow failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        play_session_state state = make_initialized_state();
        state.score_system.init(1);
        state.song_data = song_data{};
        state.editor_resume_state = editor_resume_state{};
        for (int i = 0; i < 10; ++i) {
            state.gauge.on_judge(judge_result::miss);
        }

        play_note_draw_queue draw_queue;
        play_update_context context;
        context.dt = 0.0f;
        context.bgm_loaded = true;
        context.bgm_audio_time_ms = 1200.0;

        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (state.failure_transition_playing || state.final_result.failed || result.request_pause_bgm) {
            std::cerr << "Editor playtest should not enter failure transition\n";
            return EXIT_FAILURE;
        }
    }

    {
        play_session_state state = make_initialized_state();
        state.score_system.init(1);
        state.timing_engine = make_basic_timing_engine();
        state.judge_system.init({note_data{note_type::tap, 480, 0, 480}}, state.timing_engine);

        play_note_draw_queue draw_queue;
        draw_queue.init_from_note_states(4, state.judge_system.note_states());

        play_update_context context;
        context.dt = 0.0f;
        context.enter_pressed = true;
        context.bgm_audio_time_ms = 700.0;

        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (!state.result_transition_playing || !result.request_fade_out_bgm || state.final_result.judge_counts[4] != 1) {
            std::cerr << "Result skip flow failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        play_session_state state = make_initialized_state();
        state.song_data = song_data{};
        state.editor_resume_state = editor_resume_state{};
        play_note_draw_queue draw_queue;
        play_update_context context;
        context.escape_pressed = true;

        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (result.navigation.target != play_navigation_target::editor || state.paused) {
            std::cerr << "Editor escape flow failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        play_session_state state = make_initialized_state();
        play_note_draw_queue draw_queue;
        play_update_context context;
        context.escape_pressed = true;

        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (!state.paused || state.ranking_enabled || !result.request_pause_bgm) {
            std::cerr << "Normal escape pause flow failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        play_session_state state = make_initialized_state();
        state.score_system.init(1);
        state.timing_engine = make_basic_timing_engine();
        state.judge_system.init({note_data{note_type::hold, 480, 0, 960}}, state.timing_engine);
        state.hitsound_path = "hitsound.mp3";

        state.input_handler.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 500.0);
        state.judge_system.update(500.0, state.input_handler);

        play_note_draw_queue draw_queue;
        play_update_context context;
        context.dt = 0.0f;
        context.bgm_audio_time_ms = 1000.0;

        state.input_handler.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 1000.0);
        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (!state.display_judge.has_value() || state.display_judge->result != judge_result::perfect) {
            std::cerr << "Hold completion should surface perfect feedback\n";
            return EXIT_FAILURE;
        }
        if (state.lane_judge_effects[0].result != judge_result::perfect ||
            state.lane_judge_effects[0].timer <= 0.0f) {
            std::cerr << "Hold completion should trigger lane judge effects\n";
            return EXIT_FAILURE;
        }
        if (result.hitsound_count != 0 || state.score_system.get_combo() != 1) {
            std::cerr << "Hold completion should add score effects without replaying hitsounds\n";
            return EXIT_FAILURE;
        }
    }

    {
        play_session_state state = make_initialized_state();
        play_note_draw_queue draw_queue;
        play_update_context context;
        context.dt = 0.05f;
        context.bgm_audio_time_ms = 1010.0;

        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (result.navigation.has_value() || state.current_ms != 1010.0) {
            std::cerr << "Gameplay clock should follow audio time without running ahead on dt\n";
            return EXIT_FAILURE;
        }
    }

    {
        play_session_state state = make_initialized_state();
        state.score_system.init(1);
        state.timing_engine = make_basic_timing_engine();
        state.judge_system.init({note_data{note_type::tap, 480, 0, 480}}, state.timing_engine);
        state.hitsound_path = "hitsound.mp3";

        play_note_draw_queue draw_queue;
        int immediate_hitsound_count = 0;
        play_update_context context;
        context.dt = 0.0f;
        context.bgm_audio_time_ms = 500.0;
        context.input_already_updated = true;
        context.play_hitsound_immediately = [&immediate_hitsound_count]() {
            ++immediate_hitsound_count;
        };

        state.input_handler.update_from_lane_states(std::array<bool, 4>{true, false, false, false}, 500.0);
        const play_update_result result = play_flow_controller::update(state, draw_queue, context);
        if (state.lane_judge_effects[0].result != judge_result::perfect ||
            state.lane_judge_effects[0].timer <= 0.0f) {
            std::cerr << "Tap judgement should trigger lane judge effects\n";
            return EXIT_FAILURE;
        }
        if (immediate_hitsound_count != 1 || result.hitsound_count != 0) {
            std::cerr << "Gameplay hitsound should use the immediate callback when available\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "play_flow_controller smoke test passed\n";
    return EXIT_SUCCESS;
}
