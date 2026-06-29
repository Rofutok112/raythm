#include "mv_editor_side_panel_view.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_scroll.h"
#include "ui_text.h"

namespace {

constexpr float kHierarchyRowHeight = 34.0f;
constexpr float kHierarchyRowGap = 6.0f;
constexpr float kProjectRowGap = 5.0f;
constexpr float kPanelWheelStep = 44.0f;

struct scroll_panel_layout {
    Rectangle viewport = {};
    Rectangle scrollbar = {};
};

enum class project_panel_row_kind {
    header,
    asset,
};

struct project_panel_row {
    project_panel_row_kind kind = project_panel_row_kind::header;
    ui::vertical_stack_item metrics = {};
    const char* header_label = nullptr;
    const mv::composition::asset_ref* asset = nullptr;
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

float hierarchy_content_height(std::size_t row_count) {
    return ui::vertical_list_content_height_with_trailing_padding(
        row_count,
        kHierarchyRowHeight,
        kHierarchyRowGap,
        kHierarchyRowGap);
}

float project_content_height(int row_count) {
    return ui::vertical_list_content_height_with_trailing_padding(
        row_count,
        kHierarchyRowHeight,
        kProjectRowGap,
        kProjectRowGap);
}

constexpr Rectangle hierarchy_row_rect(Rectangle viewport, int index, float scroll_offset) {
    return ui::vertical_list_row_rect(viewport, index, kHierarchyRowHeight, kHierarchyRowGap, scroll_offset);
}

std::vector<project_panel_row> project_panel_rows_for(const mv::composition::mv_composition& composition) {
    std::vector<project_panel_row> rows;
    rows.reserve(composition.assets.size() + 2);
    float y = 0.0f;
    auto push_header = [&](const char* label) {
        rows.push_back({
            .kind = project_panel_row_kind::header,
            .metrics = {y, 22.0f},
            .header_label = label,
        });
        y += 28.0f;
    };
    auto push_assets = [&](const char* type) {
        for (const mv::composition::asset_ref& asset : composition.assets) {
            if (asset.type != type) {
                continue;
            }
            rows.push_back({
                .kind = project_panel_row_kind::asset,
                .metrics = {y, kHierarchyRowHeight},
                .asset = &asset,
            });
            y += kHierarchyRowHeight + kProjectRowGap;
        }
    };

    push_header("Images");
    push_assets("image");
    push_header("Scripts");
    push_assets("script");
    return rows;
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
    const float content_height = hierarchy_content_height(composition.objects.size());
    ui::scroll_offset_state scroll_state =
        ui::scroll_offset_state_for(layer_view, content_height, result.scroll_offset);

    result.scroll_offset = scroll_state.offset;
    if (!shift_down && !ctrl_down) {
        scroll_state = ui::wheel_scrolled_offset_state(
            layer_view, mouse, wheel, content_height, result.scroll_offset, kPanelWheelStep);
        result.scroll_offset = scroll_state.offset;
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
        const ui::index_range visible_rows = ui::vertical_list_visible_range(
            composition.objects.size(), layer_view, kHierarchyRowHeight, kHierarchyRowGap, result.scroll_offset);
        for (int row_index = visible_rows.begin; row_index < visible_rows.end; ++row_index) {
            const mv::composition::layer& layer = composition.objects[static_cast<std::size_t>(row_index)];
            const std::size_t layer_index = layer_index_by_id(composition, layer.id);
            const Rectangle row = hierarchy_row_rect(layer_view, row_index, result.scroll_offset);
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
    const float content_height = project_content_height(row_count);
    ui::scroll_offset_state scroll_state =
        ui::scroll_offset_state_for(project_view, content_height, result.scroll_offset);

    result.scroll_offset = scroll_state.offset;
    if (!shift_down && !ctrl_down) {
        scroll_state = ui::wheel_scrolled_offset_state(
            project_view, mouse, wheel, content_height, result.scroll_offset, kPanelWheelStep);
        result.scroll_offset = scroll_state.offset;
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
        const std::vector<project_panel_row> rows = project_panel_rows_for(composition);
        const ui::index_range visible_rows = ui::vertical_stack_visible_range(
            static_cast<int>(rows.size()),
            [&](int index) {
                return rows[static_cast<std::size_t>(index)].metrics;
            },
            project_view,
            result.scroll_offset);
        for (int row_index = visible_rows.begin; row_index < visible_rows.end; ++row_index) {
            const project_panel_row& item = rows[static_cast<std::size_t>(row_index)];
            const Rectangle row = ui::vertical_stack_item_rect(project_view, item.metrics, result.scroll_offset);
            if (item.kind == project_panel_row_kind::header) {
                ui::draw_text_in_rect(item.header_label, 11, row, g_theme->text_muted, ui::text_align::left);
                continue;
            }
            const mv::composition::asset_ref& asset = *item.asset;
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
