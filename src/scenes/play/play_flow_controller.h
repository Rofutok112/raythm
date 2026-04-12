#pragma once

#include <functional>
#include <optional>

#include "play_note_draw_queue.h"
#include "play_session_types.h"

struct play_update_context {
    float dt = 0.0f;
    bool escape_pressed = false;
    bool enter_pressed = false;
    bool left_click_pressed = false;
    bool backspace_pressed = false;
    bool window_focused = true;
    bool bgm_loaded = false;
    std::optional<double> bgm_audio_time_ms;
    bool pause_resume_clicked = false;
    bool pause_restart_clicked = false;
    bool pause_song_select_clicked = false;
    std::optional<play_draw_window> draw_window;
    std::function<void()> play_hitsound_immediately;
};

struct play_update_result {
    play_navigation_request navigation;
    bool request_play_bgm = false;
    bool request_pause_bgm = false;
    int hitsound_count = 0;
};

class play_flow_controller final {
public:
    static play_update_result update(play_session_state& state, play_note_draw_queue& draw_queue,
                                     const play_update_context& context);
};
