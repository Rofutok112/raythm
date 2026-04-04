#include "title/title_spectrum_visualizer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "audio_manager.h"

namespace {

constexpr Color kSpectrumBarColor = {182, 186, 194, 92};

constexpr std::array<int, title_spectrum_visualizer::kBarCount + 1> kSpectrumRanges = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 20, 22, 24, 26, 28, 30, 33, 36, 39, 42, 46, 50, 54, 58,
    63, 68, 73, 78, 84, 90, 96, 102, 108, 112, 116, 119, 122, 124, 126, 127, 128
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

    const float gap = 2.0f;
    const float bar_width =
        (rect.width - gap * static_cast<float>(kBarCount - 1)) / static_cast<float>(kBarCount);
    const float baseline = rect.y + rect.height;
    const float max_height = rect.height * 0.7f;

    for (int i = 0; i < kBarCount; ++i) {
        const float value = std::clamp(bars_[static_cast<size_t>(i)], 0.0f, 1.0f);
        const float shaped_value = std::pow(value, 0.82f);
        const float min_height = rect.height * 0.012f;
        const float height = min_height + (max_height - min_height) * shaped_value;
        const float x = rect.x + static_cast<float>(i) * (bar_width + gap);
        const Rectangle bar_rect = {x, baseline - height, bar_width, height};
        DrawRectangleRec(bar_rect, kSpectrumBarColor);
    }
}
