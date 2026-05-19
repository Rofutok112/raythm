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
    std::vector<size_t> selected_note_indices;
};

enum class editor_timeline_drag_mode {
    create,
    resize_left,
    resize_right,
    resize_start,
    resize_end,
    move_notes,
    range_select,
    scroll_resize_start,
    scroll_resize_end,
};

struct editor_timeline_note_drag_state {
    bool active = false;
    editor_timeline_drag_mode mode = editor_timeline_drag_mode::create;
    std::optional<size_t> note_index;
    int lane = 0;
    int current_lane = 0;
    int start_tick = 0;
    int current_tick = 0;
    Vector2 start_mouse = {};
    Vector2 current_mouse = {};
    note_data original_note;
    std::vector<size_t> note_indices;
    std::vector<note_data> original_notes;
    std::optional<size_t> scroll_event_index;
    scroll_event original_scroll_event;
};

struct editor_note_palette_selection {
    note_type type = note_type::tap;
    bool is_ray = false;
};

enum class editor_right_panel_tab {
    timing,
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

struct editor_hitsound_request {
    note_type type = note_type::tap;
    bool is_ray = false;
};

struct editor_hitsound_paths {
    std::string tap;
    std::string ray_tap;
    std::string release;
    std::string ray_release;
    std::string stay;
    std::string ray_stay;

    [[nodiscard]] bool has_any() const {
        return !tap.empty() || !ray_tap.empty() || !release.empty() ||
               !ray_release.empty() || !stay.empty() || !ray_stay.empty();
    }

    [[nodiscard]] const std::string& path_for(const editor_hitsound_request& request) const {
        if (request.type == note_type::release) {
            if (request.is_ray && !ray_release.empty()) {
                return ray_release;
            }
            return release.empty() ? tap : release;
        }
        if (request.type == note_type::stay) {
            if (request.is_ray && !ray_stay.empty()) {
                return ray_stay;
            }
            return stay.empty() ? tap : stay;
        }
        if (request.is_ray && !ray_tap.empty()) {
            return ray_tap;
        }
        return tap;
    }
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
    editor_hitsound_paths hitsounds;
    bool waveform_visible = true;
    int waveform_offset_ms = 0;
    audio_waveform_summary waveform_summary;
    float bottom_tick = 0.0f;
    float bottom_tick_target = 0.0f;
    float ticks_per_pixel = 2.0f;
    int snap_index = 4;
    std::optional<size_t> selected_note_index;
    std::vector<size_t> selected_note_indices;
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
    const editor_hitsound_paths* hitsounds = nullptr;
    std::optional<audio_clock_snapshot> bgm_clock;
    std::optional<double> bgm_length_seconds;
    std::optional<int> seek_tick;
    std::optional<int> space_playback_start_tick;
    bool loop_enabled = false;
    int loop_start_tick = 0;
    int loop_end_tick = 0;
};

struct editor_transport_state {
    bool audio_loaded = false;
    bool audio_playing = false;
    double audio_time_seconds = 0.0;
    int playback_tick = 0;
    int previous_playback_tick = 0;
    bool previous_audio_playing = false;
    int audio_length_tick = 0;
    bool loop_enabled = false;
    int loop_start_tick = 0;
    int loop_end_tick = 0;
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
    std::vector<editor_hitsound_request> hitsound_requests;
    std::optional<int> next_space_playback_start_tick;
    bool loop_seeked = false;
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
    editor_note_palette_selection palette;
    std::vector<size_t> selected_note_indices;
    bool ctrl_down = false;
    bool shift_down = false;
};

struct editor_timeline_result {
    std::optional<size_t> selected_note_index;
    std::vector<size_t> selected_note_indices;
    editor_timeline_note_drag_state drag_state;
    std::optional<size_t> note_to_delete_index;
    std::vector<size_t> notes_to_delete_indices;
    bool request_seek = false;
    int seek_tick = 0;
    bool scroll_seek_if_paused = false;
    bool request_apply_selected_timing = false;
    bool request_apply_selected_scroll = false;
    std::optional<size_t> selected_scroll_event_index;
    std::optional<note_data> note_to_add;
    std::optional<size_t> note_to_modify_index;
    std::optional<note_data> note_to_modify;
    std::vector<std::pair<size_t, note_data>> notes_to_modify;
    std::optional<size_t> scroll_event_to_modify_index;
    std::optional<scroll_event> scroll_event_to_modify;
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
    std::optional<size_t> select_scroll_event_index;
    bool request_add_bpm = false;
    bool request_add_meter = false;
    bool request_add_speed = false;
    bool request_add_stop = false;
    bool request_delete_selected = false;
    bool request_delete_selected_scroll = false;
    bool request_apply_selected = false;
    bool request_apply_selected_scroll = false;
};
