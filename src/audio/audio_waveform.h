#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct audio_waveform_peak {
    double seconds = 0.0;
    float amplitude = 0.0f;
};

struct audio_waveform_summary {
    std::vector<audio_waveform_peak> peaks;
    double length_seconds = 0.0;
};

class audio_waveform final {
public:
    static audio_waveform_summary build(const std::string& file_path, std::size_t segment_count = 14336);
};
