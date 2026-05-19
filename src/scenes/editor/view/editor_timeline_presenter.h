#pragma once

#include <optional>
#include <vector>

#include "audio_waveform.h"
#include "editor/editor_meter_map.h"
#include "editor/editor_state.h"
#include "editor/editor_timeline_view.h"
#include "editor/viewport/editor_timeline_viewport.h"

struct editor_timeline_presenter_model {
    const editor_state& state;
    const editor_meter_map& meter_map;
    const audio_waveform_summary* waveform_summary = nullptr;
    bool waveform_visible = false;
    int waveform_offset_ms = 0;
    bool audio_loaded = false;
    int playback_tick = 0;
    bool loop_enabled = false;
    int loop_start_tick = 0;
    int loop_end_tick = 0;
    std::optional<size_t> selected_note_index;
    std::vector<size_t> selected_note_indices;
    std::optional<size_t> selected_scroll_event_index;
    std::optional<note_data> preview_note;
    std::optional<size_t> preview_note_index;
    bool preview_has_overlap = false;
    editor_timeline_viewport_model viewport;
};

class editor_timeline_presenter final {
public:
    static void draw(const editor_timeline_presenter_model& model);
};
