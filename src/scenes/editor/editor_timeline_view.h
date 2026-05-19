#pragma once

#include <optional>
#include <vector>

#include "audio_waveform.h"
#include "editor_meter_map.h"
#include "timing_engine.h"
#include "raylib.h"

enum class editor_timeline_note_type {
    tap,
    hold,
    release,
    stay
};

struct editor_timeline_note {
    editor_timeline_note_type type = editor_timeline_note_type::tap;
    int tick = 0;
    int lane = 0;
    int end_tick = 0;
    bool is_ray = false;
    int lane_width = 1;
};

struct editor_timeline_scroll_event {
    scroll_event_type type = scroll_event_type::speed;
    int tick = 0;
    int duration = 0;
    float multiplier = 1.0f;
};

struct editor_timeline_note_draw_info {
    Rectangle head_rect = {};
    Rectangle body_rect = {};
    Rectangle tail_rect = {};
    Rectangle left_resize_rect = {};
    Rectangle right_resize_rect = {};
    Rectangle end_resize_rect = {};
    bool has_body = false;
};

struct editor_timeline_metrics {
    Rectangle panel_rect = {};
    float padding = 18.0f;
    float scrollbar_gap = 10.0f;
    float scrollbar_width = 10.0f;
    float lane_gap = 6.0f;
    float note_head_height = 14.0f;
    float bottom_tick = 0.0f;
    float ticks_per_pixel = 2.0f;
    int key_count = 4;

    Rectangle content_rect() const;
    Rectangle scrollbar_track_rect() const;
    float visible_tick_span() const;
    float tick_to_y(int tick) const;
    int y_to_tick(float y) const;
    float lane_width() const;
    Rectangle lane_rect(int lane) const;
    editor_timeline_note_draw_info note_rects(const editor_timeline_note& note) const;
};

struct editor_timeline_view_model {
    editor_timeline_metrics metrics;
    std::vector<editor_meter_map::grid_line> grid_lines;
    std::vector<editor_timeline_scroll_event> scroll_events;
    std::vector<editor_timeline_note> notes;
    std::optional<size_t> selected_note_index;
    std::vector<size_t> selected_note_indices;
    std::optional<size_t> selected_scroll_event_index;
    std::optional<int> playback_tick;
    bool loop_enabled = false;
    int loop_start_tick = 0;
    int loop_end_tick = 0;
    const audio_waveform_summary* waveform_summary = nullptr;
    const timing_engine* timing_engine = nullptr;
    bool waveform_visible = false;
    int waveform_offset_ms = 0;
    std::optional<editor_timeline_note> preview_note;
    bool preview_has_overlap = false;
    int min_tick = 0;
    int max_tick = 0;
    int snap_interval = 1;
    float content_height_pixels = 0.0f;
    float scroll_offset_pixels = 0.0f;
};

class editor_timeline_view {
public:
    static void draw(const editor_timeline_view_model& model);
};
