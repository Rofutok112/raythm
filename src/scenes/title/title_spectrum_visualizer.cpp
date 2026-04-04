#include "title/title_spectrum_visualizer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "audio_manager.h"
#include "theme.h"

namespace {

constexpr std::array<int, title_spectrum_visualizer::kBarCount + 1> kSpectrumRanges = {
    1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 17, 20,
    24, 29, 35, 42, 50, 60, 72, 86, 102, 114, 122, 128
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
}

void title_spectrum_visualizer::update() {
    std::array<float, 128> spectrum = {};
    const bool has_audio = audio_manager::instance().get_bgm_fft256(spectrum);

    for (int i = 0; i < kBarCount; ++i) {
        float target = 0.0f;
        if (has_audio) {
            const float band = average_band_energy(spectrum, kSpectrumRanges[static_cast<size_t>(i)],
                                                   kSpectrumRanges[static_cast<size_t>(i + 1)]);
            target = std::clamp(std::sqrt(std::max(0.0f, band)) * 7.5f, 0.0f, 1.0f);
        }

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
    const float gap = 8.0f;
    const float bar_width =
        (rect.width - gap * static_cast<float>(kBarCount - 1)) / static_cast<float>(kBarCount);
    const float baseline = rect.y + rect.height;

    for (int i = 0; i < kBarCount; ++i) {
        const float value = std::clamp(bars_[static_cast<size_t>(i)], 0.0f, 1.0f);
        const float height = std::max(4.0f, rect.height * value);
        const float x = rect.x + static_cast<float>(i) * (bar_width + gap);
        const Rectangle bar_rect = {x, baseline - height, bar_width, height};
        const float glow = 0.18f + value * 0.42f;
        DrawRectangleRounded(bar_rect, 0.35f, 8, with_alpha_scale(t.accent, glow));
        DrawRectangleRoundedLinesEx(bar_rect, 0.35f, 8, 1.0f, with_alpha_scale(t.text, 0.12f + value * 0.25f));
    }
}
