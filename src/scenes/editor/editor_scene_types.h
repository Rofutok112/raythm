#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "audio_manager.h"
#include "audio_waveform.h"
#include "editor_meter_map.h"
#include "editor_state.h"
#include "editor_timeline_view.h"
#include "editor_timing_panel.h"
#include "song_loader.h"
#include "ui_text_input.h"

struct editor_resume_state {
    std::shared_ptr<editor_state> state;
    int playback_tick = 0;
    float bottom_tick = 0.0f;
    float bottom_tick_target = 0.0f;
    float ticks_per_pixel = 2.0f;
    int snap_index = 4;
    bool waveform_visible = true;
    std::optional<size_t> selected_note_index;
};

struct editor_timeline_note_drag_state {
    bool active = false;
    int lane = 0;
    int start_tick = 0;
    int current_tick = 0;
};

enum class editor_pending_action {
    none,
    exit_to_song_select,
};

enum class editor_navigation_target {
    none,
    song_select,
    playtest,
};

struct editor_navigation_request {
    editor_navigation_target target = editor_navigation_target::none;
    int playtest_start_tick = 0;

    [[nodiscard]] bool has_value() const {
        return target != editor_navigation_target::none;
    }
};

struct metadata_panel_state {
    ui::text_input_state difficulty_input;
    ui::text_input_state chart_author_input;
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
    editor_pending_action action_after_save = editor_pending_action::none;
};

struct unsaved_changes_dialog_state {
    bool open = false;
    editor_pending_action pending = editor_pending_action::none;
};

struct editor_start_request {
    song_data song;
    std::optional<std::string> chart_path;
    std::optional<chart_meta> initial_meta;
    int new_chart_key_count = 4;
    std::shared_ptr<editor_state> state;
    std::optional<editor_resume_state> resume_state;
};

struct editor_session_load_result {
    std::shared_ptr<editor_state> state;
    std::optional<std::string> chart_path;
    editor_meter_map meter_map;
    editor_timing_panel_state timing_panel;
    metadata_panel_state metadata_panel;
    save_dialog_state save_dialog;
    unsaved_changes_dialog_state unsaved_changes_dialog;
    std::vector<std::string> load_errors;
    int audio_length_tick = 0;
    bool audio_loaded = false;
    bool audio_playing = false;
    double audio_time_seconds = 0.0;
    int playback_tick = 0;
    int previous_playback_tick = 0;
    bool previous_audio_playing = false;
    std::string hitsound_path;
    bool waveform_visible = true;
    int waveform_offset_ms = 0;
    audio_waveform_summary waveform_summary;
    std::vector<editor_timeline_waveform_sample> waveform_samples;
    float bottom_tick = 0.0f;
    float bottom_tick_target = 0.0f;
    float ticks_per_pixel = 2.0f;
    int snap_index = 4;
    std::optional<size_t> selected_note_index;
};

struct editor_flow_context {
    const song_data* song = nullptr;
    const chart_data* chart_for_save = nullptr;
    std::shared_ptr<editor_state> state;
    metadata_panel_state* metadata_panel = nullptr;
    save_dialog_state* save_dialog = nullptr;
    unsaved_changes_dialog_state* unsaved_changes_dialog = nullptr;
    bool escape_pressed = false;
    bool back_clicked = false;
    bool ctrl_s_pressed = false;
    bool f5_pressed = false;
    bool shift_pressed = false;
    bool has_active_metadata_input = false;
    bool has_active_timing_input = false;
    bool timing_bar_pick_mode = false;
    bool timeline_drag_active = false;
    bool save_dialog_submit = false;
    bool save_dialog_cancel = false;
    bool unsaved_save_clicked = false;
    bool unsaved_discard_clicked = false;
    bool unsaved_cancel_clicked = false;
    bool key_count_confirm_clicked = false;
    bool key_count_cancel_clicked = false;
    int playback_tick = 0;
};

struct editor_flow_result {
    editor_navigation_request navigation;
    bool consume_update = false;
    bool request_apply_confirmed_key_count = false;
    bool clear_notes_for_key_count_change = false;
    std::optional<std::string> saved_chart_path;
};

struct editor_transport_context {
    const editor_state* state = nullptr;
    bool audio_loaded = false;
    bool audio_playing = false;
    double audio_time_seconds = 0.0;
    int playback_tick = 0;
    int previous_playback_tick = 0;
    bool previous_audio_playing = false;
    const std::string* hitsound_path = nullptr;
    std::optional<audio_clock_snapshot> bgm_clock;
    std::optional<double> bgm_length_seconds;
    std::optional<int> seek_tick;
    std::optional<int> space_playback_start_tick;
};

struct editor_transport_state {
    bool audio_loaded = false;
    bool audio_playing = false;
    double audio_time_seconds = 0.0;
    int playback_tick = 0;
    int previous_playback_tick = 0;
    bool previous_audio_playing = false;
    int audio_length_tick = 0;
};

struct editor_transport_result {
    bool audio_loaded = false;
    bool audio_playing = false;
    double audio_time_seconds = 0.0;
    int playback_tick = 0;
    int previous_playback_tick = 0;
    bool previous_audio_playing = false;
    int audio_length_tick = 0;
    bool request_play_bgm = false;
    bool request_pause_bgm = false;
    std::optional<double> seek_bgm_seconds;
    int hitsound_count = 0;
    std::optional<int> next_space_playback_start_tick;
};

struct editor_timeline_context {
    const editor_state* state = nullptr;
    const editor_meter_map* meter_map = nullptr;
    editor_timeline_metrics metrics;
    Vector2 mouse = {};
    bool timeline_hovered = false;
    bool left_pressed = false;
    bool left_down = false;
    bool left_released = false;
    bool right_pressed = false;
    bool escape_pressed = false;
    bool alt_down = false;
    int snap_division = 1;
    std::optional<size_t> selected_note_index;
    editor_timeline_note_drag_state drag_state;
};

struct editor_timeline_result {
    std::optional<size_t> selected_note_index;
    editor_timeline_note_drag_state drag_state;
    std::optional<size_t> note_to_delete_index;
    bool request_seek = false;
    int seek_tick = 0;
    bool scroll_seek_if_paused = false;
    bool request_apply_selected_timing = false;
    std::optional<note_data> note_to_add;
};

struct editor_metadata_panel_actions {
    bool metadata_input_activated = false;
    bool metadata_submit_requested = false;
    bool key_count_toggle_requested = false;
};

struct editor_metadata_panel_result {
    bool request_apply_metadata = false;
};

struct editor_timing_panel_actions {
    editor_timing_panel_result panel_result;
    bool clicked_outside_editor = false;
};

struct editor_timing_panel_update_result {
    std::optional<size_t> select_timing_event_index;
    bool request_add_bpm = false;
    bool request_add_meter = false;
    bool request_delete_selected = false;
    bool request_apply_selected = false;
};
