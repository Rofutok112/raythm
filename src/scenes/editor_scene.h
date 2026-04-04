#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "audio_waveform.h"
#include "editor/editor_meter_map.h"
#include "editor/editor_state.h"
#include "editor/editor_timeline_view.h"
#include "editor/editor_timing_panel.h"
#include "editor/editor_panel_controller.h"
#include "editor/editor_scene_sync.h"
#include "editor/editor_scene_types.h"
#include "editor/editor_timeline_controller.h"
#include "editor/editor_transport_controller.h"
#include "raylib.h"
#include "scene.h"
#include "song_loader.h"

class editor_scene final : public scene {
public:
    editor_scene(scene_manager& manager, song_data song, std::string chart_path);
    editor_scene(scene_manager& manager, song_data song, int key_count);
    editor_scene(scene_manager& manager, song_data song, chart_meta initial_meta);
    editor_scene(scene_manager& manager, song_data song, editor_resume_state resume);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    int computed_chart_level() const;
    chart_data make_chart_data_for_save() const;
    editor_resume_state build_resume_state() const;
    editor_scene_sync_context make_sync_context();
    std::optional<note_data> dragged_note() const;
    std::vector<size_t> sorted_timing_event_indices() const;
    editor_timeline_metrics timeline_metrics() const;
    float visible_tick_span() const;
    float content_tick_span() const;
    float content_height_pixels() const;
    float scroll_offset_pixels() const;
    float min_bottom_tick() const;
    float max_bottom_tick() const;
    int snap_division() const;
    int snap_interval() const;
    int snap_tick(int raw_tick) const;
    int default_timing_event_tick() const;
    void apply_flow_result(const editor_flow_result& result);
    void apply_transport_result(const editor_transport_result& result);
    void sync_transport_state(bool suppress_hitsounds = false);
    void seek_audio_to_tick(int tick);
    std::string playback_status_text() const;
    void rebuild_hit_regions() const;
    void handle_shortcuts();
    void handle_text_input();
    void handle_timeline_interaction();
    void apply_scroll_and_zoom(float dt);
    void select_timing_event(std::optional<size_t> index, bool scroll_into_view);
    void scroll_to_tick(int tick);
    bool apply_selected_timing_event();
    void add_timing_event(timing_event_type type);
    void delete_selected_timing_event();
    bool can_delete_selected_timing_event() const;
    void draw_left_panel();
    void draw_right_panel();
    void draw_timeline() const;
    void draw_cursor_hud() const;
    void draw_header_tools();
    bool has_active_metadata_input() const;
    bool apply_metadata_changes(bool clear_notes_for_key_count_change);
    bool apply_chart_offset(int offset_ms);
    std::string generated_chart_id(const std::string& difficulty) const;
    bool has_blocking_modal() const;
    void draw_unsaved_changes_dialog() const;
    void draw_save_dialog();
    void draw_key_count_confirmation() const;

    song_data song_;
    std::optional<std::string> chart_path_;
    std::optional<chart_meta> initial_meta_;
    int new_chart_key_count_ = 4;
    std::shared_ptr<editor_state> state_;
    std::optional<editor_resume_state> resume_state_;
    editor_meter_map meter_map_;
    editor_timing_panel_state timing_panel_;
    std::vector<std::string> load_errors_;
    int audio_length_tick_ = 0;
    bool audio_loaded_ = false;
    bool audio_playing_ = false;
    double audio_time_seconds_ = 0.0;
    int playback_tick_ = 0;
    int previous_playback_tick_ = 0;
    bool previous_audio_playing_ = false;
    std::optional<int> space_playback_start_tick_;
    std::string hitsound_path_;
    bool waveform_visible_ = true;
    int waveform_offset_ms_ = 0;
    audio_waveform_summary waveform_summary_;
    std::vector<editor_timeline_waveform_sample> waveform_samples_;
    float bottom_tick_ = 0.0f;
    float bottom_tick_target_ = 0.0f;
    float ticks_per_pixel_ = 2.0f;
    int snap_index_ = 4;
    bool snap_dropdown_open_ = false;
    std::optional<size_t> selected_note_index_;
    editor_timeline_note_drag_state timeline_drag_;
    metadata_panel_state metadata_panel_;
    save_dialog_state save_dialog_;
    unsaved_changes_dialog_state unsaved_changes_dialog_;
    bool scrollbar_dragging_ = false;
    float scrollbar_drag_offset_ = 0.0f;
};
