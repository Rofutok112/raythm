#include "title/title_spectrum_visualizer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "audio_manager.h"
#include "theme.h"

namespace {

Color with_alpha_scale(Color color, float alpha_scale) {
    color.a = static_cast<unsigned char>(std::clamp(alpha_scale, 0.0f, 1.0f) * 255.0f);
    return color;
}

float average_bucket_energy(const std::array<float, 128>& spectrum, int begin, int end) {
    if (begin >= end) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (int i = begin; i < end; ++i) {
        sum += spectrum[static_cast<size_t>(i)];
    }
    return sum / static_cast<float>(end - begin);
}

}  // namespace

void title_spectrum_visualizer::reset() {
    bars_.fill(0.0f);
    peaks_.fill(0.0f);
    peak_velocities_.fill(0.0f);
    peak_hold_timers_.fill(0.0f);
    dynamic_peak_ = 0.12f;
}

void title_spectrum_visualizer::update() {
    std::array<float, 128> spectrum = {};
    const bool has_audio = audio_manager::instance().get_bgm_fft256(spectrum);
    std::array<float, kBarCount> targets = {};
    float frame_peak = 0.0f;
    constexpr float kFrequencyCurve = 1.55f;

    for (int i = 0; i < kBarCount; ++i) {
        float target = 0.0f;
        if (has_audio) {
            const float start_t = static_cast<float>(i) / static_cast<float>(kBarCount);
            const float end_t = static_cast<float>(i + 1) / static_cast<float>(kBarCount);
            int begin = static_cast<int>(std::floor(std::pow(start_t, kFrequencyCurve) * static_cast<float>(spectrum.size())));
            int end = static_cast<int>(std::floor(std::pow(end_t, kFrequencyCurve) * static_cast<float>(spectrum.size())));
            begin = std::clamp(begin, 0, static_cast<int>(spectrum.size()) - 1);
            end = std::clamp(end, begin + 1, static_cast<int>(spectrum.size()));

            const float averaged = average_bucket_energy(spectrum, begin, end);
            const float normalized = std::clamp(averaged, 0.0f, 1.0f);
            const float band_t = static_cast<float>(i) / static_cast<float>(kBarCount - 1);
            const float low_cut = 0.24f + band_t * 0.98f;
            const float detail_boost = 0.82f + std::sqrt(band_t) * 0.36f;
            target = std::pow(normalized, 0.82f) * low_cut * detail_boost;
        }
        targets[static_cast<size_t>(i)] = target;
        frame_peak = std::max(frame_peak, target);
    }

    if (frame_peak > dynamic_peak_) {
        dynamic_peak_ += (frame_peak - dynamic_peak_) * 0.18f;
    } else {
        dynamic_peak_ += (frame_peak - dynamic_peak_) * 0.04f;
    }
    dynamic_peak_ = std::max(0.12f, dynamic_peak_);

    for (int i = 0; i < kBarCount; ++i) {
        const float target = std::clamp(targets[static_cast<size_t>(i)] / dynamic_peak_, 0.0f, 1.0f);
        if (target > bars_[static_cast<size_t>(i)]) {
            bars_[static_cast<size_t>(i)] += (target - bars_[static_cast<size_t>(i)]) * 0.38f;
        } else {
            bars_[static_cast<size_t>(i)] += (target - bars_[static_cast<size_t>(i)]) * 0.15f;
        }

        const float bar_height = bars_[static_cast<size_t>(i)];
        if (bar_height > peaks_[static_cast<size_t>(i)]) {
            peaks_[static_cast<size_t>(i)] +=
                (bar_height - peaks_[static_cast<size_t>(i)]) * 0.24f;
            peak_velocities_[static_cast<size_t>(i)] = 0.0f;
            peak_hold_timers_[static_cast<size_t>(i)] = 7.0f;
        } else {
            if (peak_hold_timers_[static_cast<size_t>(i)] > 0.0f) {
                peak_hold_timers_[static_cast<size_t>(i)] -= 1.0f;
            } else {
                peak_velocities_[static_cast<size_t>(i)] += 0.0035f;
                peaks_[static_cast<size_t>(i)] =
                    std::max(0.0f, peaks_[static_cast<size_t>(i)] - peak_velocities_[static_cast<size_t>(i)]);
            }
        }
    }
}

void title_spectrum_visualizer::draw(const Rectangle& rect) const {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }

    const float gap = 3.0f;
    const float bar_width =
        (rect.width - gap * static_cast<float>(kBarCount - 1)) / static_cast<float>(kBarCount);
    const float baseline = rect.y + rect.height;
    const float max_height = rect.height * 0.55f;
    const Color base_low = {107, 33, 168, 128};
    const Color base_top = {216, 180, 254, 230};
    const Color peak_glow = {216, 180, 254, 110};
    const Color peak_color = {242, 230, 255, 220};

    for (int i = 0; i < kBarCount; ++i) {
        const float value = std::clamp(bars_[static_cast<size_t>(i)], 0.0f, 1.0f);
        const float height = value * max_height;
        const float x = rect.x + static_cast<float>(i) * (bar_width + gap);
        if (height > 0.5f) {
            const float y = baseline - height;
            DrawRectangleGradientV(
                static_cast<int>(x),
                static_cast<int>(y),
                static_cast<int>(bar_width),
                static_cast<int>(height),
                base_top,
                base_low);
        }

        const float peak_y = baseline - std::clamp(peaks_[static_cast<size_t>(i)], 0.0f, 1.0f) * max_height - 3.0f;
        DrawRectangleRec({x, peak_y - 1.0f, bar_width, 4.0f}, peak_glow);
        DrawRectangleRec({x, peak_y, bar_width, 2.0f}, peak_color);
    }
}
