#pragma once

#include <algorithm>
#include <string>

#include "raylib.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"

namespace ui {

struct tooltip_config {
    float padding_x = 10.0f;
    float padding_y = 6.0f;
    float offset_x = 14.0f;
    float offset_y = 12.0f;
    int font_size = 12;
    draw_layer target_layer = draw_layer::overlay;
};

inline Rectangle tooltip_rect_for_point(Vector2 point, const char* text, const tooltip_config& config) {
    const Vector2 text_size = measure_text_size(text, static_cast<float>(config.font_size));
    Rectangle rect = {
        point.x + config.offset_x,
        point.y + config.offset_y,
        text_size.x + config.padding_x * 2.0f,
        text_size.y + config.padding_y * 2.0f,
    };
    rect.x = std::clamp(rect.x, 8.0f, static_cast<float>(kScreenWidth) - rect.width - 8.0f);
    rect.y = std::clamp(rect.y, 8.0f, static_cast<float>(kScreenHeight) - rect.height - 8.0f);
    return rect;
}

inline void enqueue_tooltip_at(Vector2 point,
                               const char* text,
                               unsigned char alpha = 255,
                               tooltip_config config = {}) {
    if (text == nullptr || *text == '\0') {
        return;
    }

    const Rectangle rect = tooltip_rect_for_point(point, text, config);
    const std::string text_copy(text);
    const Color background = with_alpha(
        g_theme->panel, static_cast<unsigned char>(235.0f * (static_cast<float>(alpha) / 255.0f)));
    const Color border = with_alpha(g_theme->border_active, alpha);
    const Color text_color = with_alpha(g_theme->text, alpha);
    const int font_size = config.font_size;
    enqueue_draw_command(config.target_layer, [rect, text_copy, background, border, text_color, font_size]() {
        draw_rect_f(rect, background);
        draw_rect_lines(rect, 1.0f, border);
        draw_text_in_rect(text_copy.c_str(), font_size, rect, text_color, text_align::center);
    });
}

inline void enqueue_hover_tooltip(Rectangle hover_rect,
                                  const char* text,
                                  unsigned char alpha = 255,
                                  tooltip_config config = {},
                                  draw_layer hover_layer = draw_layer::base) {
    if (!is_hovered(hover_rect, hover_layer)) {
        return;
    }
    enqueue_tooltip_at(virtual_screen::get_virtual_mouse(), text, alpha, config);
}

}  // namespace ui
