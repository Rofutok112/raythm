#pragma once

#include <functional>
#include <string>
#include <vector>

#include "editor/daw/editor_daw_view.h"
#include "editor/editor_scene_types.h"

namespace editor_screen_view {

struct frame_context {
    std::function<void()> rebuild_hit_regions;
};

struct left_panel_context {
    const song_data& song;
    editor_state& state;
    metadata_panel_state& metadata_panel;
    const editor_note_palette_selection& note_palette;
    const std::vector<std::string>& load_errors;
    double now = 0.0;
};

struct header_context {
    const editor_transport_state& transport;
    const editor_timeline_viewport_state& viewport;
    bool snap_dropdown_open = false;
};

struct metadata_modal_context {
    const song_data& song;
    editor_state& state;
    metadata_panel_state& metadata_panel;
    const editor_note_palette_selection& note_palette;
    const std::vector<std::string>& load_errors;
    double now = 0.0;
};

struct timing_modal_context {
    editor_state& state;
    editor_meter_map& meter_map;
    editor_timing_panel_state& timing_panel;
    const std::vector<size_t>& selected_note_indices;
    bool can_delete_selected_timing_event = false;
    bool can_delete_selected_scroll_event = false;
    const char* offset_label = "";
};

void begin_frame(const frame_context& context);

editor_left_panel_view_result draw_left_panel(const left_panel_context& context);

editor_right_panel_view_result draw_timeline(
    const std::function<editor_right_panel_view_result()>& draw_timeline);

editor_header_view_result draw_header(const header_context& context);

bool draw_save_dialog(save_dialog_state& save_dialog);

void draw_unsaved_changes_dialog();

editor::daw::metadata_modal_result draw_metadata_modal(const metadata_modal_context& context);

editor::daw::timing_modal_result draw_timing_modal(const timing_modal_context& context);

void draw_key_count_confirmation(int pending_key_count);

void end_frame();

}  // namespace editor_screen_view
