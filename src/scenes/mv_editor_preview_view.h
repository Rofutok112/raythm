#pragma once

#include <array>
#include <string>

#include "mv/composition/mv_composition.h"
#include "raylib.h"

enum class mv_preview_drag_mode {
    none,
    move,
    north,
    south,
    west,
    east,
    northwest,
    northeast,
    southwest,
    southeast,
};

struct mv_editor_preview_drag_update_result {
    bool active = false;
    std::string layer_id;
    mv_preview_drag_mode mode = mv_preview_drag_mode::none;
    Rectangle next_bounds = {};
};

struct mv_editor_preview_drag_start_result {
    std::string selected_layer_id;
    bool sync_inspector = false;
    bool drag_started = false;
    std::string drag_layer_id;
    mv_preview_drag_mode mode = mv_preview_drag_mode::none;
    Vector2 origin_mouse = {0.0f, 0.0f};
    Rectangle origin_rect = {};
    mv::composition::transform origin_transform;
};

struct mv_editor_preview_drag_end_result {
    bool ended = false;
    bool cancelled = false;
};

bool preview_transformable(const mv::composition::layer& layer);

void draw_preview_transform_overlay(Rectangle bounds, bool locked);

void draw_mv_preview_background(Rectangle preview,
                                const mv::composition::mv_composition& composition);

Rectangle layer_preview_bounds(Rectangle preview,
                               const mv::composition::mv_composition& composition,
                               const mv::composition::layer& layer,
                               const Texture2D* texture);

void apply_preview_rect_to_transform(Rectangle preview,
                                     const mv::composition::mv_composition& composition,
                                     const mv::composition::layer& layer,
                                     const Texture2D* texture,
                                     Rectangle bounds,
                                     mv::composition::transform& transform);

void draw_preview_layer(Rectangle preview,
                        const mv::composition::mv_composition& composition,
                        const mv::composition::layer& layer,
                        bool selected,
                        double visual_time_ms,
                        const Texture2D* texture = nullptr,
                        const std::array<float, 256>* waveform_samples = nullptr,
                        const std::array<float, 128>* spectrum = nullptr);

mv_editor_preview_drag_update_result preview_drag_update_result_for(
    const std::string& layer_id,
    mv_preview_drag_mode mode,
    Vector2 mouse,
    Vector2 origin_mouse,
    Rectangle origin_rect);

mv_editor_preview_drag_start_result preview_drag_start_result_for_selected_handles(
    const mv::composition::layer& selected,
    Rectangle selected_bounds,
    Vector2 mouse);

mv_editor_preview_drag_start_result preview_drag_start_result_for_layer_body(
    const mv::composition::layer& candidate,
    Rectangle bounds,
    Vector2 mouse);

mv_editor_preview_drag_end_result preview_drag_end_result_for(bool drag_active,
                                                              bool editable_layer_available);
