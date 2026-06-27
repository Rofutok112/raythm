#pragma once

#include <algorithm>

#include "raylib.h"
#include "theme.h"
#include "ui_draw.h"

namespace song_select::filter_widget {

struct option_button_options {
    int font_size = 11;
    float border_width = 1.0f;
    bool selected = false;
    bool interactive = true;
    Color base = {};
    Color hover = {};
    Color selected_base = {};
    Color selected_hover = {};
    Color text = {};
    Color selected_text = {};
    unsigned char normal_alpha = 255;
    unsigned char hover_alpha = 255;
    unsigned char selected_alpha = 255;
    unsigned char selected_hover_alpha = 255;
    unsigned char text_alpha = 255;
};

inline ui::button_state option_button(Rectangle rect, const char* label, option_button_options options = {}) {
    const Color base = options.base.a > 0 ? options.base : g_theme->row;
    const Color hover = options.hover.a > 0 ? options.hover : g_theme->row_hover;
    const Color selected_base = options.selected_base.a > 0 ? options.selected_base : g_theme->row_selected;
    const Color selected_hover = options.selected_hover.a > 0 ? options.selected_hover : selected_base;
    const Color text = options.text.a > 0 ? options.text : g_theme->text_secondary;
    const Color selected_text = options.selected_text.a > 0 ? options.selected_text : g_theme->text;

    return ui::button(rect, label, {
        .font_size = options.font_size,
        .border_width = options.border_width,
        .bg = with_alpha(options.selected ? selected_base : base,
                         options.selected ? options.selected_alpha : options.normal_alpha),
        .bg_hover = with_alpha(options.selected ? selected_hover : hover,
                               options.selected ? options.selected_hover_alpha : options.hover_alpha),
        .text_color = with_alpha(options.selected ? selected_text : text, options.text_alpha),
        .custom_colors = true,
        .interactive = options.interactive,
    });
}

struct filter_icon_button_options {
    ui::draw_layer layer = ui::draw_layer::base;
    float border_width = 1.2f;
    bool active = false;
    bool interactive = true;
    Color base = {};
    Color hover = {};
    Color active_base = {};
    Color active_hover = {};
    Color border = {};
    Color active_border = {};
    Color icon = {};
    Color active_icon = {};
    unsigned char normal_alpha = 255;
    unsigned char hover_alpha = 255;
    unsigned char active_alpha = 255;
    unsigned char active_hover_alpha = 255;
    unsigned char icon_alpha = 255;
};

inline void draw_filter_icon(Rectangle rect, Color color, float stroke_width) {
    const float center_x = rect.x + rect.width * 0.5f;
    const float line_half_width = std::max(1.0f, rect.width * 0.46f);
    const float knob_size = std::max(3.0f, rect.width * 0.16f);
    const float knob_half = knob_size * 0.5f;
    const float y_positions[] = {
        rect.y + rect.height * 0.18f,
        rect.y + rect.height * 0.50f,
        rect.y + rect.height * 0.82f,
    };
    const float knob_offsets[] = {
        -rect.width * 0.17f,
        rect.width * 0.25f,
        -rect.width * 0.33f,
    };

    for (int i = 0; i < 3; ++i) {
        const float y = y_positions[i];
        ui::draw_line_ex({center_x - line_half_width, y}, {center_x + line_half_width, y}, stroke_width, color);
        const float knob_x = center_x + knob_offsets[i];
        ui::surface_fill({knob_x - knob_half, y - knob_half, knob_size, knob_size}, color);
    }
}

inline ui::button_state filter_icon_button(Rectangle rect, filter_icon_button_options options = {}) {
    const Color base = options.base.a > 0 ? options.base : g_theme->row;
    const Color hover = options.hover.a > 0 ? options.hover : base;
    const Color active_base = options.active_base.a > 0 ? options.active_base : g_theme->row_selected;
    const Color active_hover = options.active_hover.a > 0 ? options.active_hover : active_base;
    const Color border = options.border.a > 0 ? options.border : g_theme->border_light;
    const Color active_border = options.active_border.a > 0 ? options.active_border : g_theme->accent;
    const Color icon = options.icon.a > 0 ? options.icon : g_theme->text_secondary;
    const Color active_icon = options.active_icon.a > 0 ? options.active_icon : g_theme->accent;

    return ui::icon_button(rect, draw_filter_icon, {
        .layer = options.layer,
        .border_width = options.border_width,
        .enabled = options.interactive,
        .bg = with_alpha(options.active ? active_base : base,
                         options.active ? options.active_alpha : options.normal_alpha),
        .bg_hover = with_alpha(options.active ? active_hover : hover,
                               options.active ? options.active_hover_alpha : options.hover_alpha),
        .icon_color = with_alpha(options.active ? active_icon : icon, options.icon_alpha),
        .icon_hover_color = with_alpha(active_icon, options.icon_alpha),
        .border_color = with_alpha(options.active ? active_border : border, options.icon_alpha),
        .border_hover_color = with_alpha(active_border, options.icon_alpha),
        .disabled_bg = with_alpha(base, options.normal_alpha),
        .disabled_bg_hover = with_alpha(base, options.normal_alpha),
        .disabled_icon_color = with_alpha(icon, options.icon_alpha),
        .disabled_border_color = with_alpha(border, options.icon_alpha),
        .icon_inset = 8.0f,
        .icon_stroke_width = 1.6f,
        .pressed_inset = 0.0f,
        .border_alpha_tracks_fill = false,
    });
}

}  // namespace song_select::filter_widget
