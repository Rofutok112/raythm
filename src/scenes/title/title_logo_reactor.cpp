#include "title/title_logo_reactor.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "audio_manager.h"

namespace {

float average_low_band_energy(const std::array<float, 128>& spectrum) {
    float sum = 0.0f;
    constexpr int kLowBandEnd = 8;
    for (int i = 1; i < kLowBandEnd; ++i) {
        sum += spectrum[static_cast<size_t>(i)];
    }
    return sum / static_cast<float>(kLowBandEnd - 1);
}

}  // namespace

void title_logo_reactor::reset() {
    pulse_ = 0.0f;
}

void title_logo_reactor::update(float dt) {
    (void)dt;

    std::array<float, 128> spectrum = {};
    float target = 0.0f;
    if (audio_manager::instance().get_bgm_fft256(spectrum)) {
        const float band_energy = average_low_band_energy(spectrum);
        target = std::clamp(std::sqrt(std::max(0.0f, band_energy)) * 9.0f, 0.0f, 1.0f);
    }

    if (target > pulse_) {
        pulse_ += (target - pulse_) * 0.32f;
    } else {
        pulse_ += (target - pulse_) * 0.10f;
    }
}

int title_logo_reactor::transform_font_size(int base_font_size) const {
    const float scale = 1.0f + pulse_ * 0.10f;
    return std::max(1, static_cast<int>(std::round(static_cast<float>(base_font_size) * scale)));
}

Rectangle title_logo_reactor::transform_rect(const Rectangle& base_rect) const {
    const float width_expand = pulse_ * 22.0f;
    const float height_expand = pulse_ * 10.0f;

    return {
        base_rect.x - width_expand * 0.5f,
        base_rect.y - height_expand * 0.5f,
        base_rect.width + width_expand,
        base_rect.height + height_expand,
    };
}
