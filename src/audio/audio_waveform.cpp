#include "audio_waveform.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "bass.h"

audio_waveform_summary audio_waveform::build(const std::string& file_path, std::size_t segment_count) {
    audio_waveform_summary summary;
    if (file_path.empty()) {
        return summary;
    }

    const std::size_t clamped_segment_count = std::clamp<std::size_t>(segment_count, 1024, 24576);
    const unsigned long stream = BASS_StreamCreateFile(
        FALSE, file_path.c_str(), 0, 0, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);
    if (stream == 0) {
        return summary;
    }

    const QWORD length_bytes = BASS_ChannelGetLength(stream, BASS_POS_BYTE);
    if (length_bytes == static_cast<QWORD>(-1)) {
        BASS_StreamFree(stream);
        return summary;
    }

    BASS_CHANNELINFO info = {};
    if (BASS_ChannelGetInfo(stream, &info) == FALSE || info.chans <= 0) {
        BASS_StreamFree(stream);
        return summary;
    }

    const std::size_t channels = static_cast<std::size_t>(info.chans);
    const std::size_t total_frames = std::max<std::size_t>(1, length_bytes / (sizeof(float) * channels));
    summary.length_seconds = BASS_ChannelBytes2Seconds(stream, length_bytes);
    summary.peaks.assign(clamped_segment_count, {});

    std::vector<float> peak_values(clamped_segment_count, 0.0f);
    std::vector<float> buffer(4096);
    std::size_t frame_cursor = 0;

    while (true) {
        const DWORD bytes_read = BASS_ChannelGetData(
            stream, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(float)));
        if (bytes_read == static_cast<DWORD>(-1) || bytes_read == 0) {
            break;
        }

        const std::size_t samples_read = bytes_read / sizeof(float);
        const std::size_t frames_read = samples_read / channels;
        for (std::size_t frame_index = 0; frame_index < frames_read; ++frame_index) {
            float amplitude = 0.0f;
            const std::size_t sample_base = frame_index * channels;
            for (std::size_t channel = 0; channel < channels; ++channel) {
                amplitude = std::max(amplitude, std::fabs(buffer[sample_base + channel]));
            }

            const std::size_t absolute_frame = frame_cursor + frame_index;
            const std::size_t segment_index = std::min(
                clamped_segment_count - 1,
                absolute_frame * clamped_segment_count / total_frames);
            peak_values[segment_index] = std::max(peak_values[segment_index], amplitude);
        }

        frame_cursor += frames_read;
    }

    BASS_StreamFree(stream);

    const float max_peak = *std::max_element(peak_values.begin(), peak_values.end());
    for (std::size_t index = 0; index < clamped_segment_count; ++index) {
        const double segment_ratio = (static_cast<double>(index) + 0.5) / static_cast<double>(clamped_segment_count);
        summary.peaks[index] = {
            summary.length_seconds * segment_ratio,
            max_peak > 0.0f ? std::clamp(peak_values[index] / max_peak, 0.0f, 1.0f) : 0.0f
        };
    }

    return summary;
}
