#include "mv_editor_side_panel_view.h"

#include <algorithm>
#include <filesystem>

#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_text.h"

namespace {

constexpr float kHierarchyRowHeight = 34.0f;
constexpr float kPanelWheelStep = 44.0f;

struct scroll_panel_layout {
    Rectangle viewport = {};
    Rectangle scrollbar = {};
};

void draw_section_title(Rectangle rect, const char* title, const char* subtitle = nullptr) {
    ui::draw_text_in_rect(title, 16, {rect.x + 18.0f, rect.y, rect.width - 36.0f, 30.0f},
                          g_theme->text, ui::text_align::left);
    if (subtitle != nullptr) {
        ui::draw_text_in_rect(subtitle, 12, {rect.x + 18.0f, rect.y + 26.0f, rect.width - 36.0f, 24.0f},
                              g_theme->text_muted, ui::text_align::left);
    }
}

scroll_panel_layout hierarchy_scroll_layout_for(Rectangle panel) {
    const Rectangle viewport = {
        panel.x + 10.0f,
        panel.y + 54.0f,
        panel.width - 32.0f,
        panel.height - 64.0f
    };
    return {
        .viewport = viewport,
        .scrollbar = {viewport.x + viewport.width + 8.0f, viewport.y, 8.0f, viewport.height}
    };
}

scroll_panel_layout panel_scroll_layout_for(Rectangle panel) {
    const Rectangle viewport = {
        panel.x + 18.0f,
        panel.y + 58.0f,
        panel.width - 50.0f,
        panel.height - 76.0f
    };
    return {
        .viewport = viewport,
        .scrollbar = {viewport.x + viewport.width + 8.0f, viewport.y, 8.0f, viewport.height}
    };
}

std::string layer_type_label(const mv::composition::layer& layer) {
    const mv::composition::component* renderer = mv::composition::renderable_component(layer);
    if (renderer == nullptr) {
        return "Empty";
    }
    if (renderer->type == "ShapeRenderer" && !renderer->shape.empty()) {
        return renderer->type + "/" + renderer->shape;
    }
    return renderer->type.empty() ? "unknown" : renderer->type;
}

std::size_t layer_index_by_id(const mv::composition::mv_composition& composition,
                              const std::string& layer_id) {
    const auto it = std::find_if(composition.objects.begin(), composition.objects.end(), [&](const auto& layer) {
        return layer.id == layer_id;
    });
    if (it == composition.objects.end()) {
        return static_cast<std::size_t>(-1);
    }
    return static_cast<std::size_t>(std::distance(composition.objects.begin(), it));
}

} // namespace

mv_editor_hierarchy_result draw_mv_hierarchy_panel(Rectangle panel,
                                                   const mv::composition::mv_composition& composition,
                                                   const std::string& selected_layer_id,
                                                   float scroll_offset,
                                                   bool scrollbar_dragging,
                                                   float scrollbar_drag_offset,
                                                   Vector2 mouse,
                                                   float wheel,
                                                   bool shift_down,
                                                   bool ctrl_down) {
    mv_editor_hierarchy_result result{
        .scroll_offset = scroll_offset,
        .scrollbar_dragging = scrollbar_dragging,
        .scrollbar_drag_offset = scrollbar_drag_offset,
    };

    ui::panel(panel);
    draw_section_title(panel, "Hierarchy", "Right-click to create");
    const scroll_panel_layout hierarchy_layout = hierarchy_scroll_layout_for(panel);
    const Rectangle layer_view = hierarchy_layout.viewport;
    const Rectangle hierarchy_scrollbar = hierarchy_layout.scrollbar;
    const float content_height =
        static_cast<float>(composition.objects.size()) * (kHierarchyRowHeight + 6.0f);
    const float max_scroll = std::max(0.0f, content_height - layer_view.height);

    result.scroll_offset = std::clamp(result.scroll_offset, 0.0f, max_scroll);
    if (ui::contains_point(layer_view, mouse) && wheel != 0.0f && !shift_down && !ctrl_down) {
        result.scroll_offset = std::clamp(result.scroll_offset - wheel * kPanelWheelStep, 0.0f, max_scroll);
    }

    bool dragging = result.scrollbar_dragging;
    float drag_offset = result.scrollbar_drag_offset;
    const ui::scrollbar_interaction scrollbar_result =
        ui::vertical_scrollbar(hierarchy_scrollbar, content_height, result.scroll_offset, dragging, drag_offset, {
            .min_thumb_height = 30.0f,
            .drag_blocked_by_layer = false,
        });
    result.scroll_offset = scrollbar_result.scroll_offset;
    result.scrollbar_dragging = scrollbar_result.dragging;
    result.scrollbar_drag_offset = drag_offset;

    {
        ui::scoped_clip_rect clip(layer_view);
        float y = layer_view.y - result.scroll_offset;
        for (const mv::composition::layer& layer : composition.objects) {
            const std::size_t layer_index = layer_index_by_id(composition, layer.id);
            const Rectangle row = {layer_view.x, y, layer_view.width, kHierarchyRowHeight};
            const bool selected = layer.id == selected_layer_id;
            const auto state = ui::selectable_row(row, selected, 1.5f);
            if (state.clicked) {
                result.selected_layer_id = layer.id;
            }
            const float reorder_button_width = selected ? 28.0f : 0.0f;
            const float reorder_gap = selected ? 5.0f : 0.0f;
            const float text_width = row.width - 20.0f - reorder_button_width * 2.0f - reorder_gap * 2.0f;
            ui::draw_text_in_rect(layer.name.c_str(), 12,
                                  {row.x + 10.0f, row.y + 2.0f, text_width, 17.0f},
                                  g_theme->text, ui::text_align::left);
            const std::string meta = layer_type_label(layer) + "   z " + std::to_string(layer.order);
            ui::draw_text_in_rect(meta.c_str(), 10,
                                  {row.x + 10.0f, row.y + 18.0f, text_width, 15.0f},
                                  g_theme->text_muted, ui::text_align::left);
            if (selected) {
                const Rectangle up_btn = {row.x + row.width - 62.0f, row.y + 5.0f, 26.0f, 24.0f};
                const Rectangle down_btn = {up_btn.x + up_btn.width + 5.0f, up_btn.y, 26.0f, 24.0f};
                const bool can_move_up = layer_index != static_cast<std::size_t>(-1) &&
                                         layer_index + 1 < composition.objects.size();
                const bool can_move_down = layer_index != static_cast<std::size_t>(-1) && layer_index > 0;
                if (ui::button(up_btn, "Up", {
                        .font_size = 9,
                        .border_width = 1.5f,
                        .bg = can_move_up ? g_theme->row : with_alpha(g_theme->row, 110),
                        .bg_hover = g_theme->row_hover,
                        .text_color = g_theme->text,
                        .custom_colors = true,
                    }).clicked &&
                    can_move_up) {
                    result.move_direction = 1;
                }
                if (ui::button(down_btn, "Dn", {
                        .font_size = 9,
                        .border_width = 1.5f,
                        .bg = can_move_down ? g_theme->row : with_alpha(g_theme->row, 110),
                        .bg_hover = g_theme->row_hover,
                        .text_color = g_theme->text,
                        .custom_colors = true,
                    }).clicked &&
                    can_move_down) {
                    result.move_direction = -1;
                }
            }
            y += kHierarchyRowHeight + 6.0f;
        }
    }
    ui::scrollbar(hierarchy_scrollbar, content_height, result.scroll_offset, {
        .track_color = with_alpha(g_theme->row, 120),
        .thumb_color = g_theme->slider_fill,
        .min_thumb_height = 30.0f,
        .custom_colors = true,
    });

    return result;
}

mv_editor_project_panel_result draw_mv_project_panel(Rectangle panel,
                                                     const mv::composition::mv_composition& composition,
                                                     const std::string& selected_asset_id,
                                                     float scroll_offset,
                                                     bool scrollbar_dragging,
                                                     float scrollbar_drag_offset,
                                                     Vector2 mouse,
                                                     float wheel,
                                                     bool shift_down,
                                                     bool ctrl_down) {
    mv_editor_project_panel_result result{
        .scroll_offset = scroll_offset,
        .scrollbar_dragging = scrollbar_dragging,
        .scrollbar_drag_offset = scrollbar_drag_offset,
    };

    ui::panel(panel);
    draw_section_title(panel, "Project", "Right-click to add assets");
    const scroll_panel_layout project_layout = panel_scroll_layout_for(panel);
    const Rectangle project_view = project_layout.viewport;
    const Rectangle project_scrollbar = project_layout.scrollbar;
    const int row_count = static_cast<int>(composition.assets.size()) + 2;
    const float content_height = static_cast<float>(row_count) * (kHierarchyRowHeight + 5.0f);
    const float max_scroll = std::max(0.0f, content_height - project_view.height);

    result.scroll_offset = std::clamp(result.scroll_offset, 0.0f, max_scroll);
    if (ui::contains_point(project_view, mouse) && wheel != 0.0f && !shift_down && !ctrl_down) {
        result.scroll_offset = std::clamp(result.scroll_offset - wheel * kPanelWheelStep, 0.0f, max_scroll);
    }

    bool dragging = result.scrollbar_dragging;
    float drag_offset = result.scrollbar_drag_offset;
    const ui::scrollbar_interaction scrollbar_result =
        ui::vertical_scrollbar(project_scrollbar, content_height, result.scroll_offset, dragging, drag_offset, {
            .min_thumb_height = 30.0f,
            .drag_blocked_by_layer = false,
        });
    result.scroll_offset = scrollbar_result.scroll_offset;
    result.scrollbar_dragging = scrollbar_result.dragging;
    result.scrollbar_drag_offset = drag_offset;

    {
        ui::scoped_clip_rect clip(project_view);
        float y = project_view.y - result.scroll_offset;
        auto draw_project_header = [&](const char* label) {
            const Rectangle row = {project_view.x, y, project_view.width, 22.0f};
            ui::draw_text_in_rect(label, 11, row, g_theme->text_muted, ui::text_align::left);
            y += 28.0f;
        };
        auto draw_asset_row = [&](const mv::composition::asset_ref& asset) {
            const Rectangle row = {project_view.x, y, project_view.width, kHierarchyRowHeight};
            const bool selected = asset.id == selected_asset_id;
            const auto state = ui::selectable_row(row, selected, 1.5f);
            if (state.clicked) {
                result.selected_asset_id = asset.id;
                result.assign_asset_id = asset.id;
            }
            const std::string name = std::filesystem::path(asset.path).filename().generic_string();
            ui::draw_text_in_rect(name.c_str(), 11,
                                  {row.x + 10.0f, row.y + 2.0f, row.width - 20.0f, 17.0f},
                                  g_theme->text, ui::text_align::left);
            const std::string meta = asset.type + "   " + asset.id;
            ui::draw_text_in_rect(meta.c_str(), 9,
                                  {row.x + 10.0f, row.y + 18.0f, row.width - 20.0f, 15.0f},
                                  g_theme->text_muted, ui::text_align::left);
            y += kHierarchyRowHeight + 5.0f;
        };
        draw_project_header("Images");
        for (const mv::composition::asset_ref& asset : composition.assets) {
            if (asset.type == "image") {
                draw_asset_row(asset);
            }
        }
        draw_project_header("Scripts");
        for (const mv::composition::asset_ref& asset : composition.assets) {
            if (asset.type == "script") {
                draw_asset_row(asset);
            }
        }
    }
    ui::scrollbar(project_scrollbar, content_height, result.scroll_offset, {
        .track_color = with_alpha(g_theme->row, 120),
        .thumb_color = g_theme->slider_fill,
        .min_thumb_height = 30.0f,
        .custom_colors = true,
    });

    return result;
}
