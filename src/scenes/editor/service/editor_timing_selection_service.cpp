#include "editor/service/editor_timing_selection_service.h"

#include <algorithm>

namespace {
bool timing_event_sort_less(const timing_event& left, size_t left_index,
                            const timing_event& right, size_t right_index) {
    if (left.tick != right.tick) {
        return left.tick < right.tick;
    }
    if (left.type != right.type) {
        return left.type == timing_event_type::bpm;
    }
    return left_index < right_index;
}
}

std::vector<size_t> editor_timing_selection_service::sorted_indices(const chart_data& data) {
    std::vector<size_t> indices(data.timing_events.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    std::stable_sort(indices.begin(), indices.end(), [&data](size_t left_index, size_t right_index) {
        const timing_event& left = data.timing_events[left_index];
        const timing_event& right = data.timing_events[right_index];
        return timing_event_sort_less(right, right_index, left, left_index);
    });
    return indices;
}

void editor_timing_selection_service::select_event(editor_scene_sync_context& sync_context,
                                                   std::optional<size_t> index,
                                                   bool scroll_into_view,
                                                   float& list_scroll_offset) {
    sync_context.timing_panel.selected_event_index = index;
    sync_context.timing_panel.active_input_field = editor_timing_input_field::none;
    sync_context.timing_panel.input_error.clear();
    sync_context.timing_panel.bar_pick_mode = false;
    editor_scene_sync::load_timing_event_inputs(sync_context);

    if (scroll_into_view && index.has_value() && *index < sync_context.state.data().timing_events.size()) {
        const auto timing_indices = sorted_indices(sync_context.state.data());
        const auto it = std::find(timing_indices.begin(), timing_indices.end(), *index);
        if (it != timing_indices.end()) {
            constexpr float kTimingRowHeight = 30.0f;
            constexpr float kTimingRowGap = 4.0f;
            constexpr float kTimingListViewportHeight = 174.0f;
            const float row_top = static_cast<float>(std::distance(timing_indices.begin(), it)) * (kTimingRowHeight + kTimingRowGap);
            const float row_bottom = row_top + kTimingRowHeight;
            if (row_top < list_scroll_offset) {
                list_scroll_offset = row_top;
            } else if (row_bottom > list_scroll_offset + kTimingListViewportHeight) {
                list_scroll_offset = row_bottom - kTimingListViewportHeight;
            }
        }
    }
}
