#include "editor_transport_controller.h"

#include <algorithm>

namespace {

editor_transport_result build_audio_state(const editor_transport_context& context) {
    editor_transport_result result;
    if (context.state == nullptr || !context.audio_loaded || !context.bgm_clock.has_value() || !context.bgm_clock->loaded) {
        return result;
    }

    result.audio_loaded = true;
    result.audio_playing = context.bgm_clock->playing;

    double seconds = result.audio_playing
        ? context.bgm_clock->audio_time_seconds
        : context.bgm_clock->stream_position_seconds;
    if (context.bgm_length_seconds.has_value()) {
        seconds = std::clamp(seconds, 0.0, *context.bgm_length_seconds);
        result.audio_length_tick = std::max(
            0, context.state->engine().ms_to_tick(*context.bgm_length_seconds * 1000.0));
    } else {
        seconds = std::max(0.0, seconds);
    }

    result.audio_time_seconds = seconds;
    result.playback_tick = context.state->engine().ms_to_tick(seconds * 1000.0);
    result.previous_playback_tick = context.previous_playback_tick;
    result.previous_audio_playing = context.previous_audio_playing;
    return result;
}

}  // namespace

editor_transport_result editor_transport_controller::sync(const editor_transport_context& context) {
    editor_transport_result result = build_audio_state(context);
    const std::string hitsound_path = context.hitsound_path == nullptr ? "" : *context.hitsound_path;

    if (!result.audio_loaded || hitsound_path.empty()) {
        result.previous_playback_tick = result.playback_tick;
        result.previous_audio_playing = result.audio_playing;
        return result;
    }

    if (!result.audio_playing) {
        result.previous_playback_tick = result.playback_tick;
        result.previous_audio_playing = false;
        return result;
    }

    if (!context.previous_audio_playing) {
        result.previous_playback_tick = result.playback_tick;
        result.previous_audio_playing = true;
        return result;
    }

    if (result.playback_tick <= context.previous_playback_tick || context.state == nullptr) {
        result.previous_playback_tick = result.playback_tick;
        result.previous_audio_playing = true;
        return result;
    }

    for (const note_data& note : context.state->data().notes) {
        if (note.tick > context.previous_playback_tick && note.tick <= result.playback_tick) {
            ++result.hitsound_count;
        }
    }

    result.previous_playback_tick = result.playback_tick;
    result.previous_audio_playing = true;
    return result;
}

editor_transport_result editor_transport_controller::toggle_playback(const editor_transport_context& context) {
    editor_transport_result result;
    if (!context.audio_loaded) {
        return result;
    }

    if (context.audio_playing) {
        result.request_pause_bgm = true;
        return result;
    }

    result.request_play_bgm = true;
    return result;
}

editor_transport_result editor_transport_controller::seek_to_tick(const editor_transport_context& context, int tick) {
    editor_transport_result result;
    if (context.state == nullptr || !context.audio_loaded) {
        return result;
    }

    const int clamped_tick = std::max(0, tick);
    result.seek_bgm_seconds = context.state->engine().tick_to_ms(clamped_tick) / 1000.0;
    return result;
}
