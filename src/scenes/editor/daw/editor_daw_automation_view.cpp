#include "editor/daw/editor_daw_automation_view.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "editor/view/editor_layout.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_scroll.h"
#include "ui_text_input.h"
#include "virtual_screen.h"

namespace editor::daw {
namespace {

constexpr std::size_t kAutomationSideGuideCount = 4;
std::array<ui::text_input_state, kAutomationSideGuideCount> gAutomationGuideInputs;

bool accepts_float_character(int codepoint, const std::string& value) {
    if (codepoint >= '0' && codepoint <= '9') {
        return true;
    }
    return codepoint == '.' && value.find('.') == std::string::npos;
}

void reset_automation_guide_input(ui::text_input_state& input, float value) {
    input.value = TextFormat("%.1f", value);
    input.cursor = input.value.size();
    input.has_selection = false;
}

bool is_unity_automation_guide(float value) {
    return std::fabs(value - 1.0f) < 0.0001f;
}

bool automation_guides_are_ordered(const scroll_automation_guides& guides) {
    return guides.values[0] <= guides.values[1] &&
           guides.values[1] <= 1.0f &&
           1.0f <= guides.values[2] &&
           guides.values[2] <= guides.values[3];
}

}  // namespace

std::vector<float> automation_snap_candidates(const std::array<float, 5>& guides,
                                              std::optional<float> pinned_multiplier) {
    std::vector<float> candidates(guides.begin(), guides.end());
    if (pinned_multiplier.has_value() &&
        std::isfinite(*pinned_multiplier) &&
        *pinned_multiplier >= 0.0f) {
        candidates.push_back(*pinned_multiplier);
    }
    return candidates;
}

float snap_automation_multiplier(const std::vector<float>& candidates, float multiplier, bool free_drag) {
    multiplier = std::max(0.0f, multiplier);
    if (free_drag) {
        return std::round(multiplier * 100.0f) / 100.0f;
    }
    if (candidates.empty()) {
        return multiplier;
    }
    float best = candidates[0];
    float best_distance = std::fabs(multiplier - best);
    for (const float candidate : candidates) {
        const float distance = std::fabs(multiplier - candidate);
        if (distance < best_distance) {
            best = candidate;
            best_distance = distance;
        }
    }
    return best;
}

void normalize_automation_guides(scroll_automation_guides& guides) {
    for (float& guide : guides.values) {
        if (!std::isfinite(guide)) {
            guide = 1.0f;
        }
        guide = std::max(0.0f, guide);
    }
}

std::array<float, 5> automation_guides(scroll_automation_guides& guides) {
    normalize_automation_guides(guides);
    return {
        guides.values[0],
        guides.values[1],
        1.0f,
        guides.values[2],
        guides.values[3],
    };
}

float automation_guide_t(std::size_t guide_index) {
    return static_cast<float>(guide_index) / 4.0f;
}

float automation_multiplier_to_t(const std::array<float, 5>& guides, float multiplier) {
    for (std::size_t index = 0; index + 1 < guides.size(); ++index) {
        const float from = guides[index];
        const float to = guides[index + 1];
        const float low = std::min(from, to);
        const float high = std::max(from, to);
        if (multiplier < low || multiplier > high) {
            continue;
        }
        if (std::fabs(to - from) < 0.0001f) {
            return automation_guide_t(index);
        }
        const float segment_t = std::clamp((multiplier - from) / (to - from), 0.0f, 1.0f);
        return (static_cast<float>(index) + segment_t) / 4.0f;
    }

    std::size_t nearest_index = 0;
    float nearest_distance = std::fabs(multiplier - guides[0]);
    for (std::size_t index = 1; index < guides.size(); ++index) {
        const float distance = std::fabs(multiplier - guides[index]);
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest_index = index;
        }
    }
    return automation_guide_t(nearest_index);
}

float automation_multiplier_at_t(const std::array<float, 5>& guides, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float scaled = t * 4.0f;
    const std::size_t index = std::min<std::size_t>(3, static_cast<std::size_t>(std::floor(scaled)));
    const float segment_t = std::clamp(scaled - static_cast<float>(index), 0.0f, 1.0f);
    return guides[index] + (guides[index + 1] - guides[index]) * segment_t;
}

bool draw_automation_guide_input(std::size_t index, Rectangle rect, scroll_automation_guides& guides) {
    if (index >= guides.values.size()) {
        return false;
    }
    normalize_automation_guides(guides);
    ui::text_input_state& input = gAutomationGuideInputs[index];
    const bool hovered = ui::contains_point(rect, virtual_screen::get_virtual_mouse());
    if (!input.active) {
        reset_automation_guide_input(input, guides.values[index]);
    }
    const ui::text_input_result result = ui::text_input(rect, input, "", "x", {
        .font_size = 12,
        .max_length = 8,
        .filter = accepts_float_character,
        .label_width = 0.0f,
        .single_rect = true,
        .plain_when_inactive = true,
    });
    if (result.submitted || result.deactivated) {
        const scroll_automation_guides previous_guides = guides;
        try {
            const float parsed_value = std::max(0.0f, std::stof(input.value));
            if (!is_unity_automation_guide(parsed_value)) {
                guides.values[index] = parsed_value;
            }
        } catch (...) {
        }
        normalize_automation_guides(guides);
        if (is_unity_automation_guide(guides.values[index]) ||
            !automation_guides_are_ordered(guides)) {
            guides = previous_guides;
        }
        reset_automation_guide_input(input, guides.values[index]);
    }
    return hovered || input.active || result.clicked;
}

right_panel_automation_graph_view_result draw_right_panel_automation_graph_view(
    const right_panel_automation_graph_view_model& view_model,
    editor_timing_panel_state& timing_state) {
    const auto& t = *g_theme;
    right_panel_automation_graph_view_result result;
    const std::vector<scroll_automation_point> empty_points;
    const std::vector<scroll_automation_point>& points =
        view_model.points != nullptr ? *view_model.points : empty_points;
    scroll_automation_guides editable_guides = view_model.scroll_guides;
    const std::array<float, 5> guides = automation_guides(editable_guides);

    std::vector<std::pair<std::size_t, scroll_automation_point>> sorted_points;
    sorted_points.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index) {
        sorted_points.push_back({index, points[index]});
        result.max_tick = std::max(result.max_tick, points[index].tick + 480);
    }
    std::stable_sort(sorted_points.begin(), sorted_points.end(), [](const auto& left, const auto& right) {
        return left.second.tick < right.second.tick;
    });

    const Rectangle graph = view_model.graph;
    ui::surface(graph, with_alpha(t.bg_alt, 120), t.border_light, 1.0f);

    auto point_to_pos = [&](const scroll_automation_point& point) {
        const float tick_t =
            static_cast<float>(std::clamp(point.tick, 0, result.max_tick)) / static_cast<float>(result.max_tick);
        const float mult_t = automation_multiplier_to_t(guides, point.multiplier);
        return Vector2{graph.x + mult_t * graph.width, graph.y + tick_t * graph.height};
    };
    auto pos_to_point = [&](Vector2 pos,
                            scroll_automation_curve curve,
                            std::optional<float> pinned_multiplier = std::nullopt) {
        const float tick_t = std::clamp((pos.y - graph.y) / graph.height, 0.0f, 1.0f);
        const float mult_t = std::clamp((pos.x - graph.x) / graph.width, 0.0f, 1.0f);
        scroll_automation_point point;
        point.tick = static_cast<int>(std::round(tick_t * static_cast<float>(result.max_tick) / 10.0f)) * 10;
        point.multiplier = snap_automation_multiplier(
            automation_snap_candidates(guides, pinned_multiplier),
            automation_multiplier_at_t(guides, mult_t),
            ui::is_shift_down());
        point.curve_to_next = curve;
        return point;
    };

    for (std::size_t guide_index = 0; guide_index < guides.size(); ++guide_index) {
        const float guide = guides[guide_index];
        const float x = graph.x + graph.width * automation_guide_t(guide_index);
        const bool unity = std::fabs(guide - 1.0f) < 0.001f;
        ui::draw_line_f(x, graph.y, x, graph.y + graph.height,
                        unity ? with_alpha(t.fast, 210) : with_alpha(t.border_light, 120));
        ui::draw_text_in_rect(TextFormat("%.1fx", guide), 11,
                              {x - 22.0f, graph.y + graph.height + 6.0f, 44.0f, 16.0f},
                              unity ? t.fast : t.text_muted);
    }
    for (int index = 0; index <= 8; ++index) {
        const float y = graph.y + graph.height * static_cast<float>(index) / 8.0f;
        ui::draw_line_f(graph.x, y, graph.x + graph.width, y, with_alpha(t.editor_grid_minor, 120));
    }

    for (std::size_t index = 1; index < sorted_points.size(); ++index) {
        const Vector2 from = point_to_pos(sorted_points[index - 1].second);
        const Vector2 to = point_to_pos(sorted_points[index].second);
        ui::draw_line_ex(from, to, 2.2f, with_alpha(t.fast, 210));
    }

    std::optional<std::size_t> hovered_point;
    for (std::size_t reverse = sorted_points.size(); reverse > 0; --reverse) {
        const std::size_t sorted_index = reverse - 1;
        const std::size_t point_index = sorted_points[sorted_index].first;
        const scroll_automation_point& point = sorted_points[sorted_index].second;
        const Vector2 pos = point_to_pos(point);
        const bool selected = view_model.selected_scroll_event_index.has_value() &&
                              *view_model.selected_scroll_event_index == point_index;
        const Rectangle hit = {pos.x - 8.0f, pos.y - 8.0f, 16.0f, 16.0f};
        if (!hovered_point.has_value() && ui::contains_point(hit, view_model.mouse)) {
            hovered_point = point_index;
        }
        if (timing_state.automation_drag_point_index.has_value() &&
            *timing_state.automation_drag_point_index == point_index) {
            hovered_point = point_index;
        }
        const float handle_size = selected ? 14.0f : 11.0f;
        const Rectangle handle = {pos.x - handle_size * 0.5f, pos.y - handle_size * 0.5f,
                                  handle_size, handle_size};
        ui::surface(handle,
                    selected ? t.accent : t.fast,
                    selected ? t.text : with_alpha(t.text, 170),
                    selected ? 2.0f : 1.4f);
    }

    const bool graph_hovered = ui::contains_point(graph, view_model.mouse);
    if (ui::is_mouse_button_pressed() && graph_hovered) {
        if (hovered_point.has_value()) {
            result.actions.panel_result.selected_scroll_event_index = hovered_point;
            timing_state.automation_drag_point_index = hovered_point;
            timing_state.automation_pending_add = false;
            result.actions.scroll_automation_point_to_modify = std::make_pair(
                *hovered_point,
                pos_to_point(view_model.mouse,
                             points[*hovered_point].curve_to_next,
                             points[*hovered_point].multiplier));
        } else {
            timing_state.automation_drag_point_index.reset();
            timing_state.automation_pending_add = true;
        }
    }
    if (ui::is_mouse_button_released()) {
        if (timing_state.automation_pending_add &&
            graph_hovered &&
            !timing_state.automation_drag_point_index.has_value()) {
            result.actions.scroll_automation_point_to_add =
                pos_to_point(view_model.mouse, scroll_automation_curve::linear);
        }
        timing_state.automation_drag_point_index.reset();
        timing_state.automation_pending_add = false;
    }
    if (ui::is_mouse_button_down() &&
        timing_state.automation_drag_point_index.has_value() &&
        *timing_state.automation_drag_point_index < points.size()) {
        const std::size_t point_index = *timing_state.automation_drag_point_index;
        const scroll_automation_point updated = pos_to_point(
            view_model.mouse,
            points[point_index].curve_to_next,
            points[point_index].multiplier);
        result.actions.panel_result.selected_scroll_event_index = point_index;
        result.actions.scroll_automation_point_to_modify = std::make_pair(point_index, updated);
    }

    return result;
}

editor_right_panel_view_result draw_timeline_automation_view(const timeline_automation_view_model& view_model,
                                                             editor_timing_panel_state& timing_state) {
    editor_right_panel_view_result result;
    if (view_model.timeline == nullptr) {
        return result;
    }

    const auto& t = *g_theme;
    const editor_timeline_view_model& model = *view_model.timeline;
    const Rectangle automation = view_model.panel;

    ui::surface(automation, with_alpha(t.section, 235), t.border_light, 1.0f);
    const Rectangle automation_header = {automation.x, automation.y, 24.0f, automation.height};
    const Rectangle automation_body = {automation_header.x + automation_header.width, automation.y,
                                       automation.width - automation_header.width, automation.height};
    ui::surface(automation_header, with_alpha(t.panel, 235), with_alpha(t.border_light, 190), 1.0f);
    ui::draw_text_in_rect("S", 13,
                          {automation_header.x + 4.0f, automation_header.y + 8.0f,
                           automation_header.width - 8.0f, 18.0f},
                          t.text_secondary);
    std::optional<float> selected_multiplier;
    if (model.selected_scroll_event_index.has_value() &&
        *model.selected_scroll_event_index < model.scroll_automation.size()) {
        selected_multiplier = model.scroll_automation[*model.selected_scroll_event_index].multiplier;
    }
    ui::draw_text_in_rect(selected_multiplier.has_value()
                              ? TextFormat("%.2fx", *selected_multiplier)
                              : "Rate",
                          11,
                          {automation_header.x + 2.0f, automation_header.y + automation_header.height - 28.0f,
                           automation_header.width - 4.0f, 18.0f},
                          selected_multiplier.has_value() ? t.fast : t.text_muted, ui::text_align::left);

    const Rectangle automation_graph = ui::inset(automation_body, ui::edge_insets{10.0f, 10.0f, 10.0f, 8.0f});
    ui::surface_fill(automation_graph, with_alpha(t.bg_alt, 80));

    scroll_automation_guides editable_guides = view_model.scroll_guides;
    const scroll_automation_guides original_guides = editable_guides;
    const std::array<float, 5> guides = automation_guides(editable_guides);
    bool guide_input_interacting = false;
    for (size_t guide_index = 0; guide_index < guides.size(); ++guide_index) {
        const float guide = guides[guide_index];
        const float x = automation_graph.x + automation_graph.width * automation_guide_t(guide_index);
        const bool unity = std::fabs(guide - 1.0f) < 0.001f;
        ui::draw_line_f(x, automation_graph.y, x, automation_graph.y + automation_graph.height,
                        unity ? with_alpha(t.fast, 210) : with_alpha(t.border_light, 110));
        Rectangle value_rect = {x - 34.0f, automation_graph.y + 4.0f, 68.0f, 24.0f};
        value_rect.x = std::clamp(value_rect.x,
                                  automation_graph.x + 2.0f,
                                  automation_graph.x + automation_graph.width - value_rect.width - 2.0f);
        if (unity) {
            ui::draw_text_in_rect("1.0", 12, value_rect, t.fast);
        } else {
            const size_t side_index = guide_index < 2 ? guide_index : guide_index - 1;
            guide_input_interacting = draw_automation_guide_input(side_index, value_rect, editable_guides) ||
                                      guide_input_interacting;
        }
    }
    if (editable_guides.values != original_guides.values) {
        result.scroll_automation_guides_to_modify = editable_guides;
    }

    const int snap_interval = std::max(1, model.snap_interval);
    const int first_snap_tick = std::max(0, (model.min_tick / snap_interval) * snap_interval);
    for (int tick = first_snap_tick; tick <= model.max_tick; tick += snap_interval) {
        const float y = model.metrics.tick_to_y(tick);
        if (ui::y_visible_in_viewport(y, automation_graph)) {
            ui::draw_line_f(automation_graph.x, y, automation_graph.x + automation_graph.width, y,
                            with_alpha(t.editor_grid_snap, 165));
        }
    }
    for (const editor_meter_map::grid_line& line : model.grid_lines) {
        const float y = model.metrics.tick_to_y(line.tick);
        if (!ui::y_visible_in_viewport(y, automation_graph)) {
            continue;
        }
        const Color color = line.major ? t.editor_grid_major : t.editor_grid_minor;
        ui::draw_line_f(automation_graph.x, y, automation_graph.x + automation_graph.width, y, color);
        if (line.major) {
            ui::draw_line_f(automation_graph.x, y + 1.0f, automation_graph.x + automation_graph.width, y + 1.0f,
                            t.editor_grid_major_glow);
        }
    }

    std::vector<std::pair<size_t, editor_timeline_scroll_automation_point>> sorted_points;
    sorted_points.reserve(model.scroll_automation.size());
    for (size_t index = 0; index < model.scroll_automation.size(); ++index) {
        sorted_points.push_back({index, model.scroll_automation[index]});
    }
    std::stable_sort(sorted_points.begin(), sorted_points.end(), [](const auto& left, const auto& right) {
        return left.second.tick < right.second.tick;
    });
    auto point_x = [&](float multiplier) {
        const float t_value = automation_multiplier_to_t(guides, multiplier);
        return automation_graph.x + t_value * automation_graph.width;
    };
    const float unity_x = point_x(1.0f);
    ui::draw_line_f(unity_x, automation_graph.y, unity_x, automation_graph.y + automation_graph.height,
                    with_alpha(t.fast, 150));
    auto point_at_mouse = [&](Vector2 mouse,
                              scroll_automation_curve curve,
                              std::optional<float> pinned_multiplier = std::nullopt) {
        scroll_automation_point point;
        const int raw_tick = std::max(0, model.metrics.y_to_tick(mouse.y));
        point.tick = std::max(0, (raw_tick + snap_interval / 2) / snap_interval * snap_interval);
        point.multiplier = snap_automation_multiplier(
            automation_snap_candidates(guides, pinned_multiplier),
            automation_multiplier_at_t(
                guides,
                std::clamp((mouse.x - automation_graph.x) / automation_graph.width, 0.0f, 1.0f)),
            ui::is_shift_down());
        point.curve_to_next = curve;
        return point;
    };

    const Vector2 mouse = view_model.mouse;
    const bool snap_ui_hovered =
        ui::contains_point(editor::layout::kSnapDropdownRect, mouse) ||
        (view_model.snap_dropdown_open && ui::contains_point(view_model.snap_menu_rect, mouse));
    std::optional<size_t> hovered_point;
    {
        ui::scoped_clip_rect clip_scope(automation_graph);
        for (size_t index = 1; index < sorted_points.size(); ++index) {
            const auto& previous = sorted_points[index - 1].second;
            const auto& current = sorted_points[index].second;
            const Vector2 from = {point_x(previous.multiplier), model.metrics.tick_to_y(previous.tick)};
            const Vector2 to = {point_x(current.multiplier), model.metrics.tick_to_y(current.tick)};
            ui::draw_line_ex(from, to, 2.0f, with_alpha(t.fast, 220));
        }
        for (size_t reverse = sorted_points.size(); reverse > 0; --reverse) {
            const size_t sorted_index = reverse - 1;
            const size_t point_index = sorted_points[sorted_index].first;
            const auto& point = sorted_points[sorted_index].second;
            const Vector2 pos = {point_x(point.multiplier), model.metrics.tick_to_y(point.tick)};
            if (pos.y < automation_graph.y || pos.y > automation_graph.y + automation_graph.height) {
                continue;
            }
            const Rectangle hit = {pos.x - 8.0f, pos.y - 8.0f, 16.0f, 16.0f};
            if (!snap_ui_hovered && !hovered_point.has_value() && ui::contains_point(hit, mouse)) {
                hovered_point = point_index;
            }
            if (timing_state.automation_drag_point_index.has_value() &&
                *timing_state.automation_drag_point_index == point_index) {
                hovered_point = point_index;
            }
            const bool hovered = hovered_point.has_value() && *hovered_point == point_index;
            const float handle_size = hovered ? 13.0f : 11.0f;
            const Rectangle handle = {pos.x - handle_size * 0.5f, pos.y - handle_size * 0.5f,
                                      handle_size, handle_size};
            ui::surface(handle,
                        hovered ? t.accent : t.fast,
                        hovered ? t.text : with_alpha(t.text, 170),
                        hovered ? 2.0f : 1.4f);
        }
    }

    const bool automation_hovered =
        ui::contains_point(automation_graph, mouse) && !guide_input_interacting && !snap_ui_hovered;
    if (ui::is_mouse_button_pressed() && automation_hovered) {
        if (hovered_point.has_value()) {
            result.panel_result.selected_scroll_event_index = hovered_point;
            timing_state.automation_drag_point_index = hovered_point;
            timing_state.automation_pending_add = false;
            result.scroll_automation_point_to_modify = std::make_pair(
                *hovered_point,
                point_at_mouse(mouse,
                               model.scroll_automation[*hovered_point].curve_to_next,
                               model.scroll_automation[*hovered_point].multiplier));
        } else {
            timing_state.automation_drag_point_index.reset();
            timing_state.automation_pending_add = true;
        }
    } else if (ui::is_mouse_button_pressed() && guide_input_interacting) {
        timing_state.automation_drag_point_index.reset();
        timing_state.automation_pending_add = false;
    } else if (ui::is_mouse_button_pressed() && snap_ui_hovered) {
        timing_state.automation_drag_point_index.reset();
        timing_state.automation_pending_add = false;
    }
    if (ui::is_mouse_button_released()) {
        if (timing_state.automation_pending_add &&
            automation_hovered &&
            !timing_state.automation_drag_point_index.has_value()) {
            result.scroll_automation_point_to_add = point_at_mouse(mouse, scroll_automation_curve::linear);
        }
        timing_state.automation_drag_point_index.reset();
        timing_state.automation_pending_add = false;
    }
    if (ui::is_mouse_button_down() &&
        !snap_ui_hovered &&
        timing_state.automation_drag_point_index.has_value() &&
        *timing_state.automation_drag_point_index < model.scroll_automation.size()) {
        result.scroll_automation_point_to_modify = std::make_pair(
            *timing_state.automation_drag_point_index,
            point_at_mouse(mouse,
                           model.scroll_automation[*timing_state.automation_drag_point_index].curve_to_next,
                           model.scroll_automation[*timing_state.automation_drag_point_index].multiplier));
        result.panel_result.selected_scroll_event_index = timing_state.automation_drag_point_index;
    }
    if (ui::is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) && !snap_ui_hovered && hovered_point.has_value()) {
        result.panel_result.selected_scroll_event_index = hovered_point;
        result.panel_result.delete_selected_scroll = true;
    }

    return result;
}

}  // namespace editor::daw
