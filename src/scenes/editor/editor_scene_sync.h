#pragma once

#include "editor_scene_types.h"

struct editor_scene_sync_context {
    editor_state& state;
    editor_meter_map& meter_map;
    editor_timing_panel_state& timing_panel;
    metadata_panel_state& metadata_panel;
    std::optional<size_t>& selected_note_index;
};

namespace editor_scene_sync {

void sync_metadata_inputs(editor_scene_sync_context context);
void sync_timing_event_selection(editor_scene_sync_context context);
void load_timing_event_inputs(editor_scene_sync_context context);
void clear_timing_event_inputs(editor_scene_sync_context context);
void sync_after_history_change(editor_scene_sync_context context);
void sync_after_timing_change(editor_scene_sync_context context);
void sync_after_metadata_change(editor_scene_sync_context context, bool key_count_changed);
void sync_after_offset_change(editor_scene_sync_context context);

}  // namespace editor_scene_sync
