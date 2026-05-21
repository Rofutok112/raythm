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
        {note_type::release, 960, 1, 960, true},
        {note_type::stay, 1200, 2, 1200, true},
    };
    return data;
}

chart_data make_negative_offset_chart() {
    chart_data data = make_chart();
    data.meta.chart_id = "negative-offset-transport-smoke";
    data.meta.offset = -300;
    return data;
}

chart_data make_positive_offset_chart() {
    chart_data data = make_chart();
    data.meta.chart_id = "positive-offset-transport-smoke";
    data.meta.offset = 300;
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
            result.hitsound_count != 1 || result.hitsound_requests.size() != 1 ||
            result.hitsound_requests.front().type != note_type::tap ||
            result.audio_length_tick <= 0 ||
            result.previous_playback_tick != 600 || !result.previous_audio_playing) {
            std::cerr << "sync should update playback state and hitsounds\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_hitsound_paths hitsounds;
        hitsounds.tap = "tap.mp3";
        hitsounds.ray_release = "ray-release.mp3";
        hitsounds.ray_stay = "ray-stay.mp3";
        editor_transport_context context;
        context.state = state.get();
        context.audio_loaded = true;
        context.previous_playback_tick = 900;
        context.previous_audio_playing = true;
        context.hitsounds = &hitsounds;
        context.bgm_clock = audio_clock_snapshot{true, true, 0.0, state->engine().tick_to_ms(1300) / 1000.0, 0.0, 0.0};
        context.bgm_length_seconds = 8.0;

        const editor_transport_result result = editor_transport_controller::sync(context);
        if (result.hitsound_requests.size() != 2 ||
            result.hitsound_requests[0].type != note_type::release ||
            !result.hitsound_requests[0].is_ray ||
            result.hitsound_requests[1].type != note_type::stay ||
            !result.hitsound_requests[1].is_ray ||
            hitsounds.path_for(result.hitsound_requests[0]) != "ray-release.mp3" ||
            hitsounds.path_for(result.hitsound_requests[1]) != "ray-stay.mp3") {
            std::cerr << "sync should preserve note type and ray state for editor hitsounds\n";
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
        const auto negative_offset_state = std::make_shared<editor_state>(make_negative_offset_chart(), "");
        editor_transport_context context;
        context.state = negative_offset_state.get();
        context.audio_loaded = true;
        context.playback_tick = 0;
        context.bgm_clock = audio_clock_snapshot{true, false, 0.0, 0.0, 0.0, 0.0};
        context.bgm_length_seconds = 8.0;

        const editor_transport_result result = editor_transport_controller::sync(context);
        if (!result.audio_loaded || result.audio_playing || result.playback_tick != 0 ||
            !nearly_equal(result.audio_time_seconds, -0.3)) {
            std::cerr << "stopped sync should preserve editor tick before audio zero\n";
            return EXIT_FAILURE;
        }
    }

    {
        const auto negative_offset_state = std::make_shared<editor_state>(make_negative_offset_chart(), "");
        editor_transport_context context;
        context.state = negative_offset_state.get();
        context.audio_loaded = true;
        context.audio_playing = false;
        context.playback_tick = 0;

        const editor_transport_result start = editor_transport_controller::toggle_playback(context);
        if (start.request_play_bgm || !start.pre_audio_playing ||
            !nearly_equal(start.audio_time_seconds, -0.3) ||
            start.next_space_playback_start_tick != 0) {
            std::cerr << "toggle_playback should start pre-audio playback before audio zero\n";
            return EXIT_FAILURE;
        }

        context.pre_audio_playing = true;
        context.audio_time_seconds = -0.3;
        context.bgm_clock = audio_clock_snapshot{true, false, 0.0, 0.0, 0.0, 0.0};
        context.dt = 0.2;
        const editor_transport_result before_zero = editor_transport_controller::sync(context);
        if (!before_zero.pre_audio_playing || before_zero.request_play_bgm ||
            !nearly_equal(before_zero.audio_time_seconds, -0.1) ||
            before_zero.playback_tick != 192) {
            std::cerr << "pre-audio playback should advance editor time before audio starts\n";
            return EXIT_FAILURE;
        }

        context.audio_time_seconds = -0.1;
        context.dt = 0.2;
        const editor_transport_result reaches_zero = editor_transport_controller::sync(context);
        if (reaches_zero.pre_audio_playing || !reaches_zero.request_play_bgm ||
            !nearly_equal(reaches_zero.audio_time_seconds, 0.0) ||
            reaches_zero.playback_tick != 288) {
            std::cerr << "pre-audio playback should request BGM at audio zero\n";
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

    {
        const auto positive_offset_state = std::make_shared<editor_state>(make_positive_offset_chart(), "");
        editor_transport_context context;
        context.state = positive_offset_state.get();
        context.audio_loaded = true;

        const editor_transport_result result = editor_transport_controller::seek_to_tick(context, -240);
        if (!result.seek_bgm_seconds.has_value() ||
            !nearly_equal(*result.seek_bgm_seconds, 0.05)) {
            std::cerr << "seek_to_tick should allow pre-chart ticks before 1:1\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_transport_context context;
        context.state = state.get();
        context.audio_loaded = true;

        const editor_transport_result result = editor_transport_controller::seek_to_tick(context, -240);
        if (!result.seek_bgm_seconds.has_value() ||
            !nearly_equal(*result.seek_bgm_seconds, -0.25)) {
            std::cerr << "seek_to_tick should preserve negative pre-audio seconds\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "editor_transport_controller smoke test passed\n";
    return EXIT_SUCCESS;
}
