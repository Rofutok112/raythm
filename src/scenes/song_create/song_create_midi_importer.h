#pragma once

#include <string>
#include <vector>

#include "data_models.h"

namespace song_create {

struct midi_timing_import {
    bool ok = false;
    int resolution = 480;
    float base_bpm = 120.0f;
    std::vector<timing_event> events;
    std::string message;
};

midi_timing_import import_midi_timing_file(const std::string& path);

int normalized_midi_tick(int source_tick, int source_resolution);

}  // namespace song_create
