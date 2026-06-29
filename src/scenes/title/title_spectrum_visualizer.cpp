#include "title/title_spectrum_visualizer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

#include "audio_manager.h"
#include "ui_draw.h"

namespace {

template <std::size_t N>
float average_bucket_energy(const std::array<float, N>& spectrum, int begin, int end) {
    if (begin >= end) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (int i = begin; i < end; ++i) {
        sum += spectrum[static_cast<size_t>(i)];
    }
    return sum / static_cast<float>(end - begin);
}

float smoothstep01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float log_frequency_fade(float frequency_hz, float start_hz, float end_hz) {
    const float safe_frequency = std::max(frequency_hz, 1.0f);
    const float t =
        (std::log(safe_frequency) - std::log(start_hz)) / (std::log(end_hz) - std::log(start_hz));
    return smoothstep01(t);
}

float perceptual_correction_db(float frequency_hz) {
    const float low_reduction = (1.0f - log_frequency_fade(frequency_hz, 80.0f, 900.0f)) * -14.0f;
    const float presence_rise = log_frequency_fade(frequency_hz, 900.0f, 2400.0f) * 12.0f;
    const float air_rolloff = log_frequency_fade(frequency_hz, 6500.0f, 14000.0f) * -8.0f;
    return low_reduction + presence_rise + air_rolloff;
}

float amplitude_to_db_level(float amplitude, float frequency_hz) {
    constexpr float kMinAmplitude = 0.00001f;
    constexpr float kDbFloor = -42.0f;
    constexpr float kDbCeiling = -18.0f;
    const float db = 20.0f * std::log10(std::max(amplitude, kMinAmplitude));
    return std::clamp((db + perceptual_correction_db(frequency_hz) - kDbFloor) / (kDbCeiling - kDbFloor), 0.0f, 1.0f);
}

float suppress_below_threshold(float level) {
    constexpr float kGateThreshold = 0.24f;
    if (level >= kGateThreshold) {
        return level;
    }

    const float t = std::clamp(level / kGateThreshold, 0.0f, 1.0f);
    return kGateThreshold * std::pow(t, 5.2f);
}

float visual_dt(float dt) {
    if (dt <= 0.0f) {
        return 1.0f / 240.0f;
    }
    return std::min(dt, 1.0f / 20.0f);
}

float blend_for_dt(float dt, float time_constant_seconds) {
    return 1.0f - std::exp(-dt / std::max(time_constant_seconds, 0.0001f));
}

}  // namespace

void title_spectrum_visualizer::reset() {
    smoothed_levels_.fill(0.0f);
    bars_.fill(0.0f);
    peaks_.fill(0.0f);
    peak_velocities_.fill(0.0f);
    peak_hold_times_.fill(0.0f);
}

void title_spectrum_visualizer::update(source input_source, float dt) {
    std::array<float, 2048> spectrum = {};
    const auto& audio = audio_manager::instance();
    const bool has_audio =
        input_source == source::preview
            ? audio.get_preview_fft4096(spectrum)
            : audio.get_bgm_fft4096(spectrum);
    const float sample_rate_hz = static_cast<float>(
        input_source == source::preview
            ? audio.get_preview_sample_rate_hz()
            : audio.get_bgm_sample_rate_hz());
    std::array<float, kBarCount> raw_levels = {};
    constexpr float kNoiseFloor = 0.002f;

    if (has_audio && sample_rate_hz > 0.0f) {
        constexpr float kMinFrequencyHz = 1.0f;
        constexpr float kMaxFrequencyHz = 16000.0f;
        constexpr float kFftSize = 4096.0f;
        const float nyquist_hz = std::max(sample_rate_hz * 0.5f, kMinFrequencyHz * 2.0f);
        const float max_frequency_hz = std::min(kMaxFrequencyHz, nyquist_hz * 0.96f);
        const float bin_width_hz = sample_rate_hz / kFftSize;
        const int min_bin = std::max(1, static_cast<int>(std::floor(kMinFrequencyHz / bin_width_hz)));
        const int max_bin = std::clamp(
            static_cast<int>(std::ceil(max_frequency_hz / bin_width_hz)),
            min_bin + kBarCount,
            static_cast<int>(spectrum.size()));
        const float bin_span = static_cast<float>(max_bin) / static_cast<float>(min_bin);
        std::array<int, kBarCount + 1> bin_edges = {};
        bin_edges[0] = min_bin;
        for (int i = 1; i <= kBarCount; ++i) {
            const float edge_t = static_cast<float>(i) / static_cast<float>(kBarCount);
            const int requested_edge = static_cast<int>(
                std::ceil(static_cast<float>(min_bin) * std::pow(bin_span, edge_t)));
            const int min_allowed = bin_edges[static_cast<size_t>(i - 1)] + 1;
            const int max_allowed = max_bin - (kBarCount - i);
            bin_edges[static_cast<size_t>(i)] = std::clamp(requested_edge, min_allowed, max_allowed);
        }

        for (int i = 0; i < kBarCount; ++i) {
            const int begin = bin_edges[static_cast<size_t>(i)];
            const int end = bin_edges[static_cast<size_t>(i + 1)];

            const float averaged = average_bucket_energy(spectrum, begin, end);
            const float center_hz = (static_cast<float>(begin + end) * 0.5f) * bin_width_hz;
            raw_levels[static_cast<size_t>(i)] =
                suppress_below_threshold(amplitude_to_db_level(averaged, center_hz));
        }
    }

    const float frame_dt = visual_dt(dt);
    for (int i = 0; i < kBarCount; ++i) {
        const int left_index = std::max(0, i - 1);
        const int right_index = std::min(kBarCount - 1, i + 1);
        const float spatially_smoothed =
            raw_levels[static_cast<size_t>(left_index)] * 0.2f +
            raw_levels[static_cast<size_t>(i)] * 0.6f +
            raw_levels[static_cast<size_t>(right_index)] * 0.2f;
        float target = std::clamp(spatially_smoothed, 0.0f, 1.0f);
        if (target < kNoiseFloor) {
            target = 0.0f;
        }

        const std::size_t index = static_cast<std::size_t>(i);
        const float input_blend = blend_for_dt(
            frame_dt,
            target > smoothed_levels_[index] ? 0.006f : 0.026f);
        smoothed_levels_[index] += (target - smoothed_levels_[index]) * input_blend;

        const float display_target = smoothed_levels_[index];
        const float bar_blend = blend_for_dt(
            frame_dt,
            display_target > bars_[index] ? 0.004f : 0.018f);
        bars_[index] += (display_target - bars_[index]) * bar_blend;

        const float peak_target = bars_[index];
        if (peak_target > peaks_[index]) {
            peaks_[index] = peak_target;
            peak_velocities_[index] = 0.0f;
            peak_hold_times_[index] = 0.001f;
        } else {
            if (peak_hold_times_[index] > 0.0f) {
                peak_hold_times_[index] = std::max(0.0f, peak_hold_times_[index] - frame_dt);
                peaks_[index] = std::max(peaks_[index], peak_target);
            } else {
                constexpr float kPeakGravity = 64.0f;
                constexpr float kPeakMaxFallSpeed = 32.0f;
                peak_velocities_[index] =
                    std::min(kPeakMaxFallSpeed, peak_velocities_[index] + kPeakGravity * frame_dt);
                peaks_[index] =
                    std::max(peak_target, peaks_[index] - peak_velocities_[index] * frame_dt);
            }
        }
    }
}

void title_spectrum_visualizer::draw(const Rectangle& rect, float alpha_scale) const {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }

    const float clamped_alpha = std::clamp(alpha_scale, 0.0f, 1.0f);
    ui::draw_block_spectrum(rect, bars_, peaks_, {
        .alpha_scale = clamped_alpha,
        .gap = 3.0f,
        .block_height = 8.0f,
        .block_gap = 4.0f,
    });
}
