#pragma once

#include <optional>

#include "editor/editor_meter_map.h"
#include "editor/editor_state.h"
#include "editor/editor_timeline_view.h"
#include "editor/viewport/editor_timeline_viewport.h"

struct editor_timeline_presenter_model {
    const editor_state& state;
    const editor_meter_map& meter_map;
    const std::vector<editor_timeline_waveform_sample>* waveform_samples = nullptr;
    bool waveform_visible = false;
    bool audio_loaded = false;
    int playback_tick = 0;
    std::optional<size_t> selected_note_index;
    std::optional<note_data> preview_note;
    bool preview_has_overlap = false;
    editor_timeline_viewport_model viewport;
};

class editor_timeline_presenter final {
public:
    static void draw(const editor_timeline_presenter_model& model);
};
