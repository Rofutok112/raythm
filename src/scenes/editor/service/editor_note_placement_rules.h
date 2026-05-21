#pragma once

#include <optional>
#include <vector>

#include "models/data_models.h"

class editor_state;

namespace editor::note_placement_rules {

bool has_stay_stack(const editor_state& state,
                    const note_data& candidate,
                    std::optional<size_t> ignore_index = std::nullopt);
bool has_stay_stack(const editor_state& state,
                    const note_data& candidate,
                    const std::vector<size_t>& ignore_indices);
bool has_stay_stack(const editor_state& state,
                    const std::vector<note_data>& candidates,
                    const std::vector<size_t>& ignore_indices = {});

bool has_stay_stack(const chart_data& chart,
                    const note_data& candidate,
                    std::optional<size_t> ignore_index = std::nullopt);
bool has_stay_stack(const chart_data& chart,
                    const note_data& candidate,
                    const std::vector<size_t>& ignore_indices);
bool has_stay_stack(const chart_data& chart,
                    const std::vector<note_data>& candidates,
                    const std::vector<size_t>& ignore_indices = {});

}
