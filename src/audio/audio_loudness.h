#pragma once

#include <string>

struct audio_loudness_analysis {
    bool valid = false;
    float integrated_lufs = 0.0f;
    float peak = 0.0f;
    float linear_gain = 1.0f;
};

audio_loudness_analysis analyze_audio_loudness(const std::string& file_path);

