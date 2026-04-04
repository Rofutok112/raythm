#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "editor/editor_transport_controller.h"

namespace {

bool nearly_equal(double left, double right) {
    return std::fabs(left - right) < 0.001;
}

chart_data make_chart() {
    chart_data data;
    data.meta.chart_id = "transport-smoke";
    data.meta.song_id = "song";
    data.meta.key_count = 4;
    data.meta.difficulty = "Normal";
    data.meta.chart_author = "Codex";
    data.meta.format_version = 1;
    data.meta.resolution = 480;
    data.meta.offset = 0;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    data.notes = {
        {note_type::tap, 240, 0, 240},
        {note_type::tap, 480, 1, 480},
        {note_type::tap, 720, 2, 720},
    };
    return data;
}

}  // namespace

int main() {
    const auto state = std::make_shared<editor_state>(make_chart(), "");

    {
        const std::string hitsound_path = "hitsound.mp3";
        editor_transport_context context;
        context.state = state.get();
        context.audio_loaded = true;
        context.previous_playback_tick = 300;
        context.previous_audio_playing = true;
        context.hitsound_path = &hitsound_path;
        context.bgm_clock = audio_clock_snapshot{true, true, 0.0, state->engine().tick_to_ms(600) / 1000.0, 0.0, 0.0};
        context.bgm_length_seconds = 8.0;

        const editor_transport_result result = editor_transport_controller::sync(context);

        if (!result.audio_loaded || !result.audio_playing || result.playback_tick != 600 ||
            result.hitsound_count != 1 || result.audio_length_tick <= 0 ||
            result.previous_playback_tick != 600 || !result.previous_audio_playing) {
            std::cerr << "sync should update playback state and hitsounds\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_transport_context context;
        context.state = state.get();
        context.audio_loaded = true;
        context.audio_playing = false;
        context.playback_tick = 480;

        const editor_transport_result result = editor_transport_controller::toggle_playback(context);
        if (!result.request_play_bgm || result.request_pause_bgm ||
            !result.next_space_playback_start_tick.has_value() ||
            *result.next_space_playback_start_tick != 480) {
            std::cerr << "toggle_playback should request play for paused audio\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_transport_context context;
        context.state = state.get();
        context.audio_loaded = true;
        context.audio_playing = true;
        context.space_playback_start_tick = 240;

        const editor_transport_result result = editor_transport_controller::toggle_playback(context);
        if (!result.request_pause_bgm || result.request_play_bgm ||
            !result.seek_bgm_seconds.has_value() ||
            !nearly_equal(*result.seek_bgm_seconds, state->engine().tick_to_ms(240) / 1000.0) ||
            result.next_space_playback_start_tick.has_value()) {
            std::cerr << "toggle_playback should request pause for playing audio\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_transport_context context;
        context.state = state.get();
        context.audio_loaded = true;

        const editor_transport_result result = editor_transport_controller::seek_to_tick(context, 480);
        if (!result.seek_bgm_seconds.has_value() ||
            !nearly_equal(*result.seek_bgm_seconds, state->engine().tick_to_ms(480) / 1000.0)) {
            std::cerr << "seek_to_tick should convert tick to audio seconds\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "editor_transport_controller smoke test passed\n";
    return EXIT_SUCCESS;
}
