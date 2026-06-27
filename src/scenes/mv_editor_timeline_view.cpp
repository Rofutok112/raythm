#include "mv_editor_timeline_view.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "mv/composition/mv_composition_evaluator.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_hit.h"
#include "ui_text.h"
#include "ui/icons/raythm_icons.h"

namespace {

constexpr float kTimelineRowHeight = 24.0f;
constexpr float kTimelineWheelStep = 44.0f;
constexpr float kTimelineHorizontalWheelMs = 700.0f;

struct timeline_panel_layout {
    Rectangle duration = {};
    Rectangle track_area = {};
    Rectangle layer_name_area = {};
    Rectangle lane_area = {};
};

using mv_icon_draw_fn = void (*)(Rectangle, Color, float);

std::string ms_label(double value_ms) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2fs", value_ms / 1000.0);
    return buffer;
}

void draw_section_title(Rectangle rect, const char* title, const char* subtitle = nullptr) {
    ui::draw_text_in_rect(title, 16, {rect.x + 18.0f, rect.y, rect.width - 36.0f, 30.0f},
                          g_theme->text, ui::text_align::left);
    if (subtitle != nullptr) {
        ui::draw_text_in_rect(subtitle, 12, {rect.x + 18.0f, rect.y + 26.0f, rect.width - 36.0f, 24.0f},
                              g_theme->text_muted, ui::text_align::left);
    }
}

timeline_panel_layout timeline_panel_layout_for(Rectangle panel) {
    const Rectangle track_area = {
        panel.x + 18.0f,
        panel.y + 58.0f,
        panel.width - 36.0f,
        panel.height - 78.0f
    };
    constexpr float layer_gutter_width = 260.0f;
    return {
        .duration = {panel.x + panel.width - 220.0f, panel.y + 16.0f, 196.0f, 24.0f},
        .track_area = track_area,
        .layer_name_area = {track_area.x, track_area.y, layer_gutter_width, track_area.height},
        .lane_area = {track_area.x + layer_gutter_width, track_area.y,
                      track_area.width - layer_gutter_width, track_area.height}
    };
}

bool draw_timeline_icon_button(Rectangle rect, mv_icon_draw_fn icon, bool active,
                               Color icon_color, float icon_inset = 4.0f) {
    const ui::row_state state = ui::row(rect, {
        .border_width = 1.0f,
        .bg = active ? g_theme->row_selected : with_alpha(g_theme->row, 120),
        .bg_hover = g_theme->row_hover,
        .border_color = active ? g_theme->border_active : g_theme->border,
        .custom_colors = true,
    });
    if (icon != nullptr) {
        const Color color = active ? icon_color : with_alpha(icon_color, 150);
        icon(ui::inset(state.visual, icon_inset), color, 2.0f);
    }
    return state.clicked;
}

mv_editor_timeline_layer_row_result draw_mv_timeline_layer_name_row(Rectangle layer_row,
                                                                    const mv::composition::layer& layer,
                                                                    bool selected) {
    ui::surface_fill(layer_row, selected ? g_theme->row_selected : with_alpha(g_theme->section, 145));
    const Rectangle visible_btn = {layer_row.x + 6.0f, layer_row.y + 3.0f, 22.0f, 18.0f};
    const Rectangle lock_btn = {visible_btn.x + visible_btn.width + 4.0f, visible_btn.y, 22.0f, 18.0f};
    const Rectangle delete_btn = {layer_row.x + layer_row.width - 28.0f, visible_btn.y, 22.0f, 18.0f};
    if (draw_timeline_icon_button(visible_btn, raythm_icons::draw_eye, layer.visible, g_theme->text, 3.0f)) {
        return {
            .action = mv_editor_timeline_layer_row_action::toggle_visibility,
            .layer_id = layer.id,
        };
    }
    if (draw_timeline_icon_button(lock_btn,
                                  layer.locked ? raythm_icons::draw_lock : raythm_icons::draw_unlock,
                                  layer.locked,
                                  g_theme->text,
                                  4.0f)) {
        return {
            .action = mv_editor_timeline_layer_row_action::toggle_lock,
            .layer_id = layer.id,
        };
    }
    if (draw_timeline_icon_button(delete_btn, raythm_icons::draw_trash_2, false, g_theme->error, 4.0f)) {
        return {
            .action = mv_editor_timeline_layer_row_action::delete_layer,
            .layer_id = layer.id,
        };
    }
    ui::draw_text_in_rect(layer.name.c_str(), 10,
                          {lock_btn.x + lock_btn.width + 8.0f, layer_row.y,
                           delete_btn.x - lock_btn.x - lock_btn.width - 12.0f, layer_row.height},
                          selected ? g_theme->text : g_theme->text_muted, ui::text_align::left);
    if (ui::is_hovered(layer_row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return {
            .action = mv_editor_timeline_layer_row_action::select,
            .layer_id = layer.id,
        };
    }
    return {};
}

mv_editor_timeline_scrub_result timeline_scrub_result_for(bool lane_hovered,
                                                          bool timeline_dragging,
                                                          bool timeline_span_hit,
                                                          Rectangle lane_area,
                                                          double timeline_horizontal_scroll_ms,
                                                          double visible_duration_ms,
                                                          double duration,
                                                          Vector2 mouse) {
    if (!lane_hovered ||
        !IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
        timeline_dragging ||
        timeline_span_hit) {
        return {};
    }
    const double ratio = std::clamp(
        static_cast<double>((mouse.x - lane_area.x) / std::max(1.0f, lane_area.width)),
        0.0,
        1.0);
    return {
        .requested = true,
        .playhead_ms = std::clamp(
            timeline_horizontal_scroll_ms + ratio * visible_duration_ms,
            0.0,
            duration),
    };
}

mv_editor_timeline_drag_start_result timeline_drag_start_result_for(
    const mv::composition::layer& layer,
    bool locked,
    Rectangle span,
    Rectangle left_handle,
    Rectangle right_handle,
    float timeline_mouse_x,
    double end_ms) {
    if (locked || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return {};
    }

    mv_editor_timeline_drag_update_mode mode = mv_editor_timeline_drag_update_mode::none;
    if (ui::is_hovered(left_handle)) {
        mode = mv_editor_timeline_drag_update_mode::trim_start;
    } else if (ui::is_hovered(right_handle)) {
        mode = mv_editor_timeline_drag_update_mode::trim_end;
    } else if (ui::is_hovered(span)) {
        mode = mv_editor_timeline_drag_update_mode::move;
    }
    if (mode == mv_editor_timeline_drag_update_mode::none) {
        return {};
    }

    return {
        .started = true,
        .layer_id = layer.id,
        .mode = mode,
        .origin_mouse_x = timeline_mouse_x,
        .origin_start_ms = layer.start_ms,
        .origin_duration_ms = layer.duration_ms <= 0.0
            ? std::max(250.0, end_ms - layer.start_ms)
            : layer.duration_ms,
    };
}

mv_editor_timeline_drag_update_result timeline_drag_update_result_for(
    const std::string& layer_id,
    mv_editor_timeline_drag_update_mode mode,
    double timeline_mouse_x,
    float origin_mouse_x,
    double origin_start_ms,
    double origin_duration_ms,
    double ms_per_pixel,
    double safe_duration) {
    if (layer_id.empty() || mode == mv_editor_timeline_drag_update_mode::none) {
        return {};
    }

    mv_editor_timeline_drag_update_result result;
    result.active = true;
    result.layer_id = layer_id;
    result.mode = mode;

    const double delta_ms = (timeline_mouse_x - origin_mouse_x) * ms_per_pixel;
    const double original_start = origin_start_ms;
    const double original_duration = std::max(250.0, origin_duration_ms);
    switch (mode) {
    case mv_editor_timeline_drag_update_mode::none:
        return {};
    case mv_editor_timeline_drag_update_mode::move:
        result.start_ms = std::clamp(
            original_start + delta_ms,
            0.0,
            std::max(0.0, safe_duration - original_duration));
        result.duration_ms = original_duration;
        result.playhead_ms = result.start_ms;
        break;
    case mv_editor_timeline_drag_update_mode::trim_start: {
        const double original_end = std::min(safe_duration, original_start + original_duration);
        const double next_start = std::clamp(original_start + delta_ms, 0.0, original_end - 250.0);
        result.start_ms = next_start;
        result.duration_ms = std::max(250.0, original_end - next_start);
        result.playhead_ms = result.start_ms;
        break;
    }
    case mv_editor_timeline_drag_update_mode::trim_end: {
        const double next_end = std::clamp(
            original_start + original_duration + delta_ms,
            original_start + 250.0,
            safe_duration);
        result.start_ms = original_start;
        result.duration_ms = std::max(250.0, next_end - original_start);
        result.playhead_ms = original_start + result.duration_ms;
        break;
    }
    }

    return result;
}

mv_editor_timeline_drag_end_result timeline_drag_end_result_for(bool timeline_drag_active) {
    return {
        .ended = timeline_drag_active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
    };
}

} // namespace

mv_editor_timeline_view_result draw_mv_timeline_view(Rectangle panel,
                                                     const mv::composition::mv_composition& composition,
                                                     const std::string& selected_layer_id,
                                                     double playhead_ms,
                                                     double duration_ms,
                                                     mv_editor_timeline_view_state state,
                                                     Vector2 mouse,
                                                     float wheel,
                                                     bool shift_down,
                                                     bool ctrl_down) {
    mv_editor_timeline_view_result result{
        .vertical_scroll_offset = state.vertical_scroll_offset,
        .horizontal_scroll_ms = state.horizontal_scroll_ms,
        .zoom = state.zoom,
    };

    ui::panel(panel);
    draw_section_title(panel, "Timeline", "Layer spans and playhead");
    const timeline_panel_layout timeline_layout = timeline_panel_layout_for(panel);
    const std::string duration_text = ms_label(playhead_ms) + " / " + ms_label(duration_ms);
    ui::draw_text_in_rect(duration_text.c_str(), 13, timeline_layout.duration,
                          g_theme->text_muted, ui::text_align::right);
    const Rectangle track_area = timeline_layout.track_area;
    ui::surface(track_area, g_theme->section, g_theme->border_light, 1.0f);
    const Rectangle layer_name_area = timeline_layout.layer_name_area;
    const Rectangle lane_area = timeline_layout.lane_area;
    ui::surface(layer_name_area, with_alpha(g_theme->row, 145), g_theme->border_light, 1.0f);

    const double safe_duration = std::max(1.0, duration_ms);
    const bool timeline_hovered = ui::contains_point(track_area, mouse);
    if (timeline_hovered && wheel != 0.0f) {
        if (ctrl_down) {
            const double mouse_ratio = std::clamp((mouse.x - lane_area.x) / std::max(1.0f, lane_area.width),
                                                  0.0f, 1.0f);
            const double previous_visible_ms = safe_duration / std::max(1.0f, result.zoom);
            const double anchor_ms = result.horizontal_scroll_ms + previous_visible_ms * mouse_ratio;
            result.zoom = std::clamp(result.zoom * (wheel > 0.0f ? 1.18f : 0.85f), 1.0f, 16.0f);
            const double next_visible_ms = std::max(250.0, safe_duration / std::max(1.0f, result.zoom));
            result.horizontal_scroll_ms = anchor_ms - next_visible_ms * mouse_ratio;
        } else if (shift_down) {
            result.horizontal_scroll_ms -= static_cast<double>(wheel) * kTimelineHorizontalWheelMs / result.zoom;
        } else {
            result.vertical_scroll_offset -= wheel * kTimelineWheelStep;
        }
    }

    const double visible_duration_ms = std::max(250.0, safe_duration / std::max(1.0f, result.zoom));
    result.horizontal_scroll_ms = std::clamp(result.horizontal_scroll_ms,
                                             0.0,
                                             std::max(0.0, safe_duration - visible_duration_ms));
    const float timeline_content_height =
        static_cast<float>(composition.objects.size()) * kTimelineRowHeight + 20.0f;
    result.vertical_scroll_offset = std::clamp(result.vertical_scroll_offset,
                                               0.0f,
                                               std::max(0.0f, timeline_content_height - lane_area.height));

    {
        ui::scoped_clip_rect lane_grid_clip(lane_area);
        for (int i = 0; i <= 8; ++i) {
            const float x = lane_area.x + lane_area.width * (static_cast<float>(i) / 8.0f);
            ui::divider({x, lane_area.y, 1.0f, lane_area.height}, g_theme->editor_grid_minor);
            const std::string tick = ms_label(result.horizontal_scroll_ms +
                                              visible_duration_ms * static_cast<double>(i) / 8.0);
            ui::draw_text_in_rect(tick.c_str(), 10,
                                  {x + 4.0f, lane_area.y + 2.0f, 76.0f, 16.0f},
                                  g_theme->text_muted, ui::text_align::left);
        }
    }

    const double ms_per_pixel = visible_duration_ms / std::max(1.0f, lane_area.width);
    bool timeline_span_hit = false;
    bool timeline_dragging = state.drag.active && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    mv_editor_timeline_drag_update_mode active_drag_mode = state.drag.mode;
    std::string active_drag_layer_id = state.drag.layer_id;
    float active_origin_mouse_x = state.drag.origin_mouse_x;
    double active_origin_start_ms = state.drag.origin_start_ms;
    double active_origin_duration_ms = state.drag.origin_duration_ms;

    float row_y = track_area.y + 10.0f - result.vertical_scroll_offset;
    for (const mv::composition::layer& layer : composition.objects) {
        const Rectangle layer_row = {layer_name_area.x, row_y - 4.0f, layer_name_area.width, 24.0f};
        const bool selected = layer.id == selected_layer_id;
        {
            ui::scoped_clip_rect layer_name_clip(layer_name_area);
            const mv_editor_timeline_layer_row_result row_result =
                draw_mv_timeline_layer_name_row(layer_row, layer, selected);
            if (row_result.action == mv_editor_timeline_layer_row_action::delete_layer) {
                result.delete_layer = row_result;
            } else if (result.layer_row.action == mv_editor_timeline_layer_row_action::none) {
                result.layer_row = row_result;
            }
        }

        const float start_x = lane_area.x +
            static_cast<float>((layer.start_ms - result.horizontal_scroll_ms) / visible_duration_ms) * lane_area.width;
        const double end_ms = layer.duration_ms <= 0.0
            ? duration_ms
            : std::min(duration_ms, layer.start_ms + layer.duration_ms);
        const float end_x = lane_area.x +
            static_cast<float>((end_ms - result.horizontal_scroll_ms) / visible_duration_ms) * lane_area.width;
        const Rectangle span = {start_x, row_y, std::max(3.0f, end_x - start_x), 16.0f};
        const bool locked = layer.locked;
        const Color span_color = locked
            ? with_alpha(g_theme->slider_fill, 95)
            : selected ? g_theme->accent : g_theme->slider_fill;
        {
            ui::scoped_clip_rect lane_clip(lane_area);
            ui::surface_fill(span, span_color);
            const Rectangle left_handle = {span.x - 4.0f, span.y - 3.0f, 8.0f, span.height + 6.0f};
            const Rectangle right_handle = {span.x + span.width - 4.0f, span.y - 3.0f, 8.0f, span.height + 6.0f};
            if (selected && !locked) {
                ui::surface_fill(left_handle, with_alpha(g_theme->border_active, 190));
                ui::surface_fill(right_handle, with_alpha(g_theme->border_active, 190));
            }
            if (ui::is_hovered(span) || ui::is_hovered(left_handle) || ui::is_hovered(right_handle)) {
                timeline_span_hit = true;
            }
            const mv_editor_timeline_drag_start_result drag_start_result =
                timeline_drag_start_result_for(
                    layer,
                    locked,
                    span,
                    left_handle,
                    right_handle,
                    mouse.x,
                    end_ms);
            if (drag_start_result.started) {
                result.drag_start = drag_start_result;
                timeline_dragging = true;
                active_drag_mode = drag_start_result.mode;
                active_drag_layer_id = drag_start_result.layer_id;
                active_origin_mouse_x = drag_start_result.origin_mouse_x;
                active_origin_start_ms = drag_start_result.origin_start_ms;
                active_origin_duration_ms = drag_start_result.origin_duration_ms;
            }
            for (const mv::composition::keyframe_track& track : layer.keyframes) {
                if (!mv::composition::is_transform_keyframe_target(track.target)) {
                    continue;
                }
                for (const mv::composition::keyframe& point : track.points) {
                    if (point.time_ms < 0.0 || point.time_ms > duration_ms) {
                        continue;
                    }
                    const float marker_x = lane_area.x +
                        static_cast<float>((point.time_ms - result.horizontal_scroll_ms) / visible_duration_ms) *
                        lane_area.width;
                    const Rectangle marker = {marker_x - 2.5f, row_y - 3.0f, 5.0f, 22.0f};
                    const Color marker_color = layer.id == selected_layer_id
                        ? g_theme->border_active
                        : with_alpha(g_theme->text_muted, 170);
                    ui::surface_fill(marker, marker_color);
                }
            }
        }

        row_y += kTimelineRowHeight;
        if (row_y > lane_area.y + lane_area.height - 12.0f) {
            break;
        }
    }

    if (timeline_dragging) {
        const std::string drag_layer_id = active_drag_layer_id.empty()
            ? selected_layer_id
            : active_drag_layer_id;
        result.drag_update = timeline_drag_update_result_for(
            drag_layer_id,
            active_drag_mode,
            mouse.x,
            active_origin_mouse_x,
            active_origin_start_ms,
            active_origin_duration_ms,
            ms_per_pixel,
            safe_duration);
    }
    result.drag_end = timeline_drag_end_result_for(state.drag.active || result.drag_start.started);
    result.scrub = timeline_scrub_result_for(
        ui::is_hovered(lane_area),
        timeline_dragging,
        timeline_span_hit,
        lane_area,
        result.horizontal_scroll_ms,
        visible_duration_ms,
        duration_ms,
        mouse);

    const float playhead_x = lane_area.x +
        static_cast<float>((playhead_ms - result.horizontal_scroll_ms) / std::max(1.0, visible_duration_ms)) *
            lane_area.width;
    {
        ui::scoped_clip_rect lane_playhead_clip(lane_area);
        ui::accent_bar({playhead_x - 1.0f, lane_area.y, 2.0f, lane_area.height}, g_theme->border_active);
    }

    return result;
}
