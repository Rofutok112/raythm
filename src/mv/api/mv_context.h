#pragma once

#include "../lang/mv_vm.h"

#include <vector>

namespace mv {

// Data fed into context builder each frame.
// Decoupled from game types so mv/ stays independent.
struct context_input {
    // time
    double current_ms = 0;
    double song_length_ms = 0;
    float bpm = 120.0f;
    int beat_number = 0;      // absolute beat count from song start
    float beat_phase = 0;     // 0..1 fraction within current beat
    int meter_numerator = 4;
    int meter_denominator = 4;

    // audio
    std::vector<float> spectrum; // normalized 0..1, arbitrary bin count
    const std::vector<float>* waveform = nullptr; // normalized 0..1, precomputed song-wide envelope
    const std::vector<float>* oscilloscope = nullptr; // live PCM-ish mono samples, normalized -1..1
    float level = 0.0f;      // current song amplitude sampled from waveform/envelope
    float rms = 0.0f;
    float peak = 0.0f;
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;
    int waveform_index = 0;  // current position inside waveform/envelope

    // song metadata
    std::string song_id;
    std::string song_title;
    std::string song_artist;
    float song_base_bpm = 0.0f;

    // chart
    std::string chart_id;
    std::string chart_song_id;
    std::string chart_difficulty;
    float chart_level = 0.0f;
    std::string chart_author;
    int chart_resolution = 0;
    int chart_offset = 0;
    int total_notes = 0;
    int combo = 0;
    float accuracy = 0;       // 0..1
    int key_count = 4;

    // screen
    float screen_w = 1920;
    float screen_h = 1080;
};

// Build the ctx mv_object from context_input.
// The returned object has sub-objects: ctx.time, ctx.audio, ctx.song, ctx.chart, ctx.screen
std::shared_ptr<mv_object> build_context(const context_input& input);

class context_builder {
public:
    context_builder();

    std::shared_ptr<mv_object> build(const context_input& input);

private:
    std::shared_ptr<mv_object> ctx_;
    std::shared_ptr<mv_object> time_;
    std::shared_ptr<mv_object> audio_;
    std::shared_ptr<mv_object> audio_analysis_;
    std::shared_ptr<mv_object> audio_bands_;
    std::shared_ptr<mv_object> audio_buffers_;
    std::shared_ptr<mv_object> song_;
    std::shared_ptr<mv_object> chart_;
    std::shared_ptr<mv_object> screen_;
    std::shared_ptr<mv_list> spectrum_;
    std::shared_ptr<mv_list> waveform_;
    std::shared_ptr<mv_list> oscilloscope_;
    const std::vector<float>* waveform_source_ = nullptr;
};

} // namespace mv
