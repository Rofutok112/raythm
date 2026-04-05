#pragma once

#include <optional>
#include <span>

#include "editor/editor_state.h"
#include "editor/editor_timeline_view.h"

struct editor_timeline_viewport_state {
    float bottom_tick = 0.0f;
    float bottom_tick_target = 0.0f;
    float ticks_per_pixel = 2.0f;
    int snap_index = 4;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
};

struct editor_timeline_viewport_model {
    const editor_state* state = nullptr;
    int audio_length_tick = 0;
    editor_timeline_viewport_state viewport;
};

struct editor_timeline_viewport_scroll_input {
    Vector2 mouse = {};
    float wheel = 0.0f;
    bool ctrl_down = false;
    bool audio_playing = false;
    int playback_tick = 0;
    float dt = 0.0f;
};

class editor_timeline_viewport final {
public:
    static editor_timeline_metrics metrics(const editor_timeline_viewport_model& model);
    static float visible_tick_span(const editor_timeline_viewport_model& model);
    static float content_tick_span(const editor_timeline_viewport_model& model);
    static float content_height_pixels(const editor_timeline_viewport_model& model);
    static float scroll_offset_pixels(const editor_timeline_viewport_model& model);
    static float min_bottom_tick();
    static float max_bottom_tick(const editor_timeline_viewport_model& model);
    static int snap_division(const editor_timeline_viewport_state& viewport);
    static int snap_interval(const editor_timeline_viewport_model& model);
    static int snap_tick(const editor_timeline_viewport_model& model, int raw_tick);
    static int default_timing_event_tick(const editor_timeline_viewport_model& model,
                                         std::optional<size_t> selected_timing_event_index);
    static editor_timeline_viewport_state apply_scroll_and_zoom(const editor_timeline_viewport_model& model,
                                                                const editor_timeline_viewport_scroll_input& input);
    static editor_timeline_viewport_state scroll_to_tick(const editor_timeline_viewport_model& model, int tick);
    static std::span<const char* const> snap_labels();
};
