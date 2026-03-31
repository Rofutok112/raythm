#pragma once

#include <optional>
#include <string>
#include <vector>

#include "raylib.h"
#include "editor_state.h"
#include "scene.h"
#include "song_loader.h"

class editor_scene final : public scene {
public:
    editor_scene(scene_manager& manager, song_data song, std::string chart_path);
    editor_scene(scene_manager& manager, song_data song, int key_count);

    void on_enter() override;
    void update(float dt) override;
    void draw() override;

private:
    struct grid_line {
        int tick = 0;
        bool major = false;
        int measure = 1;
        int beat = 1;
    };

    struct meter_segment {
        int start_tick = 0;
        int numerator = 4;
        int denominator = 4;
        int beat_index_offset = 0;
        int measure_index_offset = 0;
    };

    struct note_draw_info {
        Rectangle head_rect = {};
        Rectangle body_rect = {};
        Rectangle tail_rect = {};
        bool has_body = false;
    };

    chart_data make_new_chart_data() const;
    void rebuild_meter_segments();
    std::vector<grid_line> visible_grid_lines(int min_tick, int max_tick) const;
    Rectangle timeline_content_rect() const;
    Rectangle timeline_scrollbar_track_rect() const;
    float visible_tick_span() const;
    float content_tick_span() const;
    float content_height_pixels() const;
    float scroll_offset_pixels() const;
    float max_bottom_tick() const;
    float tick_to_timeline_y(int tick) const;
    int timeline_y_to_tick(float y) const;
    float lane_width() const;
    Rectangle lane_rect(int lane) const;
    double beat_number_at_tick(int tick) const;
    std::string bar_beat_label(int tick) const;
    int snap_division() const;
    int snap_interval() const;
    int snap_tick(int raw_tick) const;
    std::optional<int> lane_at_position(Vector2 point) const;
    std::optional<size_t> note_at_position(Vector2 point) const;
    note_draw_info note_rects(const note_data& note) const;
    void handle_shortcuts();
    void handle_timeline_interaction();
    void apply_scroll_and_zoom(float dt);
    void draw_left_panel();
    void draw_right_panel() const;
    void draw_timeline() const;
    void draw_timeline_grid(int min_tick, int max_tick) const;
    void draw_timeline_notes() const;
    void draw_cursor_hud() const;
    void draw_header_tools();

    song_data song_;
    std::optional<std::string> chart_path_;
    int new_chart_key_count_ = 4;
    editor_state state_;
    std::vector<meter_segment> meter_segments_;
    std::vector<std::string> load_errors_;
    int audio_length_tick_ = 0;
    float bottom_tick_ = 0.0f;
    float bottom_tick_target_ = 0.0f;
    float ticks_per_pixel_ = 2.0f;
    int snap_index_ = 4;
    bool snap_dropdown_open_ = false;
    std::optional<size_t> selected_note_index_;
    bool note_dragging_ = false;
    int drag_lane_ = 0;
    int drag_start_tick_ = 0;
    int drag_current_tick_ = 0;
    bool scrollbar_dragging_ = false;
    float scrollbar_drag_offset_ = 0.0f;
};
