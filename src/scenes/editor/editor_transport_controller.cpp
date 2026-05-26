#include "editor_transport_controller.h"

#include <algorithm>

namespace {

double chart_ms_for_tick(const editor_state& state, int tick) {
    if (tick >= 0) {
        return state.engine().tick_to_ms(tick);
    }

    const int resolution = std::max(1, state.data().meta.resolution);
    const double bpm = std::max(0.001, static_cast<double>(state.engine().get_bpm_at(0)));
    const double ms_per_tick = 60000.0 / (static_cast<double>(resolution) * bpm);
    return state.engine().tick_to_ms(0) + static_cast<double>(tick) * ms_per_tick;
}

editor_transport_result build_audio_state(const editor_transport_context& context) {
    editor_transport_result result;
    if (context.state == nullptr || !context.audio_loaded || !context.bgm_clock.has_value() || !context.bgm_clock->loaded) {
        return result;
    }

    result.audio_loaded = true;
    result.audio_playing = context.bgm_clock->playing;
    if (!result.audio_playing) {
        result.pre_audio_playing = context.pre_audio_playing;
        result.audio_time_seconds = context.pre_audio_playing
            ? context.audio_time_seconds + std::max(0.0, context.dt)
            : chart_ms_for_tick(*context.state, context.playback_tick) / 1000.0;
        if (result.pre_audio_playing && result.audio_time_seconds >= 0.0) {
            result.audio_time_seconds = 0.0;
            result.pre_audio_playing = false;
            result.request_play_bgm = true;
        }
        result.playback_tick = context.state->engine().ms_to_tick(result.audio_time_seconds * 1000.0);
        result.previous_playback_tick = result.playback_tick;
        result.previous_audio_playing = false;
        if (context.bgm_length_seconds.has_value()) {
            result.audio_length_tick = std::max(
                0, context.state->engine().ms_to_tick(*context.bgm_length_seconds * 1000.0));
        }
        return result;
    }

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
    result.pre_audio_playing = false;
    result.previous_playback_tick = context.previous_playback_tick;
    result.previous_audio_playing = context.previous_audio_playing;
    return result;
}

editor_hitsound_request hitsound_request_for_note(const note_data& note) {
    note_type type = note.type;
    if (type == note_type::hold) {
        type = note_type::tap;
    }
    return {type, note.is_ray};
}

int scheduled_hitsound_tick(const editor_transport_context& context,
                            const editor_transport_result& result) {
    if (context.state == nullptr) {
        return result.playback_tick;
    }

    const double lead_seconds = std::max(0.0, context.hitsound_schedule_lead_seconds);
    if (lead_seconds <= 0.0) {
        return result.playback_tick;
    }

    return std::max(
        result.playback_tick,
        context.state->engine().ms_to_tick((result.audio_time_seconds + lead_seconds) * 1000.0));
}

}  // namespace

editor_transport_result editor_transport_controller::sync(const editor_transport_context& context) {
    editor_transport_result result = build_audio_state(context);
    const std::string hitsound_path = context.hitsound_path == nullptr ? "" : *context.hitsound_path;
    const bool has_hitsounds = context.hitsounds != nullptr
        ? context.hitsounds->has_any()
        : !hitsound_path.empty();

    if (!result.audio_loaded || !has_hitsounds) {
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

    const int hitsound_until_tick = scheduled_hitsound_tick(context, result);
    if (hitsound_until_tick <= context.previous_playback_tick || context.state == nullptr) {
        result.previous_playback_tick = hitsound_until_tick;
        result.previous_audio_playing = true;
        return result;
    }

    const std::vector<size_t> note_indices =
        context.state->note_indices_in_tick_range(context.previous_playback_tick + 1, hitsound_until_tick);
    for (const size_t index : note_indices) {
        const note_data& note = context.state->data().notes[index];
        if (note.tick > context.previous_playback_tick && note.tick <= hitsound_until_tick) {
            ++result.hitsound_count;
            result.hitsound_requests.push_back(hitsound_request_for_note(note));
        }
    }

    result.previous_playback_tick = hitsound_until_tick;
    result.previous_audio_playing = true;
    return result;
}

editor_transport_result editor_transport_controller::toggle_playback(const editor_transport_context& context) {
    editor_transport_result result;
    if (!context.audio_loaded) {
        return result;
    }

    if (context.audio_playing || context.pre_audio_playing) {
        result.request_pause_bgm = true;
        result.pre_audio_playing = false;
        result.next_space_playback_start_tick = std::nullopt;
        if (context.state != nullptr && context.space_playback_start_tick.has_value()) {
            result.seek_bgm_seconds =
                chart_ms_for_tick(*context.state, *context.space_playback_start_tick) / 1000.0;
        }
        return result;
    }

    result.next_space_playback_start_tick = context.playback_tick;
    if (context.state != nullptr &&
        chart_ms_for_tick(*context.state, context.playback_tick) < 0.0) {
        result.audio_loaded = true;
        result.audio_playing = false;
        result.pre_audio_playing = true;
        result.audio_time_seconds = chart_ms_for_tick(*context.state, context.playback_tick) / 1000.0;
        result.playback_tick = context.playback_tick;
        result.previous_playback_tick = result.playback_tick;
        result.previous_audio_playing = false;
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

    result.seek_bgm_seconds = chart_ms_for_tick(*context.state, tick) / 1000.0;
    return result;
}
