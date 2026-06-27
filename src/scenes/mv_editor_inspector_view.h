#pragma once

#include <string>
#include <unordered_map>

#include "mv/composition/mv_composition.h"
#include "raylib.h"
#include "ui_inspector.h"
#include "ui_text_input.h"

struct mv_editor_inspector_layer_name_result {
    bool changed = false;
    bool commit_requested = false;
    std::string name;
};

enum class mv_editor_inspector_transform_field {
    none,
    position_x,
    position_y,
    scale,
};

struct mv_editor_inspector_transform_input_result {
    mv_editor_inspector_transform_field field = mv_editor_inspector_transform_field::none;
    bool changed = false;
    bool finalized = false;
    float value = 0.0f;
    int decimals = 0;
};

struct mv_editor_inspector_transform_opacity_result {
    bool changed = false;
    float opacity = 1.0f;
};

struct mv_editor_inspector_transform_card_result {
    mv_editor_inspector_transform_input_result position_x;
    mv_editor_inspector_transform_input_result position_y;
    mv_editor_inspector_transform_input_result scale;
    mv_editor_inspector_transform_opacity_result opacity;
};

struct mv_editor_inspector_component_result {
    std::string component_id;
    bool text_changed = false;
    std::string text;
    bool fill_changed = false;
    std::string fill;
    bool script_entry_changed = false;
    std::string script_entry;
    bool script_source_changed = false;
    std::string script_source;
    bool edit_pending = false;
    bool commit_requested = false;
    std::string commit_label;
};

struct mv_editor_inspector_component_amount_result {
    std::string component_id;
    bool changed = false;
    float amount = 0.0f;
};

struct mv_editor_inspector_component_remove_result {
    bool requested = false;
    std::string component_id;
};

struct mv_editor_inspector_component_card_result {
    mv_editor_inspector_component_result component;
    mv_editor_inspector_component_amount_result amount;
    mv_editor_inspector_component_remove_result remove;
};

struct mv_editor_inspector_add_component_result {
    bool requested = false;
    Vector2 menu_position = {0.0f, 0.0f};
};

float component_inspector_card_height(
    const mv::composition::component& component,
    const ui::inspector::color_picker_state* color_picker = nullptr);

mv_editor_inspector_layer_name_result draw_mv_inspector_layer_header(Rectangle body,
                                                                     ui::text_input_state& name_input,
                                                                     const std::string& type_label);

mv_editor_inspector_transform_card_result draw_mv_inspector_transform_card(
    Rectangle card,
    const mv::composition::component& transform,
    ui::text_input_state& x_input,
    ui::text_input_state& y_input,
    ui::text_input_state& scale_input);

mv_editor_inspector_component_card_result draw_mv_inspector_component_card(
    Rectangle card,
    const mv::composition::component& component,
    std::unordered_map<std::string, ui::text_input_state>& text_inputs,
    std::unordered_map<std::string, ui::text_input_state>& fill_inputs,
    std::unordered_map<std::string, ui::text_input_state>& script_entry_inputs,
    std::unordered_map<std::string, ui::text_input_state>& script_inputs,
    std::unordered_map<std::string, ui::inspector::color_picker_state>& color_pickers);

mv_editor_inspector_add_component_result draw_mv_inspector_add_component_button(Rectangle button);
