#pragma once

#include <optional>
#include <string>
#include <vector>

#include "raylib.h"
#include "editor_meter_map.h"
#include "editor_state.h"
#include "editor_timeline_view.h"
#include "editor_timing_panel.h"
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
    struct timeline_note_drag_state {
        bool active = false;
        int lane = 0;
        int start_tick = 0;
        int current_tick = 0;
    };

    chart_data make_new_chart_data() const;
    std::optional<note_data> dragged_note() const;
    std::vector<size_t> sorted_timing_event_indices() const;
    editor_timeline_metrics timeline_metrics() const;
    float visible_tick_span() const;
    float content_tick_span() const;
    float content_height_pixels() const;
    float scroll_offset_pixels() const;
    float max_bottom_tick() const;
    int snap_division() const;
    int snap_interval() const;
    int snap_tick(int raw_tick) const;
    int default_timing_event_tick() const;
    std::optional<int> lane_at_position(Vector2 point) const;
    std::optional<size_t> note_at_position(Vector2 point) const;
    void rebuild_hit_regions() const;
    void handle_shortcuts();
    void handle_text_input();
    void handle_timeline_interaction();
    void apply_scroll_and_zoom(float dt);
    void select_timing_event(std::optional<size_t> index, bool scroll_into_view);
    void scroll_to_tick(int tick);
    void sync_timing_event_selection();
    bool apply_selected_timing_event();
    void add_timing_event(timing_event_type type);
    void delete_selected_timing_event();
    bool can_delete_selected_timing_event() const;
    void load_timing_event_inputs();
    void clear_timing_event_inputs();
    void draw_left_panel();
    void draw_right_panel();
    void draw_timeline() const;
    void draw_cursor_hud() const;
    void draw_header_tools();

    song_data song_;
    std::optional<std::string> chart_path_;
    int new_chart_key_count_ = 4;
    editor_state state_;
    editor_meter_map meter_map_;
    editor_timing_panel_state timing_panel_;
    std::vector<std::string> load_errors_;
    int audio_length_tick_ = 0;
    float bottom_tick_ = 0.0f;
    float bottom_tick_target_ = 0.0f;
    float ticks_per_pixel_ = 2.0f;
    int snap_index_ = 4;
    bool snap_dropdown_open_ = false;
    std::optional<size_t> selected_note_index_;
    timeline_note_drag_state timeline_drag_;
    bool scrollbar_dragging_ = false;
    float scrollbar_drag_offset_ = 0.0f;
};
