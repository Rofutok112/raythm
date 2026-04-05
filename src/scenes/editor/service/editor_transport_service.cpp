#include "editor_transport_service.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "audio_manager.h"
#include "editor/editor_transport_controller.h"

namespace {

constexpr float kPlaybackRestartEpsilonSeconds = 0.01f;

editor_transport_context build_context(const editor_transport_state& transport,
                                       const editor_state* state,
                                       const std::string& hitsound_path,
                                       bool suppress_hitsounds) {
    editor_transport_context context;
    context.state = state;
    context.audio_loaded = transport.audio_loaded;
    context.audio_playing = transport.audio_playing;
    context.audio_time_seconds = transport.audio_time_seconds;
    context.playback_tick = transport.playback_tick;
    context.previous_playback_tick = suppress_hitsounds
        ? transport.playback_tick
        : transport.previous_playback_tick;
    context.previous_audio_playing = suppress_hitsounds
        ? false
        : transport.previous_audio_playing;
    context.hitsound_path = &hitsound_path;
    if (transport.audio_loaded && audio_manager::instance().is_bgm_loaded()) {
        context.bgm_clock = audio_manager::instance().get_bgm_clock();
        context.bgm_length_seconds = audio_manager::instance().get_bgm_length_seconds();
    }
    return context;
}

void apply_result(editor_transport_state& transport,
                  const editor_transport_result& result,
                  const std::string& hitsound_path) {
    transport.audio_loaded = result.audio_loaded;
    transport.audio_playing = result.audio_playing;
    transport.audio_time_seconds = result.audio_time_seconds;
    transport.playback_tick = result.playback_tick;
    transport.previous_playback_tick = result.previous_playback_tick;
    transport.previous_audio_playing = result.previous_audio_playing;
    transport.audio_length_tick = result.audio_length_tick;

    for (int i = 0; i < result.hitsound_count; ++i) {
        audio_manager::instance().play_se(hitsound_path, 0.45f);
    }
}

std::string format_playback_time(double seconds) {
    const int total_ms = std::max(0, static_cast<int>(std::lround(seconds * 1000.0)));
    const int minutes = total_ms / 60000;
    const int whole_seconds = (total_ms / 1000) % 60;
    const int centiseconds = (total_ms % 1000) / 10;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d.%02d", minutes, whole_seconds, centiseconds);
    return buffer;
}

}  // namespace

void editor_transport_service::sync(editor_transport_state& transport,
                                    const editor_state* state,
                                    const std::string& hitsound_path,
                                    bool suppress_hitsounds) {
    apply_result(
        transport,
        editor_transport_controller::sync(build_context(transport, state, hitsound_path, suppress_hitsounds)),
        hitsound_path);
}

std::optional<int> editor_transport_service::toggle_playback(editor_transport_state& transport,
                                                             const editor_state* state,
                                                             std::optional<int>& space_playback_start_tick,
                                                             const std::string& hitsound_path) {
    editor_transport_context context;
    context.state = state;
    context.audio_loaded = transport.audio_loaded;
    context.audio_playing = transport.audio_playing;
    context.playback_tick = transport.playback_tick;
    context.space_playback_start_tick = space_playback_start_tick;

    const std::optional<int> restore_tick = context.space_playback_start_tick;
    const editor_transport_result result = editor_transport_controller::toggle_playback(context);
    space_playback_start_tick = result.next_space_playback_start_tick;

    if (result.request_pause_bgm) {
        audio_manager::instance().pause_bgm();
        if (result.seek_bgm_seconds.has_value()) {
            audio_manager::instance().seek_bgm(*result.seek_bgm_seconds);
        }
    } else if (result.request_play_bgm && audio_manager::instance().is_bgm_loaded()) {
        const double length_seconds = audio_manager::instance().get_bgm_length_seconds();
        const double position_seconds = audio_manager::instance().get_bgm_position_seconds();
        const bool restart = length_seconds > 0.0 &&
                             position_seconds >= std::max(0.0, length_seconds - kPlaybackRestartEpsilonSeconds);
        audio_manager::instance().play_bgm(restart);
    }

    sync(transport, state, hitsound_path, true);
    return result.request_pause_bgm ? restore_tick : std::nullopt;
}

void editor_transport_service::pause_for_seek(editor_transport_state& transport,
                                              const editor_state* state,
                                              std::optional<int>& space_playback_start_tick,
                                              const std::string& hitsound_path) {
    audio_manager::instance().pause_bgm();
    space_playback_start_tick.reset();
    sync(transport, state, hitsound_path, true);
}

void editor_transport_service::seek_to_tick(editor_transport_state& transport,
                                            const editor_state* state,
                                            int tick,
                                            const std::string& hitsound_path) {
    editor_transport_context context;
    context.state = state;
    context.audio_loaded = transport.audio_loaded;
    const editor_transport_result result = editor_transport_controller::seek_to_tick(context, tick);
    if (!result.seek_bgm_seconds.has_value()) {
        return;
    }

    audio_manager::instance().seek_bgm(*result.seek_bgm_seconds);
    sync(transport, state, hitsound_path, true);
}

std::string editor_transport_service::playback_status_text(const editor_transport_state& transport) {
    if (!transport.audio_loaded) {
        return "No audio";
    }

    return std::string(transport.audio_playing ? "Playing " : "Paused ") +
        format_playback_time(transport.audio_time_seconds);
}
