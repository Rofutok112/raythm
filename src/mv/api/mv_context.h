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

    // audio
    std::vector<float> spectrum; // normalized 0..1, arbitrary bin count
    const std::vector<float>* waveform = nullptr; // normalized 0..1, precomputed song-wide envelope
    const std::vector<float>* oscilloscope = nullptr; // live PCM-ish mono samples, normalized -1..1
    float level = 0.0f;      // current song amplitude sampled from waveform/envelope
    int waveform_index = 0;  // current position inside waveform/envelope

    // chart
    int total_notes = 0;
    int combo = 0;
    float accuracy = 0;       // 0..1
    int key_count = 4;

    // screen
    float screen_w = 1280;
    float screen_h = 720;
};

// Build the ctx mv_object from context_input.
// The returned object has sub-objects: ctx.time, ctx.audio, ctx.chart, ctx.screen
std::shared_ptr<mv_object> build_context(const context_input& input);

class context_builder {
public:
    context_builder();

    std::shared_ptr<mv_object> build(const context_input& input);

private:
    std::shared_ptr<mv_object> ctx_;
    std::shared_ptr<mv_object> time_;
    std::shared_ptr<mv_object> audio_;
    std::shared_ptr<mv_object> chart_;
    std::shared_ptr<mv_object> screen_;
    std::shared_ptr<mv_list> spectrum_;
    std::shared_ptr<mv_list> waveform_;
    std::shared_ptr<mv_list> oscilloscope_;
    const std::vector<float>* waveform_source_ = nullptr;
};

} // namespace mv
