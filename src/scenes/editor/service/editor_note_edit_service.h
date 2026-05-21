#pragma once

#include <vector>

#include "editor/editor_scene_types.h"
#include "editor/editor_state.h"

struct editor_note_edit_result {
    bool changed = false;
    std::vector<size_t> selected_note_indices;
};

class editor_note_edit_service final {
public:
    static std::vector<note_data> notes_for_selection(const editor_state& state,
                                                      std::vector<size_t> selected_note_indices);
    static editor_note_edit_result paste_or_duplicate(editor_state& state,
                                                      const std::vector<size_t>& selected_note_indices,
                                                      const std::vector<note_data>& clipboard_notes,
                                                      bool duplicate_selection);
    static editor_note_edit_result delete_selection(editor_state& state,
                                                    const std::vector<size_t>& selected_note_indices);
    static editor_note_edit_result apply_timeline_notes(editor_state& state,
                                                       const editor_timeline_result& timeline_result);
};
