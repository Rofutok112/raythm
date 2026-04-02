#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "raylib.h"
#include "audio_waveform.h"
#include "editor_meter_map.h"
#include "editor_state.h"
#include "editor_timeline_view.h"
#include "editor_timing_panel.h"
#include "scene.h"
#include "song_loader.h"
#include "ui_text_input.h"

class editor_scene final : public scene {
public:
    struct resume_state {
        std::shared_ptr<editor_state> state;
        int playback_tick = 0;
        float bottom_tick = 0.0f;
        float bottom_tick_target = 0.0f;
        float ticks_per_pixel = 2.0f;
        int snap_index = 4;
        bool waveform_visible = true;
        std::optional<size_t> selected_note_index;
    };

    editor_scene(scene_manager& manager, song_data song, std::string chart_path);
    editor_scene(scene_manager& manager, song_data song, int key_count);
    editor_scene(scene_manager& manager, song_data song, chart_meta initial_meta);
    editor_scene(scene_manager& manager, song_data song, resume_state resume);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    enum class pending_action {
        none,
        exit_to_song_select,
    };

    struct timeline_note_drag_state {
        bool active = false;
        int lane = 0;
        int start_tick = 0;
        int current_tick = 0;
    };

    struct metadata_panel_state {
        ui::text_input_state difficulty_input;
        ui::text_input_state chart_author_input;
        ui::text_input_state chart_name_input;
        ui::text_input_state description_input;
        int key_count = 4;
        std::string error;
        bool key_count_confirm_open = false;
        int pending_key_count = 4;
    };

    struct save_dialog_state {
        bool open = false;
        ui::text_input_state file_name_input;
        std::string error;
        bool submit_requested = false;
        pending_action action_after_save = pending_action::none;
    };

    struct unsaved_changes_dialog_state {
        bool open = false;
        pending_action pending = pending_action::none;
    };

    chart_data make_new_chart_data() const;
    chart_data make_chart_data_for_save() const;
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
    void update_audio_clock();
    void refresh_audio_length_tick();
    void update_note_hitsounds();
    void rebuild_waveform_data(const std::string& audio_path);
    void rebuild_waveform_samples();
    void toggle_audio_playback();
    void seek_audio_to_tick(int tick, bool scroll_into_view);
    std::string playback_status_text() const;
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
    void scroll_timing_list_to_bottom();
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
    void sync_metadata_inputs();
    bool has_active_metadata_input() const;
    bool apply_metadata_changes(bool clear_notes_for_key_count_change);
    bool apply_chart_offset(int offset_ms);
    std::string generated_chart_id(const std::string& difficulty) const;
    std::string suggested_chart_file_name() const;
    void open_save_dialog(pending_action action_after_save = pending_action::none);
    void close_save_dialog();
    bool save_to_path(const std::string& file_path);
    bool save_chart();
    bool save_chart_from_dialog();
    void request_action(pending_action action);
    void perform_action(pending_action action);
    bool has_blocking_modal() const;
    void close_key_count_confirmation();
    void update_key_count_confirmation();
    void update_unsaved_changes_dialog();
    void draw_unsaved_changes_dialog() const;
    void draw_save_dialog();
    void draw_key_count_confirmation() const;
    resume_state build_resume_state() const;
    void start_playtest(int start_tick);

    song_data song_;
    std::optional<std::string> chart_path_;
    std::optional<chart_meta> initial_meta_;
    int new_chart_key_count_ = 4;
    std::shared_ptr<editor_state> state_;
    std::optional<resume_state> resume_state_;
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
    timeline_note_drag_state timeline_drag_;
    metadata_panel_state metadata_panel_;
    save_dialog_state save_dialog_;
    unsaved_changes_dialog_state unsaved_changes_dialog_;
    bool scrollbar_dragging_ = false;
    float scrollbar_drag_offset_ = 0.0f;
};
