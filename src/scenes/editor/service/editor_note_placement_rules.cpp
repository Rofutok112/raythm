#include "editor_note_placement_rules.h"

#include <algorithm>

namespace editor::note_placement_rules {
namespace {

bool lanes_overlap(const note_data& left, const note_data& right) {
    return left.lane <= note_last_lane(right) && right.lane <= note_last_lane(left);
}

bool is_ignored(size_t index, const std::vector<size_t>& ignore_indices) {
    return std::find(ignore_indices.begin(), ignore_indices.end(), index) != ignore_indices.end();
}

bool stay_stack(const note_data& left, const note_data& right) {
    return left.type == note_type::stay &&
        right.type == note_type::stay &&
        left.tick == right.tick &&
        lanes_overlap(left, right);
}

}

bool has_stay_stack(const chart_data& chart,
                    const note_data& candidate,
                    std::optional<size_t> ignore_index) {
    std::vector<size_t> ignore_indices;
    if (ignore_index.has_value()) {
        ignore_indices.push_back(*ignore_index);
    }
    return has_stay_stack(chart, candidate, ignore_indices);
}

bool has_stay_stack(const chart_data& chart,
                    const note_data& candidate,
                    const std::vector<size_t>& ignore_indices) {
    if (candidate.type != note_type::stay) {
        return false;
    }

    for (size_t i = 0; i < chart.notes.size(); ++i) {
        if (is_ignored(i, ignore_indices)) {
            continue;
        }
        if (stay_stack(candidate, chart.notes[i])) {
            return true;
        }
    }
    return false;
}

bool has_stay_stack(const chart_data& chart,
                    const std::vector<note_data>& candidates,
                    const std::vector<size_t>& ignore_indices) {
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (has_stay_stack(chart, candidates[i], ignore_indices)) {
            return true;
        }
        for (size_t j = i + 1; j < candidates.size(); ++j) {
            if (stay_stack(candidates[i], candidates[j])) {
                return true;
            }
        }
    }
    return false;
}

}
