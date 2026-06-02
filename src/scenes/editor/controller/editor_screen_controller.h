#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "editor/controller/editor_timing_action_controller.h"
#include "editor/daw/editor_daw_types.h"
#include "editor/editor_scene_sync.h"
#include "editor/editor_scene_types.h"

namespace editor_screen_controller {

struct context {
    const song_data& song;
    editor_state& state;
    editor_meter_map& meter_map;
    editor_timing_panel_state& timing_panel;
    metadata_panel_state& metadata_panel;
    editor_transport_state& transport;
    std::optional<int>& space_playback_start_tick;
    const std::string& hitsound_path;
    const editor_hitsound_paths& hitsounds;
    bool& waveform_visible;
    editor_timeline_viewport_state& viewport;
    bool& snap_dropdown_open;
    std::vector<size_t>& selected_note_indices;
    editor_timeline_note_drag_state& timeline_drag;
    editor_note_palette_selection& note_palette;
    const std::vector<std::string>& load_errors;
    save_dialog_state& save_dialog;
    unsaved_changes_dialog_state& unsaved_changes_dialog;
    bool& metadata_modal_open;
    bool& timing_modal_open;
    bool& playtest_button_requested;
    std::function<void()> rebuild_hit_regions;
    std::function<editor_right_panel_view_result()> draw_timeline;
    std::function<editor_scene_sync_context()> make_sync_context;
    std::function<editor_timing_action_controller::context()> timing_action_context;
    std::function<void(std::optional<size_t>, bool)> select_timing_event;
    std::function<void(std::optional<size_t>, bool)> select_scroll_event;
    std::function<void(int)> scroll_to_tick;
    std::function<bool(bool)> apply_metadata_changes;
    std::function<bool(int)> apply_chart_offset;
};

void draw_and_update(const context& context);

}  // namespace editor_screen_controller
