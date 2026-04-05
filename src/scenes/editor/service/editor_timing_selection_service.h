#pragma once

#include <optional>
#include <vector>

#include "editor/editor_scene_sync.h"

class editor_timing_selection_service final {
public:
    static std::vector<size_t> sorted_indices(const chart_data& data);
    static void select_event(editor_scene_sync_context& sync_context,
                             std::optional<size_t> index,
                             bool scroll_into_view,
                             float& list_scroll_offset);
};
