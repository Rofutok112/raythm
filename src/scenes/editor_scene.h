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
#include "editor/viewport/editor_timeline_viewport.h"
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
    chart_data make_chart_data_for_save() const;
    editor_resume_state build_resume_state() const;
    editor_scene_sync_context make_sync_context();
    editor_timeline_viewport_model viewport_model() const;
    std::optional<note_data> dragged_note() const;
    std::vector<size_t> sorted_timing_event_indices() const;
    editor_timeline_metrics timeline_metrics() const;
    int default_timing_event_tick() const;
    void apply_flow_result(const editor_flow_result& result);
    void rebuild_hit_regions() const;
    void apply_scroll_and_zoom(float dt);
    void select_timing_event(std::optional<size_t> index, bool scroll_into_view);
    void scroll_to_tick(int tick);
    bool apply_selected_timing_event();
    void add_timing_event(timing_event_type type);
    void delete_selected_timing_event();
    bool can_delete_selected_timing_event() const;
    void draw_timeline() const;
    bool has_active_metadata_input() const;
    bool apply_metadata_changes(bool clear_notes_for_key_count_change);
    bool apply_chart_offset(int offset_ms);
    std::string generated_chart_id(const std::string& difficulty) const;
    bool has_blocking_modal() const;

    song_data song_;
    std::optional<std::string> chart_path_;
    std::optional<chart_meta> initial_meta_;
    int new_chart_key_count_ = 4;
    std::shared_ptr<editor_state> state_;
    std::optional<editor_resume_state> resume_state_;
    editor_meter_map meter_map_;
    editor_timing_panel_state timing_panel_;
    std::vector<std::string> load_errors_;
    editor_transport_state transport_;
    std::optional<int> space_playback_start_tick_;
    std::string hitsound_path_;
    bool waveform_visible_ = true;
    int waveform_offset_ms_ = 0;
    audio_waveform_summary waveform_summary_;
    std::vector<editor_timeline_waveform_sample> waveform_samples_;
    editor_timeline_viewport_state viewport_;
    bool snap_dropdown_open_ = false;
    std::optional<size_t> selected_note_index_;
    editor_timeline_note_drag_state timeline_drag_;
    metadata_panel_state metadata_panel_;
    save_dialog_state save_dialog_;
    unsaved_changes_dialog_state unsaved_changes_dialog_;
};
