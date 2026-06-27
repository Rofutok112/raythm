#pragma once

#include <string>

#include "raylib.h"

enum class mv_editor_context_menu_target {
    none,
    hierarchy,
    project_assets,
    components,
    timeline,
};

enum class mv_editor_context_menu_action {
    none,
    close,
    add_empty_layer,
    add_text_layer,
    add_rect_layer,
    add_image_layer,
    add_beat_grid_layer,
    add_waveform_layer,
    add_spectrum_layer,
    import_image_asset,
    create_script_asset,
    add_component,
    clear_effects,
    delete_layer,
};

struct mv_editor_context_menu_result {
    mv_editor_context_menu_action action = mv_editor_context_menu_action::none;
    std::string component_type;
};

mv_editor_context_menu_result draw_mv_context_menu(mv_editor_context_menu_target target,
                                                   Vector2 position,
                                                   bool has_layer,
                                                   bool has_effects,
                                                   bool opened_this_frame,
                                                   Vector2 mouse);
