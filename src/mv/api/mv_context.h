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

} // namespace mv
