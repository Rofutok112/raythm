#include "play_session_loader.h"

#include <algorithm>
#include <filesystem>
#include <optional>

#include "app_paths.h"
#include "audio_manager.h"
#include "audio_waveform.h"
#include "chart_judge_events.h"
#include "chart_level_cache.h"
#include "game_settings.h"
#include "path_utils.h"
#include "player_note_offsets.h"
#include "play_chart_filter.h"
#include "play_speed_compensation.h"
#include "ranking_service.h"

namespace {

std::optional<song_data> load_sample_song() {
    song_load_result result = song_loader::load_all(path_utils::to_utf8(app_paths::songs_root()));
    if (result.songs.empty()) {
        return std::nullopt;
    }
    return result.songs.front();
}

std::optional<chart_data> load_chart_for_key_count(const song_data& song, int key_count) {
    for (const std::string& chart_path : song.chart_paths) {
        const chart_parse_result parse_result = song_loader::load_chart(chart_path);
        if (parse_result.success && parse_result.data.has_value() &&
            parse_result.data->meta.key_count == key_count) {
            chart_data chart = *parse_result.data;
            if (const std::optional<float> cached_level = chart_level_cache::find_level(chart_path);
                cached_level.has_value()) {
                chart.meta.level = *cached_level;
            }
            return chart;
        }
    }
    return std::nullopt;
}

double calculate_song_end_ms(const chart_data& chart, const timing_engine& engine, const audio_manager& audio) {
    int last_tick = 0;
    for (const note_data& note : chart.notes) {
        last_tick = std::max(last_tick, note.type == note_type::hold ? note.end_tick : note.tick);
    }

    const double chart_tail_ms = engine.tick_to_ms(last_tick) + play_session_constants::kChartEndTailMs;
    const double audio_end_ms = audio.get_bgm_length_seconds() * 1000.0;
    return audio_end_ms > 0.0 ? std::min(chart_tail_ms, audio_end_ms) : chart_tail_ms;
}

std::vector<float> build_mv_waveform(const std::filesystem::path& audio_path) {
    constexpr std::size_t kMvWaveformSegmentCount = 512;

    const audio_waveform_summary summary = audio_waveform::build(path_utils::to_utf8(audio_path), kMvWaveformSegmentCount);
    std::vector<float> waveform;
    waveform.reserve(summary.peaks.size());
    for (const audio_waveform_peak& peak : summary.peaks) {
        waveform.push_back(peak.amplitude);
    }
    return waveform;
}

std::optional<double> detect_first_audible_ms(const std::filesystem::path& audio_path) {
    constexpr std::size_t kAnalysisSegmentCount = 4096;
    constexpr float kAudibleThreshold = 0.02f;

    const audio_waveform_summary summary =
        audio_waveform::build(path_utils::to_utf8(audio_path), kAnalysisSegmentCount);
    for (const audio_waveform_peak& peak : summary.peaks) {
        if (peak.amplitude >= kAudibleThreshold) {
            return peak.seconds * 1000.0;
        }
    }
    return std::nullopt;
}

std::string audio_asset_path(const char* file_name) {
    const std::filesystem::path path = app_paths::audio_root() / file_name;
    return std::filesystem::exists(path) ? path_utils::to_utf8(path) : "";
}

play_hitsound_paths load_hitsound_paths() {
    return {
        audio_asset_path("HitSound_Tap.mp3"),
        audio_asset_path("HitSound_RayTap.mp3"),
        audio_asset_path("HitSound_Release.mp3"),
        audio_asset_path("HitSound_RayRelease.mp3"),
        audio_asset_path("HitSound_Stay.mp3"),
        audio_asset_path("HitSound_RayStay.mp3"),
    };
}

void preload_hitsounds(audio_manager& audio, const play_hitsound_paths& hitsounds) {
    const std::string* paths[] = {
        &hitsounds.tap,
        &hitsounds.ray_tap,
        &hitsounds.release,
        &hitsounds.ray_release,
        &hitsounds.stay,
        &hitsounds.ray_stay,
    };
    for (const std::string* path : paths) {
        if (path != nullptr && !path->empty()) {
            audio.preload_se(*path);
        }
    }
}

}  // namespace

namespace play_session_loader {

play_session_state load(const play_start_request& request, play_note_draw_queue& draw_queue) {
    play_session_state state;
    state.key_count = request.key_count;
    state.song_data = request.song_data;
    state.selected_chart_path = request.selected_chart_path;
    state.chart_data = request.chart_data;
    state.editor_resume_state = request.editor_resume_state;
    state.start_tick = std::max(0, request.start_tick);
    state.camera_angle_degrees = g_settings.camera_angle_degrees;
    state.lane_speed =
        play_speed_compensation::compensated_lane_speed(g_settings.note_speed, state.camera_angle_degrees);

    if (!state.song_data.has_value()) {
        state.song_data = load_sample_song();
        if (!state.song_data.has_value()) {
            state.status_text = "No playable song package found";
            draw_queue.clear();
            return state;
        }
    }

    state.hitsounds = load_hitsound_paths();
    state.hitsound_path = state.hitsounds.tap;
    if (state.hitsound_path.empty()) {
        state.hitsound_path = audio_asset_path("hitsound.mp3");
        state.hitsounds.tap = state.hitsound_path;
    }

    if (state.chart_data.has_value()) {
        state.key_count = state.chart_data->meta.key_count;
    } else if (state.selected_chart_path.has_value()) {
        const chart_parse_result parse_result = song_loader::load_chart(*state.selected_chart_path);
        if (parse_result.success && parse_result.data.has_value()) {
            state.chart_data = parse_result.data;
            state.key_count = state.chart_data->meta.key_count;
            if (request.selected_chart_level.has_value()) {
                state.chart_data->meta.level = *request.selected_chart_level;
            } else if (const std::optional<float> cached_level =
                           chart_level_cache::find_level(*state.selected_chart_path);
                       cached_level.has_value()) {
                state.chart_data->meta.level = *cached_level;
            }
        } else {
            state.status_text = "Failed to load selected chart";
            draw_queue.clear();
            return state;
        }
    } else {
        state.chart_data = load_chart_for_key_count(*state.song_data, state.key_count);
    }

    if (!state.chart_data.has_value()) {
        state.status_text = "No chart found for selected key mode";
        draw_queue.clear();
        return state;
    }

    state.chart_data->meta.song_id = state.song_data->meta.song_id;
    state.chart_data = play_chart_filter::prepare_chart_for_playback(*state.chart_data, state.start_tick);

    state.input_handler = input_handler(g_settings.keys);
    state.input_handler.set_key_count(state.key_count);

    const int player_chart_offset_ms =
        load_player_chart_offset(state.chart_data->meta.chart_id);
    const int effective_chart_time_offset_ms =
        state.chart_data->meta.offset + g_settings.global_note_offset_ms + player_chart_offset_ms;
    state.timing_engine.init(state.chart_data->timing_events, state.chart_data->meta.resolution, effective_chart_time_offset_ms);
    state.scroll_map.init(*state.chart_data, state.timing_engine);
    state.start_ms = state.timing_engine.tick_to_ms(state.start_tick);
    state.judge_system.init(state.chart_data->notes, state.timing_engine);
    if (!state.editor_resume_state.has_value()) {
        ranking_service::refresh_scoring_ruleset_cache_for_chart_start(state.chart_data->meta, false);
    }
    state.score_system.init(chart_judge_events::count(*state.chart_data, state.timing_engine));
    state.performance_system.init(*state.chart_data, state.timing_engine);
    state.gauge = gauge{};

    audio_manager& audio = audio_manager::instance();
    const std::filesystem::path audio_path =
        path_utils::join_utf8(state.song_data->directory, state.song_data->meta.audio_file);
    audio.load_bgm(path_utils::to_utf8(audio_path));
    audio.set_bgm_volume(g_settings.bgm_volume);
    preload_hitsounds(audio, state.hitsounds);
    state.mv_waveform = build_mv_waveform(audio_path);
    if (!state.editor_resume_state.has_value() && state.start_tick == 0) {
        if (const std::optional<double> first_audible_ms = detect_first_audible_ms(audio_path);
            first_audible_ms.has_value()) {
            state.start_ms = *first_audible_ms - play_session_constants::kAudioLeadInBeforeFirstSoundMs;
        }
    }
    if (state.start_ms > 0.0) {
        audio.seek_bgm(state.start_ms / 1000.0);
    } else {
        audio.seek_bgm(0.0);
    }

    draw_queue.init_from_note_states(state.key_count, state.judge_system.note_states(), &state.scroll_map);

    state.song_end_chart_time_ms = calculate_song_end_ms(*state.chart_data, state.timing_engine, audio);
    state.chart_time_ms = state.start_ms;
    state.paused_chart_time_ms = state.start_ms;
    state.paused = false;
    state.ranking_enabled = !state.editor_resume_state.has_value();
    state.auto_paused_by_focus = false;
    state.initialized = true;
    state.status_text.clear();
    state.last_judge.reset();
    state.display_judge.reset();
    state.lane_hold_dim_amounts.fill(0.0f);
    state.lane_judge_effects.fill(lane_judge_effect{});
    state.final_result = {};
    state.judge_feedback_timer = 0.0f;
    state.intro_playing = play_session_constants::kIntroDurationSeconds > 0.0f;
    state.intro_timer = play_session_constants::kIntroDurationSeconds;
    state.failure_transition_playing = false;
    state.failure_transition_timer = 0.0f;
    state.result_transition_playing = false;
    state.result_transition_timer = 0.0f;
    state.chart_end_fade_started = false;
    state.combo_display = 0;
    return state;
}

}  // namespace play_session_loader
