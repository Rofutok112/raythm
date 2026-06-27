#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "editor/daw/editor_daw_types.h"
#include "editor/editor_timeline_types.h"
#include "editor/editor_timing_panel.h"

namespace editor::daw {

struct timeline_automation_view_model {
    Rectangle panel = {};
    const editor_timeline_view_model* timeline = nullptr;
    scroll_automation_guides scroll_guides;
    Rectangle snap_menu_rect = {};
    bool snap_dropdown_open = false;
    Vector2 mouse = {};
};

struct right_panel_automation_graph_view_model {
    Rectangle graph = {};
    const std::vector<scroll_automation_point>* points = nullptr;
    scroll_automation_guides scroll_guides;
    std::optional<std::size_t> selected_scroll_event_index;
    Vector2 mouse = {};
};

struct right_panel_automation_graph_view_result {
    editor_right_panel_view_result actions;
    int max_tick = 1920;
};

std::vector<float> automation_snap_candidates(const std::array<float, 5>& guides,
                                              std::optional<float> pinned_multiplier = std::nullopt);
float snap_automation_multiplier(const std::vector<float>& candidates, float multiplier, bool free_drag);
void normalize_automation_guides(scroll_automation_guides& guides);
std::array<float, 5> automation_guides(scroll_automation_guides& guides);
float automation_guide_t(std::size_t guide_index);
float automation_multiplier_to_t(const std::array<float, 5>& guides, float multiplier);
float automation_multiplier_at_t(const std::array<float, 5>& guides, float t);
bool draw_automation_guide_input(std::size_t index, Rectangle rect, scroll_automation_guides& guides);

editor_right_panel_view_result draw_timeline_automation_view(const timeline_automation_view_model& model,
                                                             editor_timing_panel_state& timing_state);
right_panel_automation_graph_view_result draw_right_panel_automation_graph_view(
    const right_panel_automation_graph_view_model& model,
    editor_timing_panel_state& timing_state);

}  // namespace editor::daw
