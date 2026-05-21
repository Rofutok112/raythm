#include "audio_loudness.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "bass.h"

namespace {
constexpr float kTargetLufs = -14.0f;
constexpr float kPeakHeadroom = 0.98f;
constexpr float kMaxBoostDb = 12.0f;
constexpr double kSilencePower = 1.0e-12;
constexpr DWORD kDecodeFlags = BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT | BASS_STREAM_PRESCAN;

unsigned long create_decode_stream(const std::string& file_path) {
    return BASS_StreamCreateFile(FALSE, file_path.c_str(), 0, 0, kDecodeFlags);
}
}  // namespace

audio_loudness_analysis analyze_audio_loudness(const std::string& file_path) {
    audio_loudness_analysis analysis;

    const unsigned long stream = create_decode_stream(file_path);
    if (stream == 0) {
        return analysis;
    }

    BASS_CHANNELINFO info = {};
    if (BASS_ChannelGetInfo(stream, &info) == FALSE || info.chans == 0) {
        BASS_StreamFree(stream);
        return analysis;
    }

    const std::size_t channels = static_cast<std::size_t>(info.chans);
    constexpr std::size_t kFramesPerRead = 8192;
    std::vector<float> samples(kFramesPerRead * channels, 0.0f);
    double sum_squares = 0.0;
    std::uint64_t sample_count = 0;
    float peak = 0.0f;

    for (;;) {
        const DWORD requested_bytes = static_cast<DWORD>(samples.size() * sizeof(float));
        const DWORD bytes_read = BASS_ChannelGetData(stream, samples.data(), requested_bytes);
        if (bytes_read == static_cast<DWORD>(-1) || bytes_read == 0) {
            break;
        }

        const std::size_t count = bytes_read / sizeof(float);
        for (std::size_t i = 0; i < count; ++i) {
            const float sample = samples[i];
            peak = std::max(peak, std::abs(sample));
            sum_squares += static_cast<double>(sample) * static_cast<double>(sample);
        }
        sample_count += static_cast<std::uint64_t>(count);
    }

    BASS_StreamFree(stream);

    if (sample_count == 0 || sum_squares <= kSilencePower) {
        return analysis;
    }

    const double mean_square = sum_squares / static_cast<double>(sample_count);
    const float loudness_lufs = static_cast<float>(-0.691 + 10.0 * std::log10(mean_square));
    const float desired_gain_db = std::clamp(kTargetLufs - loudness_lufs, -60.0f, kMaxBoostDb);
    float gain = std::pow(10.0f, desired_gain_db / 20.0f);
    if (peak > 0.0f) {
        gain = std::min(gain, kPeakHeadroom / peak);
    }

    analysis.valid = true;
    analysis.integrated_lufs = loudness_lufs;
    analysis.peak = peak;
    analysis.linear_gain = std::clamp(gain, 0.0f, 4.0f);
    return analysis;
}

