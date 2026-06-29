#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "raylib.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_text.h"
#include "ui_text_input.h"

namespace ui::inspector {

struct row_style {
    float value_label_width = 120.0f;
    float slider_label_width = 92.0f;
    float row_height = 26.0f;
    float slider_row_height = 28.0f;
    float font_size = 11.0f;
    float track_margin_left = 110.0f;
    float track_margin_right = 70.0f;
    float content_padding = 10.0f;
};

struct card_style {
    float header_height = 26.0f;
    float horizontal_padding = 8.0f;
    float bottom_padding = 8.0f;
    float row_gap = 4.0f;
    float remove_width = 60.0f;
    float remove_height = 20.0f;
    float remove_margin = 4.0f;
};

struct component_card_result {
    Rectangle body = {};
    bool remove_clicked = false;
};

struct color_picker_state {
    bool open = false;
    int active_channel = -1;
    bool edit_started = false;
};

struct color_row_result {
    text_input_result input = {};
    bool changed = false;
    bool picker_changed = false;
};

struct field_cursor {
    Rectangle body = {};
    float y = 0.0f;
    row_style rows = {};
    card_style card = {};

    Rectangle row_rect() const {
        return {body.x, y, body.width, rows.row_height};
    }

    void advance() {
        y += rows.row_height + card.row_gap;
    }
};

inline float component_card_height(int field_count,
                                   const row_style& rows = {},
                                   const card_style& card = {}) {
    const int safe_count = field_count < 0 ? 0 : field_count;
    const float fields_height =
        safe_count == 0
            ? 0.0f
            : static_cast<float>(safe_count) * rows.row_height +
                  static_cast<float>(safe_count - 1) * card.row_gap;
    return card.header_height + fields_height + card.bottom_padding;
}

inline float color_picker_height(const row_style& rows = {}) {
    return rows.row_height * 4.0f + 14.0f;
}

inline bool is_hex_color(const std::string& value) {
    if (value.size() != 7 || value[0] != '#') {
        return false;
    }
    for (std::size_t i = 1; i < value.size(); ++i) {
        const char ch = value[i];
        const bool hex = (ch >= '0' && ch <= '9') ||
                         (ch >= 'a' && ch <= 'f') ||
                         (ch >= 'A' && ch <= 'F');
        if (!hex) {
            return false;
        }
    }
    return true;
}

inline int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + ch - 'a';
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + ch - 'A';
    }
    return 0;
}

inline std::string normalize_hex_color(std::string value) {
    if (!is_hex_color(value)) {
        return value;
    }
    for (std::size_t i = 1; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'F') {
            value[i] = static_cast<char>('a' + (value[i] - 'A'));
        }
    }
    return value;
}

inline Color color_from_hex(const std::string& value, Color fallback = WHITE) {
    if (!is_hex_color(value)) {
        return fallback;
    }
    return {
        static_cast<unsigned char>(hex_value(value[1]) * 16 + hex_value(value[2])),
        static_cast<unsigned char>(hex_value(value[3]) * 16 + hex_value(value[4])),
        static_cast<unsigned char>(hex_value(value[5]) * 16 + hex_value(value[6])),
        255
    };
}

inline std::string color_to_hex(Color color) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", color.r, color.g, color.b);
    return buffer;
}

inline bool hex_color_filter(int codepoint, const std::string& current_value) {
    if (current_value.empty()) {
        return codepoint == '#';
    }
    if (current_value.size() >= 7) {
        return false;
    }
    return (codepoint >= '0' && codepoint <= '9') ||
           (codepoint >= 'a' && codepoint <= 'f') ||
           (codepoint >= 'A' && codepoint <= 'F');
}

inline component_card_result draw_component_card(Rectangle rect,
                                                 const std::string& title,
                                                 bool removable,
                                                 const card_style& style = {}) {
    draw_rect_f(rect, with_alpha(g_theme->section, 230));
    draw_rect_lines(rect, 1.0f, g_theme->border_light);
    draw_text_in_rect(title.c_str(), 12,
                      {rect.x + 10.0f, rect.y, rect.width - style.remove_width - 24.0f, style.header_height},
                      g_theme->text, text_align::left);
    component_card_result result;
    result.body = {
        rect.x + style.horizontal_padding,
        rect.y + style.header_height,
        rect.width - style.horizontal_padding * 2.0f,
        std::max(0.0f, rect.height - style.header_height - style.bottom_padding),
    };
    if (removable) {
        const Rectangle remove_btn = {
            rect.x + rect.width - style.remove_width - 10.0f,
            rect.y + style.remove_margin,
            style.remove_width,
            style.remove_height,
        };
        result.remove_clicked = button(remove_btn, "Remove", {
            .font_size = 10,
            .border_width = 1.5f,
        }).clicked;
    }
    return result;
}

inline field_cursor make_field_cursor(Rectangle card_body,
                                      const row_style& rows = {},
                                      const card_style& card = {}) {
    return {card_body, card_body.y, rows, card};
}

inline void draw_value_row(Rectangle body, float y, const char* label, const std::string& value,
                           const row_style& style = {}) {
    const Rectangle row = {body.x, y, body.width, style.row_height};
    draw_rect_f(row, g_theme->section);
    draw_rect_lines(row, 1.0f, g_theme->border_light);
    draw_text_in_rect(label, static_cast<int>(style.font_size),
                      {row.x + 10.0f, row.y, style.value_label_width, row.height},
                      g_theme->text_muted, text_align::left);
    draw_text_in_rect(value.c_str(), static_cast<int>(style.font_size),
                      {row.x + style.value_label_width + 12.0f, row.y,
                       row.width - style.value_label_width - 22.0f, row.height},
                      g_theme->text, text_align::right);
}

inline text_input_result draw_text_row(Rectangle body, float y,
                                       text_input_state& state,
                                       const char* label,
                                       const char* placeholder,
                                       const char* default_value = nullptr,
                                       text_input_filter filter = default_text_input_filter,
                                       const row_style& style = {},
                                       draw_layer layer = draw_layer::base,
                                       size_t max_length = 64) {
    return text_input({body.x, y, body.width, style.row_height}, state, label, placeholder, {
        .default_value = default_value,
        .layer = layer,
        .font_size = static_cast<int>(style.font_size),
        .max_length = max_length,
        .filter = filter,
        .label_width = style.slider_label_width,
    });
}

inline text_input_result draw_number_row(Rectangle body, float y,
                                         text_input_state& state,
                                         const char* label,
                                         const char* placeholder,
                                         text_input_filter filter,
                                         const row_style& style = {},
                                         draw_layer layer = draw_layer::base,
                                         size_t max_length = 16) {
    return draw_text_row(body, y, state, label, placeholder, nullptr, filter, style, layer, max_length);
}

inline color_row_result draw_color_row(Rectangle body, float y,
                                       text_input_state& input_state,
                                       color_picker_state& picker_state,
                                       const char* label = "Color",
                                       const char* default_value = "#ffffff",
                                       const row_style& style = {},
                                       draw_layer layer = draw_layer::base) {
    color_row_result result;
    result.input = draw_text_row(body, y, input_state, label, default_value, default_value,
                                 hex_color_filter, style, layer, 7);
    if (result.input.changed && is_hex_color(input_state.value)) {
        input_state.value = normalize_hex_color(input_state.value);
        result.changed = true;
    }

    const Rectangle row = {body.x, y, body.width, style.row_height};
    const Color current = color_from_hex(input_state.value, color_from_hex(default_value));
    const Rectangle swatch = {
        row.x + row.width - 28.0f,
        row.y + 6.0f,
        16.0f,
        std::max(10.0f, row.height - 12.0f)
    };
    color_swatch(swatch, current, {
        .roundness = 0.0f,
        .border_color = picker_state.open ? g_theme->border_active : g_theme->border_light,
    });
    if (is_clicked(swatch, layer)) {
        picker_state.open = !picker_state.open;
        picker_state.active_channel = -1;
        picker_state.edit_started = false;
        input_state.active = false;
    }

    if (!picker_state.open) {
        return result;
    }

    const Rectangle picker = {
        body.x,
        y + style.row_height + 4.0f,
        body.width,
        color_picker_height(style) - 4.0f
    };
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const std::array<Rectangle, 2> picker_close_exclusions{picker, row};
    if (is_mouse_button_pressed_outside(picker_close_exclusions, mouse)) {
        picker_state.open = false;
        picker_state.active_channel = -1;
        return result;
    }

    draw_rect_f(picker, with_alpha(g_theme->panel, 250));
    draw_rect_lines(picker, 1.0f, g_theme->border_active);
    const Rectangle preview = {picker.x + 8.0f, picker.y + 7.0f, 28.0f, style.row_height - 8.0f};
    draw_rect_f(preview, current);
    draw_rect_lines(preview, 1.0f, g_theme->border_light);
    draw_text_in_rect(input_state.value.c_str(), static_cast<int>(style.font_size),
                      {preview.x + preview.width + 8.0f, picker.y + 2.0f,
                       picker.width - preview.width - 24.0f, style.row_height},
                      g_theme->text, text_align::left);

    Color next = current;
    unsigned char* channels[3] = {&next.r, &next.g, &next.b};
    const char* labels[3] = {"R", "G", "B"};
    const Color fills[3] = {
        {255, 90, 90, 255},
        {90, 220, 120, 255},
        {100, 170, 255, 255},
    };
    if (is_mouse_button_released()) {
        picker_state.active_channel = -1;
    }
    for (int channel = 0; channel < 3; ++channel) {
        const float row_y = picker.y + style.row_height + 6.0f +
                            static_cast<float>(channel) * style.row_height;
        draw_text_in_rect(labels[channel], static_cast<int>(style.font_size),
                          {picker.x + 8.0f, row_y, 18.0f, style.row_height},
                          g_theme->text_muted, text_align::left);
        const Rectangle track = {
            picker.x + 28.0f,
            row_y + style.row_height * 0.5f - 3.0f,
            picker.width - 72.0f,
            6.0f
        };
        const float ratio = static_cast<float>(*channels[channel]) / 255.0f;
        channel_slider_track(track, ratio, fills[channel], picker_state.active_channel == channel, {
            .thumb_width = 6.0f,
            .thumb_height = 14.0f,
            .thumb_top_offset = -4.0f,
        });
        draw_text_in_rect(std::to_string(static_cast<int>(*channels[channel])).c_str(),
                          static_cast<int>(style.font_size),
                          {track.x + track.width + 8.0f, row_y, 30.0f, style.row_height},
                          g_theme->text, text_align::right);
        if (ui::is_mouse_button_pressed(track, MOUSE_BUTTON_LEFT, draw_layer::overlay)) {
            picker_state.active_channel = channel;
            picker_state.edit_started = false;
        }
        if (picker_state.active_channel == channel && is_mouse_button_down()) {
            const float next_ratio = std::clamp((mouse.x - track.x) / track.width, 0.0f, 1.0f);
            const unsigned char next_value = static_cast<unsigned char>(std::round(next_ratio * 255.0f));
            if (*channels[channel] != next_value) {
                *channels[channel] = next_value;
                input_state.value = color_to_hex(next);
                result.changed = true;
                result.picker_changed = true;
            }
        }
    }
    return result;
}

inline float draw_slider_row(Rectangle body, float y, const char* label,
                             const std::string& value, float ratio,
                             const row_style& style = {},
                             draw_layer layer = draw_layer::base) {
    return slider_relative({body.x, y, body.width, style.slider_row_height},
                           label, value.c_str(), ratio,
                           style.track_margin_left, style.track_margin_right, {
        .layer = layer,
        .font_size = static_cast<int>(style.font_size),
        .track_top_offset = style.slider_row_height * 0.5f,
        .label_width = style.slider_label_width,
        .content_padding = style.content_padding,
    });
}

}  // namespace ui::inspector
