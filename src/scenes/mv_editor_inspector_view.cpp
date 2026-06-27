#include "mv_editor_inspector_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include "mv/composition/mv_component_registry.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_text.h"

namespace {

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

bool signed_number_filter(int codepoint, const std::string& current_value) {
    if (codepoint >= '0' && codepoint <= '9') {
        return true;
    }
    if (codepoint == '.') {
        return current_value.find('.') == std::string::npos;
    }
    if (codepoint == '-') {
        return current_value.empty();
    }
    return false;
}

std::optional<float> parse_float_input(const std::string& value) {
    if (value.empty() || value == "-" || value == "." || value == "-.") {
        return std::nullopt;
    }
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
}

std::string format_float_input(float value, int decimals) {
    char buffer[64];
    if (decimals <= 0) {
        std::snprintf(buffer, sizeof(buffer), "%.0f", value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    }
    return buffer;
}

int hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

bool is_valid_hex_color(const std::string& value) {
    if (value.size() != 7 || value[0] != '#') {
        return false;
    }
    for (std::size_t i = 1; i < value.size(); ++i) {
        if (hex_digit(value[i]) < 0) {
            return false;
        }
    }
    return true;
}

std::string component_display_title(const mv::composition::component& component) {
    const std::string title = mv::composition::display_name_for_component_type(component.type);
    return title.empty() ? std::string{"Component"} : title;
}

mv_editor_inspector_layer_name_result draw_mv_inspector_layer_name(Rectangle rect,
                                                                   ui::text_input_state& input) {
    const auto result = ui::text_input(rect, input, "Name", "Object name", {
        .font_size = 12,
        .max_length = 96,
        .filter = wide_text_filter,
        .label_width = 72.0f,
    });
    return {
        .changed = result.changed,
        .commit_requested = result.deactivated || result.submitted,
        .name = input.value.empty() ? "Object" : input.value,
    };
}

mv_editor_inspector_transform_input_result draw_mv_inspector_transform_input(
    Rectangle body,
    float y,
    ui::text_input_state& input,
    const char* label,
    float current_value,
    int decimals,
    mv_editor_inspector_transform_field field) {
    const auto result =
        ui::inspector::draw_number_row(body,
                                       y,
                                       input,
                                       label,
                                       "0",
                                       signed_number_filter);
    const bool is_scale = field == mv_editor_inspector_transform_field::scale;
    auto normalized_value = [&](float value) {
        value = decimals <= 0 ? std::round(value) : value;
        return is_scale ? std::clamp(value, 0.05f, 8.0f) : value;
    };

    mv_editor_inspector_transform_input_result output;
    output.field = field;
    output.decimals = decimals;

    if (result.changed) {
        if (const std::optional<float> parsed = parse_float_input(input.value)) {
            output.changed = true;
            output.value = normalized_value(*parsed);
        }
    }
    if ((result.deactivated || result.submitted) && !input.value.empty()) {
        output.finalized = true;
        output.value = current_value;
        if (const std::optional<float> parsed = parse_float_input(input.value)) {
            output.value = normalized_value(*parsed);
        }
    }
    return output;
}

mv_editor_inspector_transform_opacity_result draw_mv_inspector_transform_opacity(Rectangle body,
                                                                                float y,
                                                                                float opacity) {
    const float changed = ui::inspector::draw_slider_row(
        body,
        y,
        "Opacity",
        std::to_string(static_cast<int>(std::round(opacity * 100.0f))) + "%",
        opacity);
    return {
        .changed = changed >= 0.0f,
        .opacity = changed >= 0.0f ? changed : opacity,
    };
}

mv_editor_inspector_component_result inspector_text_component_result_for(
    const mv::composition::component& component,
    const ui::text_input_result& text_result,
    const ui::inspector::color_row_result& color_result,
    ui::text_input_state& text_input,
    ui::text_input_state& fill_input) {
    mv_editor_inspector_component_result result;
    result.component_id = component.id;
    result.commit_requested =
        text_result.deactivated || text_result.submitted ||
        color_result.input.deactivated || color_result.input.submitted;
    result.commit_label = "Edit Text";
    if (text_result.changed) {
        result.text_changed = true;
        result.text = text_input.value;
        result.edit_pending = true;
    }
    if (color_result.changed && is_valid_hex_color(fill_input.value)) {
        fill_input.value = ui::inspector::normalize_hex_color(fill_input.value);
        result.fill_changed = true;
        result.fill = fill_input.value;
        result.edit_pending = true;
    }
    return result;
}

mv_editor_inspector_component_result inspector_fill_component_result_for(
    const mv::composition::component& component,
    const ui::inspector::color_row_result& color_result,
    ui::text_input_state& fill_input) {
    mv_editor_inspector_component_result result;
    result.component_id = component.id;
    result.commit_requested = color_result.input.deactivated || color_result.input.submitted;
    result.commit_label = "Edit Color";
    if (color_result.changed && is_valid_hex_color(fill_input.value)) {
        fill_input.value = ui::inspector::normalize_hex_color(fill_input.value);
        result.fill_changed = true;
        result.fill = fill_input.value;
        result.edit_pending = true;
    }
    return result;
}

mv_editor_inspector_component_result inspector_lua_component_result_for(
    const mv::composition::component& component,
    const ui::text_input_result& entry_result,
    const ui::text_input_result& script_result,
    ui::text_input_state& entry_input,
    ui::text_input_state& script_input) {
    mv_editor_inspector_component_result result;
    result.component_id = component.id;
    result.commit_requested =
        entry_result.deactivated || entry_result.submitted ||
        script_result.deactivated || script_result.submitted;
    result.commit_label = "Edit Lua Behaviour";
    if (entry_result.changed) {
        result.script_entry_changed = true;
        result.script_entry = entry_input.value.empty() ? "update" : entry_input.value;
        result.edit_pending = true;
    }
    if (script_result.changed) {
        result.script_source_changed = true;
        result.script_source = script_input.value;
        result.edit_pending = true;
    }
    return result;
}

mv_editor_inspector_component_amount_result inspector_amount_component_result_for(
    const mv::composition::component& component,
    float slider_value) {
    if (slider_value < 0.0f) {
        return {};
    }

    mv_editor_inspector_component_amount_result result;
    result.component_id = component.id;
    result.changed = true;
    if (component.type == "Fade") {
        result.amount = 100.0f + slider_value * 1900.0f;
    } else if (component.type == "Pulse") {
        result.amount = slider_value * 0.3f;
    } else if (component.type == "Flash") {
        result.amount = slider_value;
    } else if (component.type == "Shake") {
        result.amount = slider_value * 120.0f;
    } else {
        result.changed = false;
    }
    return result;
}

std::string ms_label(double value_ms) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2fs", value_ms / 1000.0);
    return buffer;
}

}  // namespace

float component_inspector_card_height(
    const mv::composition::component& component,
    const ui::inspector::color_picker_state* color_picker) {
    const float color_picker_extra =
        color_picker != nullptr && color_picker->open
            ? ui::inspector::color_picker_height() + ui::inspector::card_style{}.row_gap
            : 0.0f;
    if (component.type == "transform") {
        return ui::inspector::component_card_height(4);
    }
    if (component.type == "TextRenderer") {
        return ui::inspector::component_card_height(2) + color_picker_extra;
    }
    if (component.type == "SpectrumRenderer") {
        return ui::inspector::component_card_height(2) + color_picker_extra;
    }
    if (component.type == "ShapeRenderer" ||
        component.type == "BackgroundRenderer" ||
        component.type == "BeatGridRenderer" ||
        component.type == "WaveformRenderer") {
        return ui::inspector::component_card_height(1) + color_picker_extra;
    }
    if (component.type == "ImageRenderer" ||
        component.type == "Fade" ||
        component.type == "Pulse" ||
        component.type == "Flash" ||
        component.type == "Shake") {
        return ui::inspector::component_card_height(1);
    }
    if (component.type == "LuaBehaviour") {
        return ui::inspector::component_card_height(3);
    }
    return ui::inspector::component_card_height(1);
}

mv_editor_inspector_layer_name_result draw_mv_inspector_layer_header(Rectangle body,
                                                                     ui::text_input_state& name_input,
                                                                     const std::string& type_label) {
    mv_editor_inspector_layer_name_result result =
        draw_mv_inspector_layer_name({body.x, body.y, body.width, 30.0f}, name_input);
    ui::draw_text_in_rect(type_label.c_str(), 11, {body.x, body.y + 31.0f, body.width, 18.0f},
                          g_theme->text_muted, ui::text_align::left);
    return result;
}

mv_editor_inspector_transform_card_result draw_mv_inspector_transform_card(
    Rectangle card,
    const mv::composition::component& transform,
    ui::text_input_state& x_input,
    ui::text_input_state& y_input,
    ui::text_input_state& scale_input) {
    const ui::inspector::component_card_result transform_card =
        ui::inspector::draw_component_card(card, "Transform", false);
    ui::inspector::field_cursor field_cursor =
        ui::inspector::make_field_cursor(transform_card.body);

    if (!x_input.active) {
        x_input.value = format_float_input(transform.position_x, 0);
    }
    if (!y_input.active) {
        y_input.value = format_float_input(transform.position_y, 0);
    }
    if (!scale_input.active) {
        scale_input.value = format_float_input(transform.scale_x, 2);
    }

    mv_editor_inspector_transform_card_result result;
    result.position_x = draw_mv_inspector_transform_input(field_cursor.body,
                                                         field_cursor.y,
                                                         x_input,
                                                         "X",
                                                         transform.position_x,
                                                         0,
                                                         mv_editor_inspector_transform_field::position_x);
    field_cursor.advance();
    result.position_y = draw_mv_inspector_transform_input(field_cursor.body,
                                                         field_cursor.y,
                                                         y_input,
                                                         "Y",
                                                         transform.position_y,
                                                         0,
                                                         mv_editor_inspector_transform_field::position_y);
    field_cursor.advance();
    result.scale = draw_mv_inspector_transform_input(field_cursor.body,
                                                    field_cursor.y,
                                                    scale_input,
                                                    "Scale",
                                                    transform.scale_x,
                                                    2,
                                                    mv_editor_inspector_transform_field::scale);
    field_cursor.advance();
    result.opacity = draw_mv_inspector_transform_opacity(field_cursor.body,
                                                        field_cursor.y,
                                                        transform.opacity);
    return result;
}

mv_editor_inspector_component_card_result draw_mv_inspector_component_card(
    Rectangle card,
    const mv::composition::component& component,
    std::unordered_map<std::string, ui::text_input_state>& text_inputs,
    std::unordered_map<std::string, ui::text_input_state>& fill_inputs,
    std::unordered_map<std::string, ui::text_input_state>& script_entry_inputs,
    std::unordered_map<std::string, ui::text_input_state>& script_inputs,
    std::unordered_map<std::string, ui::inspector::color_picker_state>& color_pickers) {
    mv_editor_inspector_component_card_result result;
    const ui::inspector::component_card_result component_card =
        ui::inspector::draw_component_card(card, component_display_title(component), true);
    if (component_card.remove_clicked) {
        result.remove = {
            .requested = true,
            .component_id = component.id,
        };
    }

    ui::inspector::field_cursor field_cursor =
        ui::inspector::make_field_cursor(component_card.body);
    if (component.type == "TextRenderer") {
        ui::text_input_state& text_input = text_inputs[component.id];
        ui::text_input_state& fill_input = fill_inputs[component.id];
        if (text_input.value.empty() && !component.text.empty()) {
            text_input.value = component.text;
        }
        if (fill_input.value.empty()) {
            fill_input.value = component.fill.empty() ? "#ffffff" : component.fill;
        }
        const auto text_result =
            ui::inspector::draw_text_row(field_cursor.body,
                                         field_cursor.y,
                                         text_input,
                                         "Text",
                                         "Text",
                                         "Text",
                                         wide_text_filter,
                                         {},
                                         ui::draw_layer::base,
                                         160);
        field_cursor.advance();
        ui::inspector::color_picker_state& color_picker = color_pickers[component.id];
        const auto color_result =
            ui::inspector::draw_color_row(field_cursor.body,
                                          field_cursor.y,
                                          fill_input,
                                          color_picker);
        result.component = inspector_text_component_result_for(
            component,
            text_result,
            color_result,
            text_input,
            fill_input);
    } else if (component.type == "ShapeRenderer" || component.type == "BackgroundRenderer" ||
               component.type == "BeatGridRenderer" || component.type == "WaveformRenderer" ||
               component.type == "SpectrumRenderer") {
        ui::text_input_state& fill_input = fill_inputs[component.id];
        if (fill_input.value.empty()) {
            fill_input.value = component.fill.empty() ? "#ffffff" : component.fill;
        }
        ui::inspector::color_picker_state& color_picker = color_pickers[component.id];
        const auto color_result =
            ui::inspector::draw_color_row(field_cursor.body,
                                          field_cursor.y,
                                          fill_input,
                                          color_picker);
        result.component = inspector_fill_component_result_for(component, color_result, fill_input);
        if (component.type == "SpectrumRenderer") {
            field_cursor.advance();
            if (color_picker.open) {
                field_cursor.y += ui::inspector::color_picker_height() +
                                  ui::inspector::card_style{}.row_gap;
            }
            ui::inspector::draw_value_row(field_cursor.body, field_cursor.y, "Style",
                                          component.shape.empty() ? "bars" : component.shape);
        }
    } else if (component.type == "ImageRenderer") {
        ui::inspector::draw_value_row(field_cursor.body, field_cursor.y, "Asset", component.asset_id);
    } else if (component.type == "Fade") {
        const float amount = component.amount <= 0.0f ? 650.0f : component.amount;
        const float changed = ui::inspector::draw_slider_row(
            field_cursor.body, field_cursor.y, "Amount", ms_label(amount),
            std::clamp((amount - 100.0f) / 1900.0f, 0.0f, 1.0f));
        result.amount = inspector_amount_component_result_for(component, changed);
    } else if (component.type == "Pulse") {
        const float amount = std::clamp(component.amount <= 0.0f ? 0.08f : component.amount, 0.0f, 0.3f);
        const std::string pulse_label =
            std::to_string(static_cast<int>(std::round(amount * 100.0f))) + "%";
        const float changed =
            ui::inspector::draw_slider_row(field_cursor.body, field_cursor.y, "Amount", pulse_label,
                                           std::clamp(amount / 0.3f, 0.0f, 1.0f));
        result.amount = inspector_amount_component_result_for(component, changed);
    } else if (component.type == "Flash") {
        const float amount = std::clamp(component.amount <= 0.0f ? 0.35f : component.amount, 0.0f, 1.0f);
        const std::string flash_label =
            std::to_string(static_cast<int>(std::round(amount * 100.0f))) + "%";
        const float changed =
            ui::inspector::draw_slider_row(field_cursor.body, field_cursor.y, "Amount", flash_label, amount);
        result.amount = inspector_amount_component_result_for(component, changed);
    } else if (component.type == "Shake") {
        const float amount = std::clamp(component.amount <= 0.0f ? 18.0f : component.amount, 0.0f, 120.0f);
        const std::string shake_label =
            std::to_string(static_cast<int>(std::round(amount))) + "px";
        const float changed =
            ui::inspector::draw_slider_row(field_cursor.body, field_cursor.y, "Amount", shake_label, amount / 120.0f);
        result.amount = inspector_amount_component_result_for(component, changed);
    } else if (component.type == "LuaBehaviour") {
        ui::text_input_state& entry_input = script_entry_inputs[component.id];
        ui::text_input_state& script_input = script_inputs[component.id];
        if (entry_input.value.empty()) {
            entry_input.value = component.script_entry.empty() ? "update" : component.script_entry;
        }
        if (script_input.value.empty()) {
            script_input.value = component.script_source.empty()
                ? "function update(self, ctx) end"
                : component.script_source;
        }
        ui::inspector::draw_value_row(field_cursor.body, field_cursor.y, "Asset",
                                      component.script_asset_id.empty()
                                          ? "Inline"
                                          : component.script_asset_id);
        field_cursor.advance();
        const auto entry_result =
            ui::inspector::draw_text_row(field_cursor.body,
                                         field_cursor.y,
                                         entry_input,
                                         "Entry",
                                         "update",
                                         "update",
                                         wide_text_filter,
                                         {},
                                         ui::draw_layer::base,
                                         64);
        field_cursor.advance();
        const auto script_result =
            ui::inspector::draw_text_row(field_cursor.body,
                                         field_cursor.y,
                                         script_input,
                                         "Script",
                                         "function update(self, ctx) end",
                                         "function update(self, ctx) end",
                                         wide_text_filter,
                                         {},
                                         ui::draw_layer::base,
                                         512);
        result.component = inspector_lua_component_result_for(
            component,
            entry_result,
            script_result,
            entry_input,
            script_input);
    }

    return result;
}

mv_editor_inspector_add_component_result draw_mv_inspector_add_component_button(Rectangle button) {
    if (ui::button(button, "Add Component", {
            .font_size = 11,
            .border_width = 1.5f,
        }).clicked) {
        return {
            .requested = true,
            .menu_position = {button.x, button.y + button.height + 4.0f},
        };
    }
    return {};
}
