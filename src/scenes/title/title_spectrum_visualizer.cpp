#include "title/title_spectrum_visualizer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "audio_manager.h"
#include "theme.h"

namespace {

constexpr std::array kSpectrumRanges = {
    1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 17, 20, 24, 29, 35, 42,
    50, 58, 66, 74, 82, 90, 98, 104, 109, 113, 117, 120, 123, 125, 127, 128
};

float average_band_energy(const std::array<float, 128>& spectrum, int begin, int end) {
    if (begin >= end) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (int i = begin; i < end; ++i) {
        sum += spectrum[static_cast<size_t>(i)];
    }
    return sum / static_cast<float>(end - begin);
}

Color with_alpha_scale(Color color, float alpha_scale) {
    color.a = static_cast<unsigned char>(std::clamp(alpha_scale, 0.0f, 1.0f) * 255.0f);
    return color;
}

Color fade_spectrum_color(Color base, float alpha_scale) {
    return with_alpha_scale(base, alpha_scale);
}

}  // namespace

void title_spectrum_visualizer::reset() {
    bars_.fill(0.0f);
    dynamic_peak_ = 0.12f;
}

void title_spectrum_visualizer::update() {
    std::array<float, 128> spectrum = {};
    const bool has_audio = audio_manager::instance().get_bgm_fft256(spectrum);
    std::array<float, kBarCount> targets = {};
    float frame_peak = 0.0f;

    for (int i = 0; i < kBarCount; ++i) {
        float target = 0.0f;
        if (has_audio) {
            const float band = average_band_energy(spectrum, kSpectrumRanges[static_cast<size_t>(i)],
                                                   kSpectrumRanges[static_cast<size_t>(i + 1)]);
            target = std::sqrt(std::max(0.0f, band)) * 7.5f;
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
            bars_[static_cast<size_t>(i)] += (target - bars_[static_cast<size_t>(i)]) * 0.45f;
        } else {
            bars_[static_cast<size_t>(i)] += (target - bars_[static_cast<size_t>(i)]) * 0.12f;
        }
    }
}

void title_spectrum_visualizer::draw(const Rectangle& rect) const {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }

    const auto& t = *g_theme;
    const float gap = 4.0f;
    const float bar_width =
        (rect.width - gap * static_cast<float>(kBarCount - 1)) / static_cast<float>(kBarCount);
    const float baseline = rect.y + rect.height;
    const Color base_color = t.accent;

    for (int i = 0; i < kBarCount; ++i) {
        const float value = std::clamp(bars_[static_cast<size_t>(i)], 0.0f, 1.0f);
        const float shaped_value = std::pow(value, 0.82f);
        const float min_height = rect.height * 0.012f;
        const float height = min_height + (rect.height - min_height) * shaped_value;
        const float x = rect.x + static_cast<float>(i) * (bar_width + gap);
        const Rectangle bar_rect = {x, baseline - height, bar_width, height};
        const float color_t = static_cast<float>(i) / static_cast<float>(kBarCount - 1);
        const float alpha_scale = 0.42f - color_t * 0.24f;
        DrawRectangleRec(bar_rect, fade_spectrum_color(base_color, alpha_scale));
    }
}
