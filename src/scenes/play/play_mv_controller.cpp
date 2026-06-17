#include "play/play_mv_controller.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>

#include "audio_manager.h"
#include "mv/api/mv_context.h"
#include "mv/mv_runtime.h"
#include "mv/mv_storage.h"
#include "mv/render/mv_renderer.h"
#include "raylib.h"
#include "scene_common.h"

namespace {

constexpr float kSpectrumClampMax = 2.0f;

int sample_waveform_index(const std::vector<float>& waveform, double chart_time_ms, double song_length_ms) {
    if (waveform.empty() || song_length_ms <= 0.0) {
        return 0;
    }

    const double progress = std::clamp(chart_time_ms / song_length_ms, 0.0, 1.0);
    const std::size_t last = waveform.size() - 1;
    return static_cast<int>(std::clamp<std::size_t>(
        static_cast<std::size_t>(progress * static_cast<double>(last)), 0, last));
}

void fill_spectrum(std::vector<float>& spectrum) {
    std::array<float, 128> fft = {};
    if (!audio_manager::instance().get_bgm_fft256(fft)) {
        spectrum.clear();
        return;
    }

    spectrum.resize(fft.size());
    for (std::size_t i = 0; i < fft.size(); ++i) {
        const float shaped = std::sqrt(std::max(0.0f, fft[i])) * 8.0f;
        spectrum[i] = std::clamp(shaped, 0.0f, kSpectrumClampMax);
    }
}

void fill_oscilloscope(std::vector<float>& oscilloscope) {
    std::array<float, 256> pcm = {};
    if (!audio_manager::instance().get_bgm_oscilloscope256(pcm)) {
        oscilloscope.clear();
        return;
    }

    oscilloscope.resize(pcm.size());
    std::copy(pcm.begin(), pcm.end(), oscilloscope.begin());
}

float oscilloscope_rms(const std::vector<float>& oscilloscope) {
    if (oscilloscope.empty()) {
        return 0.0f;
    }

    double sum = 0.0;
    for (float sample : oscilloscope) {
        sum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(oscilloscope.size())));
}

float oscilloscope_peak(const std::vector<float>& oscilloscope) {
    float peak = 0.0f;
    for (float sample : oscilloscope) {
        peak = std::max(peak, std::fabs(sample));
    }
    return peak;
}

float spectrum_band_average(const std::vector<float>& spectrum, std::size_t start, std::size_t end) {
    if (start >= end || start >= spectrum.size()) {
        return 0.0f;
    }

    const std::size_t clamped_end = std::min(end, spectrum.size());
    double sum = 0.0;
    for (std::size_t i = start; i < clamped_end; ++i) {
        sum += spectrum[i];
    }
    return static_cast<float>(sum / static_cast<double>(clamped_end - start));
}

void fill_timing_context(mv::context_input& input, const play_session_state& state) {
    const int current_tick = state.timing_engine.ms_to_tick(state.chart_time_ms);
    input.bpm = state.timing_engine.get_bpm_at(current_tick);
    input.meter_numerator = state.timing_engine.get_meter_numerator_at(current_tick);
    input.meter_denominator = state.timing_engine.get_meter_denominator_at(current_tick);

    const double beat_duration_ms = 60000.0 / input.bpm;
    if (beat_duration_ms <= 0.0) {
        return;
    }
    double ms_in_beat = std::fmod(state.chart_time_ms, beat_duration_ms);
    if (ms_in_beat < 0.0) {
        ms_in_beat += beat_duration_ms;
    }
    input.beat_phase = static_cast<float>(ms_in_beat / beat_duration_ms);
    input.beat_number = static_cast<int>(state.chart_time_ms / beat_duration_ms);
}

void fill_chart_context(mv::context_input& input, const play_session_state& state) {
    if (state.song_data.has_value()) {
        input.song_id = state.song_data->meta.song_id;
        input.song_title = state.song_data->meta.title;
        input.song_artist = state.song_data->meta.artist;
        input.song_base_bpm = state.song_data->meta.base_bpm;
    }
    if (state.chart_data.has_value()) {
        input.chart_id = state.chart_data->meta.chart_id;
        input.chart_song_id = state.chart_data->meta.song_id;
        input.chart_difficulty = state.chart_data->meta.difficulty;
        input.chart_level = state.chart_data->meta.level;
        input.chart_author = state.chart_data->meta.chart_author;
        input.chart_resolution = state.chart_data->meta.resolution;
        input.chart_offset = state.chart_data->meta.offset;
        input.total_notes = static_cast<int>(state.chart_data->notes.size());
    }
}

}  // namespace

play_mv_controller::play_mv_controller() = default;
play_mv_controller::~play_mv_controller() = default;

void play_mv_controller::load_for_song(const std::optional<song_data>& song) {
    reset();
    if (!song.has_value()) {
        TraceLog(LOG_INFO, "MV: no song_data, skipping script load");
        return;
    }

    const auto package = mv::find_first_package_for_song(song->meta.song_id);
    if (!package.has_value()) {
        TraceLog(LOG_INFO, "MV: script file not found");
        return;
    }

    const auto script_file = mv::script_path(*package);
    TraceLog(LOG_INFO, "MV: looking for script at %s", script_file.string().c_str());
    runtime_ = std::make_unique<mv::mv_runtime>();
    if (runtime_->load_file(script_file.string())) {
        TraceLog(LOG_INFO, "MV: script loaded OK");
        return;
    }

    TraceLog(LOG_WARNING, "MV: compile failed");
    for (const auto& error : runtime_->last_errors()) {
        TraceLog(LOG_WARNING, "MV:   L%d: %s (%s)", error.line, error.message.c_str(), error.phase.c_str());
    }
    runtime_.reset();
}

void play_mv_controller::reset() {
    runtime_.reset();
    spectrum_buffer_.clear();
    oscilloscope_buffer_.clear();
}

void play_mv_controller::draw(const play_session_state& state, double visual_time_ms) {
    if (runtime_ == nullptr || !runtime_->is_loaded()) {
        return;
    }

    mv::context_input input;
    input.current_ms = visual_time_ms;
    input.song_length_ms = state.song_end_chart_time_ms;
    fill_timing_context(input, state);
    input.combo = state.combo_display;
    if (state.last_judge.has_value() &&
        state.last_judge->is_ray &&
        state.last_judge->result != judge_result::miss) {
        input.ray_pulse = 1.0f;
        input.ray_lane = state.last_judge->lane;
    }
    input.key_count = state.key_count;

    fill_spectrum(spectrum_buffer_);
    input.spectrum = spectrum_buffer_;
    fill_oscilloscope(oscilloscope_buffer_);
    input.oscilloscope = &oscilloscope_buffer_;
    input.rms = oscilloscope_rms(oscilloscope_buffer_);
    input.peak = oscilloscope_peak(oscilloscope_buffer_);
    const std::size_t spectrum_size = input.spectrum.size();
    const std::size_t first_split = spectrum_size / 3;
    const std::size_t second_split = (spectrum_size * 2) / 3;
    input.low = spectrum_band_average(input.spectrum, 0, first_split);
    input.mid = spectrum_band_average(input.spectrum, first_split, second_split);
    input.high = spectrum_band_average(input.spectrum, second_split, spectrum_size);

    input.waveform = &state.mv_waveform;
    input.waveform_index = sample_waveform_index(state.mv_waveform, input.current_ms, input.song_length_ms);
    if (!state.mv_waveform.empty()) {
        input.level = state.mv_waveform[static_cast<std::size_t>(input.waveform_index)];
    }
    fill_chart_context(input, state);
    input.screen_w = static_cast<float>(kScreenWidth);
    input.screen_h = static_cast<float>(kScreenHeight);

    try {
        const mv::scene* scene = runtime_->tick_ref(input);
        if (scene != nullptr) {
            static int mv_log_count = 0;
            if (mv_log_count < 3) {
                TraceLog(LOG_INFO, "MV: tick OK, %d nodes", static_cast<int>(scene->nodes.size()));
                ++mv_log_count;
            }
            mv::render_scene(*scene);
            return;
        }

        static int mv_fail_count = 0;
        if (mv_fail_count < 3) {
            TraceLog(LOG_WARNING, "MV: tick returned nullopt");
            for (const auto& error : runtime_->last_errors()) {
                TraceLog(LOG_WARNING, "MV:   L%d: %s (%s)", error.line, error.message.c_str(), error.phase.c_str());
            }
            ++mv_fail_count;
        }
    } catch (const std::exception& ex) {
        TraceLog(LOG_WARNING, "MV: disabled after exception: %s", ex.what());
        reset();
    } catch (...) {
        TraceLog(LOG_WARNING, "MV: disabled after unknown exception");
        reset();
    }
}
