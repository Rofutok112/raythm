#pragma once

#include <cstddef>
#include <cmath>
#include <string>

#include "ui_draw.h"

namespace ui {

using text_input_filter = bool (*)(int codepoint, const std::string& current_value);

struct text_input_state {
    std::string value;
    bool active = false;
};

struct text_input_result {
    bool clicked = false;
    bool changed = false;
    bool submitted = false;
    bool activated = false;
    bool deactivated = false;
    bool defaulted = false;
};

inline bool default_text_input_filter(int codepoint, const std::string&) {
    return codepoint >= 32 && codepoint <= 126;
}

inline text_input_result draw_text_input(Rectangle rect, text_input_state& state,
                                         const char* label, const char* placeholder,
                                         const char* default_value = nullptr,
                                         draw_layer layer = draw_layer::base,
                                         int font_size = 16, size_t max_length = 32,
                                         text_input_filter filter = default_text_input_filter,
                                         float label_width = 84.0f) {
    text_input_result result;

    const auto apply_default_if_empty = [&]() {
        if (default_value != nullptr && state.value.empty()) {
            state.value = default_value;
            result.changed = true;
            result.defaulted = true;
        }
    };

    const bool hovered = is_hovered(rect, layer);
    const bool pressed = is_pressed(rect, layer);
    const bool clicked = is_clicked(rect, layer);
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    detail::draw_row_visual(rect, hovered, pressed,
                            state.active ? g_theme->row_selected : g_theme->row,
                            state.active ? g_theme->row_selected_hover : g_theme->row_hover,
                            state.active ? g_theme->border_active : g_theme->border,
                            1.5f);

    if (clicked) {
        result.clicked = true;
        if (!state.active) {
            result.activated = true;
        }
        state.active = true;
    } else if (state.active && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !hovered) {
        apply_default_if_empty();
        state.active = false;
        result.deactivated = true;
    }

    if (state.active) {
        int codepoint = GetCharPressed();
        while (codepoint > 0) {
            if (state.value.size() < max_length &&
                (filter == nullptr || filter(codepoint, state.value))) {
                state.value.push_back(static_cast<char>(codepoint));
                result.changed = true;
            }
            codepoint = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && !state.value.empty()) {
            state.value.pop_back();
            result.changed = true;
        }

        if (IsKeyPressed(KEY_ENTER)) {
            apply_default_if_empty();
            result.submitted = true;
            state.active = false;
            result.deactivated = true;
        }
    }

    const Rectangle content_rect = inset(visual, edge_insets::symmetric(0.0f, 12.0f));
    const Rectangle label_rect = {content_rect.x, content_rect.y, label_width, content_rect.height};
    const Rectangle input_rect = {
        content_rect.x + label_width,
        content_rect.y + 4.0f,
        content_rect.width - label_width,
        content_rect.height - 8.0f
    };

    DrawRectangleRec(input_rect, state.active ? with_alpha(g_theme->panel, 255) : with_alpha(g_theme->section, 255));
    DrawRectangleLinesEx(input_rect, 1.5f, state.active ? g_theme->border_active : g_theme->border_light);
    draw_text_in_rect(label, font_size, label_rect,
                      state.active ? g_theme->text : g_theme->text_secondary, text_align::left);

    std::string display_value = state.value;
    if (display_value.empty() && !state.active && placeholder != nullptr) {
        display_value = placeholder;
    }
    if (state.active && (GetTime() * 2.0 - std::floor(GetTime() * 2.0)) < 0.5) {
        display_value += "_";
    }

    const Rectangle text_rect = {
        input_rect.x + 10.0f,
        input_rect.y,
        std::max(0.0f, input_rect.width - 20.0f + 4.0f),
        input_rect.height
    };
    const Rectangle shifted_text_rect = {text_rect.x, text_rect.y + 1.5f, text_rect.width, text_rect.height};
    const Color text_color = state.value.empty() && !state.active ? g_theme->text_hint : g_theme->text;
    if (!state.active && !state.value.empty()) {
        const float marquee_y = shifted_text_rect.y + (shifted_text_rect.height - static_cast<float>(font_size)) * 0.5f;
        draw_marquee_text(display_value.c_str(), shifted_text_rect.x, marquee_y, font_size, text_color,
                          shifted_text_rect.width, GetTime());
    } else {
        draw_text_in_rect(display_value.c_str(), font_size, shifted_text_rect, text_color, text_align::left);
    }

    return result;
}

}  // namespace ui
