#pragma once

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "audio_waveform.h"
#include "editor/editor_meter_map.h"
#include "editor/editor_scene_types.h"
#include "editor/editor_state.h"
#include "editor/editor_timing_panel.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "raylib.h"
#include "ui_text_input.h"

struct editor_left_panel_view_model {
    const char* song_title = "";
    float level = 0.0f;
    bool has_file = false;
    bool is_dirty = false;
    metadata_panel_state* metadata_panel = nullptr;
    editor_note_palette_selection note_palette;
    const std::string* load_error = nullptr;
    double now = 0.0;
};

struct editor_left_panel_view_result {
    ui::text_input_result difficulty_result;
    ui::text_input_result author_result;
    bool key_count_left_clicked = false;
    bool key_count_right_clicked = false;
    std::optional<note_type> selected_note_type;
    bool ray_toggled = false;
};

struct editor_header_view_model {
    const char* playback_status = "";
    bool audio_loaded = false;
    bool audio_playing = false;
    const char* offset_label = "";
    bool waveform_visible = true;
    std::span<const char* const> snap_labels = {};
    int snap_index = 0;
    bool snap_dropdown_open = false;
};

struct editor_header_view_result {
    bool restart_requested = false;
    bool playtest_requested = false;
    bool playback_toggled = false;
    bool metadata_modal_requested = false;
    bool timing_modal_requested = false;
    bool offset_left_clicked = false;
    bool offset_right_clicked = false;
    bool waveform_toggled = false;
    int snap_index_clicked = -1;
    bool snap_dropdown_toggled = false;
    bool snap_dropdown_close_requested = false;
};

struct editor_right_panel_view_model {
    const std::vector<timing_event>* timing_events = nullptr;
    const std::vector<scroll_automation_point>* scroll_automation = nullptr;
    const scroll_automation_guides* scroll_guides = nullptr;
    const editor_meter_map* meter_map = nullptr;
    std::optional<size_t> selected_event_index;
    std::optional<size_t> selected_scroll_event_index;
    size_t selected_note_count = 0;
    std::string selected_note_summary;
    bool delete_enabled = false;
    bool scroll_delete_enabled = false;
    Vector2 mouse = {};
};

struct editor_right_panel_view_result {
    editor_timing_panel_result panel_result;
    std::optional<scroll_automation_point> scroll_automation_point_to_add;
    std::optional<std::pair<size_t, scroll_automation_point>> scroll_automation_point_to_modify;
    std::optional<scroll_automation_guides> scroll_automation_guides_to_modify;
    bool clicked_outside_editor = false;
};

struct editor_timeline_presenter_model {
    const editor_state& state;
    const editor_meter_map& meter_map;
    const audio_waveform_summary* waveform_summary = nullptr;
    bool waveform_visible = false;
    int waveform_offset_ms = 0;
    bool audio_loaded = false;
    int playback_tick = 0;
    std::vector<size_t> selected_note_indices;
    std::optional<size_t> selected_scroll_event_index;
    std::vector<note_data> preview_notes;
    std::vector<size_t> preview_note_indices;
    bool preview_has_overlap = false;
    std::optional<Rectangle> selection_rect;
    editor_timeline_viewport_model viewport;
};
