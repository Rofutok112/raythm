#pragma once

#include <optional>
#include <vector>

#include "audio_waveform.h"
#include "editor_meter_map.h"
#include "editor_timeline_note_geometry.h"
#include "raylib.h"
#include "timing_engine.h"

enum class editor_timeline_note_type {
    tap,
    hold,
    release,
    stay,
    decorative_hold
};

struct editor_timeline_note {
    editor_timeline_note_type type = editor_timeline_note_type::tap;
    int tick = 0;
    int lane = 0;
    int end_tick = 0;
    bool is_ray = false;
    int lane_width = 1;
    size_t source_index = 0;
};

struct editor_timeline_scroll_automation_point {
    int tick = 0;
    float multiplier = 1.0f;
    scroll_automation_curve curve_to_next = scroll_automation_curve::hold;
};

struct editor_timeline_metrics {
    Rectangle panel_rect = {};
    float padding = 18.0f;
    float scrollbar_gap = 10.0f;
    float scrollbar_width = 10.0f;
    float lane_gap = 6.0f;
    float right_reserved_width = 0.0f;
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
    editor_timeline_note_geometry note_rects(const editor_timeline_note& note) const;
};

struct editor_timeline_view_model {
    editor_timeline_metrics metrics;
    std::vector<editor_meter_map::grid_line> grid_lines;
    std::vector<editor_timeline_scroll_automation_point> scroll_automation;
    std::vector<editor_timeline_note> notes;
    const std::vector<editor_timeline_note>* minimap_notes = nullptr;
    size_t minimap_generation = 0;
    std::vector<size_t> selected_note_indices;
    std::optional<size_t> selected_scroll_event_index;
    std::optional<int> playback_tick;
    const audio_waveform_summary* waveform_summary = nullptr;
    const timing_engine* timing_engine = nullptr;
    bool waveform_visible = false;
    int waveform_offset_ms = 0;
    std::vector<editor_timeline_note> preview_notes;
    std::vector<size_t> preview_note_indices;
    bool preview_has_overlap = false;
    std::optional<Rectangle> selection_rect;
    int min_tick = 0;
    int max_tick = 0;
    int snap_interval = 1;
    float content_height_pixels = 0.0f;
    float scroll_offset_pixels = 0.0f;
};
