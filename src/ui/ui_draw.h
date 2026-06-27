#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"
#include "ui_coord.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_hit.h"
#include "ui_layout.h"
#include "ui_text.h"

// 繰り返し使用される UI 描画パターンのユーティリティ。
// theme.h の g_theme を参照するため、テーマ設定後に使用すること。
namespace ui {

struct draw_command {
    int layer = 0;
    std::uint64_t order = 0;
    std::function<void()> draw;
};

inline std::vector<draw_command>& draw_queue() {
    static std::vector<draw_command> queue;
    return queue;
}

inline std::uint64_t& draw_queue_order() {
    static std::uint64_t order = 0;
    return order;
}

// 即時描画と併存する frame-local な UI 描画キュー。
// overlay から段階導入し、将来的には hit test 優先順位も同じ layer に揃える前提。
inline void begin_draw_queue() {
    draw_queue().clear();
    draw_queue_order() = 0;
}

inline void enqueue_draw_command(draw_layer layer, std::function<void()> draw) {
    draw_queue().push_back({static_cast<int>(layer), draw_queue_order()++, std::move(draw)});
}

template <typename Fn>
inline void enqueue_draw_command(draw_layer layer, Fn&& draw) {
    enqueue_draw_command(layer, std::function<void()>(std::forward<Fn>(draw)));
}

inline void flush_draw_queue() {
    auto& queue = draw_queue();
    std::stable_sort(queue.begin(), queue.end(), [](const draw_command& left, const draw_command& right) {
        if (left.layer != right.layer) {
            return left.layer < right.layer;
        }
        return left.order < right.order;
    });

    for (const draw_command& command : queue) {
        command.draw();
    }
    queue.clear();
}

namespace detail {

inline void draw_button_visual(Rectangle rect, bool hovered, bool pressed, const char* label,
                               int font_size, Color bg, Color bg_hover, Color text_color,
                               float border_width, Color border_color = {}) {
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    const Color fill = lerp_color(bg, bg_hover, hovered ? 1.0f : 0.0f);
    const Color stroke = border_color.a > 0 ? border_color : g_theme->border;
    draw_rect_f(visual, fill);
    draw_rect_lines(visual, border_width, with_alpha(stroke, fill.a));
    draw_text_in_rect(label, font_size, visual, text_color);
}

inline void draw_row_visual(Rectangle rect, bool hovered, bool pressed, Color bg, Color bg_hover,
                            Color border_color, float border_width) {
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    draw_rect_f(visual, lerp_color(bg, bg_hover, hovered ? 1.0f : 0.0f));
    draw_rect_lines(visual, border_width, border_color);
}

}  // namespace detail

// ── ボタン ──────────────────────────────────────────────

// ボタンの描画状態。button が返し、呼び出し側でクリック判定に使用する。
struct button_state {
    bool hovered;
    bool pressed;
    bool clicked;
};

struct row_state {
    bool hovered;
    bool pressed;
    bool clicked;
    Rectangle visual;
};

struct selector_state {
    row_state row;
    button_state left;
    button_state right;
};

struct value_selector_layout {
    Rectangle label_rect;
    Rectangle value_rect;
    Rectangle left_button_rect;
    Rectangle right_button_rect;
};

struct dropdown_state {
    row_state trigger;
    int clicked_index;
};

struct context_menu_item {
    const char* label;
    bool enabled;
    enum class kind {
        action,
        header,
        separator,
    } item_kind = kind::action;
};

struct context_menu_state {
    int clicked_index;
};

struct slider_layout {
    Rectangle label_rect;
    Rectangle track_rect;
    Rectangle value_rect;
};

enum class tab_button_style {
    raised,
    underline,
};

struct scrollbar_interaction {
    float scroll_offset;
    bool changed;
    bool dragging;
};

struct button_options {
    draw_layer layer = draw_layer::base;
    int font_size = 24;
    float border_width = 2.0f;
    Color bg = {};
    Color bg_hover = {};
    Color text_color = {};
    bool custom_colors = false;
    bool interactive = true;
    Color border_color = {};
    bool custom_border = false;
};

struct row_action_layout_options {
    float button_width = 92.0f;
    float button_height = 38.0f;
    float button_gap = 12.0f;
    float right_padding = 14.0f;
};

struct action_button_options {
    draw_layer layer = draw_layer::base;
    int font_size = 16;
    float border_width = 1.5f;
    bool enabled = true;
    Color bg = {};
    Color bg_hover = {};
    Color text_color = {};
    Color border_color = {};
    Color disabled_bg = {};
    Color disabled_bg_hover = {};
    Color disabled_text_color = {};
    Color disabled_border_color = {};
};

struct toned_action_button_options {
    draw_layer layer = draw_layer::base;
    int font_size = 16;
    float border_width = 1.5f;
    bool enabled = true;
    float bg_mix = 0.08f;
    float bg_hover_mix = 0.16f;
    float border_mix = 0.35f;
    unsigned char bg_alpha = 220;
    unsigned char bg_hover_alpha = 220;
    unsigned char border_alpha = 220;
    unsigned char disabled_bg_alpha = 95;
    unsigned char disabled_border_alpha = 115;
    Color text_color = {};
    Color disabled_text_color = {};
    Color bg_base = {};
    Color bg_hover_base = {};
    Color border_base = {};
};

using icon_draw_fn = void (*)(Rectangle, Color, float);

struct icon_button_options {
    draw_layer layer = draw_layer::base;
    float border_width = 1.5f;
    bool enabled = true;
    Color bg = {};
    Color bg_hover = {};
    Color icon_color = {};
    Color icon_hover_color = {};
    Color border_color = {};
    Color border_hover_color = {};
    Color disabled_bg = {};
    Color disabled_bg_hover = {};
    Color disabled_icon_color = {};
    Color disabled_border_color = {};
    float icon_inset = 9.0f;
    float icon_stroke_width = 3.0f;
    float pressed_inset = 1.5f;
    float icon_pressed_inset = -1.0f;
    bool border_alpha_tracks_fill = true;
};

struct tab_button_options {
    draw_layer layer = draw_layer::base;
    int font_size = 16;
    bool selected = false;
    tab_button_style style = tab_button_style::raised;
    float border_width = 1.5f;
    float selected_border_width = 2.0f;
    float underline_height = 3.0f;
    float underline_inset_x = 18.0f;
    bool interactive = true;
    Color bg = {};
    Color bg_hover = {};
    Color bg_selected = {};
    Color bg_selected_hover = {};
    Color border = {};
    Color border_selected = {};
    Color text_color = {};
    Color selected_text_color = {};
    Color underline_color = {};
    bool custom_colors = false;
};

struct row_options {
    draw_layer layer = draw_layer::base;
    float border_width = 2.0f;
    Color bg = {};
    Color bg_hover = {};
    Color border_color = {};
    bool custom_colors = false;
};

struct hover_surface_options {
    draw_layer layer = draw_layer::base;
    float border_width = 1.5f;
    Color fill = {};
    Color fill_hover = {};
    Color border_color = {};
    bool custom_colors = false;
    bool interactive = true;
    float pressed_inset = 0.0f;
};

struct toggle_row_options {
    draw_layer layer = draw_layer::base;
    int label_font_size = 18;
    int description_font_size = 12;
    float border_width = 1.0f;
    float checked_border_width = 1.6f;
    Color bg = {};
    Color bg_hover = {};
    Color border_color = {};
    Color checked_border_color = {};
    Color label_color = {};
    Color description_color = {};
    Color switch_bg = {};
    Color switch_border_color = {};
    Color knob_color = {};
    bool custom_colors = false;
    float text_left_padding = 14.0f;
    float label_top_padding = 5.0f;
    float description_top_padding = 29.0f;
    float text_right_reserved = 100.0f;
    float switch_width = 48.0f;
    float switch_height = 20.0f;
    float switch_right_inset = 22.0f;
    float knob_size = 16.0f;
    float pressed_inset = 1.5f;
};

struct surface_options {
    float border_width = 0.0f;
    Color fill = {};
    Color border_color = {};
    bool custom_colors = false;
};

struct progress_bar_options {
    Color bg = {};
    Color fill = {};
    Color border_color = {};
    float border_width = 1.0f;
    float fill_inset = 0.0f;
    bool custom_colors = false;
};

struct placeholder_options {
    int font_size = 16;
    bool body_text = false;
    text_align align = text_align::center;
    float border_width = 1.0f;
    bool draw_border = true;
    Color fill = {};
    Color border_color = {};
    Color text_color = {};
    bool custom_colors = false;
};

struct value_selector_options {
    draw_layer layer = draw_layer::base;
    int font_size = 24;
    float button_size = 34.0f;
    float button_gap = 10.0f;
    float label_width = 200.0f;
    float content_padding = 18.0f;
};

struct slider_options {
    draw_layer layer = draw_layer::base;
    int font_size = 22;
    float track_top_offset = 26.0f;
    float label_width = 200.0f;
    float content_padding = 18.0f;
    bool drag_blocked_by_layer = true;
};

struct dropdown_options {
    draw_layer trigger_layer = draw_layer::base;
    draw_layer menu_layer = draw_layer::overlay;
    int font_size = 18;
    float label_width = 72.0f;
    float content_padding = 12.0f;
    float item_height = 30.0f;
    float item_spacing = 4.0f;
};

struct context_menu_options {
    draw_layer layer = draw_layer::overlay;
    int font_size = 16;
    float item_height = 30.0f;
    float item_spacing = 4.0f;
};

struct scrollbar_options {
    draw_layer layer = draw_layer::base;
    Color track_color = {};
    Color thumb_color = {};
    float min_thumb_height = 36.0f;
    bool custom_colors = false;
    bool drag_blocked_by_layer = true;
};

inline Rectangle row_action_rect(Rectangle row, int slot_from_right,
                                 row_action_layout_options options = {}) {
    const int slot = std::max(0, slot_from_right);
    const float slot_offset = static_cast<float>(slot) * (options.button_width + options.button_gap);
    return {
        row.x + row.width - options.right_padding - options.button_width - slot_offset,
        row.y + (row.height - options.button_height) * 0.5f,
        options.button_width,
        options.button_height,
    };
}

inline button_state button(Rectangle rect, const char* label, button_options options = {}) {
    const bool hovered = options.interactive && is_hovered(rect, options.layer);
    const bool pressed = options.interactive && is_pressed(rect, options.layer);
    const bool clicked = options.interactive && is_clicked(rect, options.layer);
    const Color bg = options.custom_colors ? options.bg : g_theme->row;
    const Color bg_hover = options.custom_colors ? options.bg_hover : g_theme->row_hover;
    const Color text_color = options.custom_colors ? options.text_color : g_theme->text;
    const Color border_color = options.custom_border ? options.border_color : Color{};
    detail::draw_button_visual(rect, hovered, pressed, label != nullptr ? label : "",
                               options.font_size, bg, bg_hover, text_color, options.border_width, border_color);
    return {hovered, pressed, clicked};
}

inline button_state row_action_button(Rectangle row,
                                      int slot_from_right,
                                      const char* label,
                                      button_options button_options = {},
                                      row_action_layout_options layout_options = {}) {
    return button(row_action_rect(row, slot_from_right, layout_options), label, button_options);
}

inline button_options action_button_to_button_options(action_button_options options) {
    const auto choose_color = [](Color supplied, Color fallback) {
        return supplied.a > 0 ? supplied : fallback;
    };
    return {
        .layer = options.layer,
        .font_size = options.font_size,
        .border_width = options.border_width,
        .bg = options.enabled
            ? choose_color(options.bg, g_theme->row)
            : choose_color(options.disabled_bg, g_theme->section),
        .bg_hover = options.enabled
            ? choose_color(options.bg_hover, g_theme->row_hover)
            : choose_color(options.disabled_bg_hover, g_theme->section),
        .text_color = options.enabled
            ? choose_color(options.text_color, g_theme->text)
            : choose_color(options.disabled_text_color, g_theme->text_muted),
        .custom_colors = true,
        .interactive = options.enabled,
        .border_color = options.enabled
            ? choose_color(options.border_color, g_theme->border)
            : choose_color(options.disabled_border_color, g_theme->border_light),
        .custom_border = true,
    };
}

inline button_state action_button(Rectangle rect, const char* label, action_button_options options = {}) {
    return button(rect, label, action_button_to_button_options(options));
}

inline action_button_options toned_action_button_to_action_button_options(Color tone,
                                                                          toned_action_button_options options = {}) {
    const Color bg_base = options.bg_base.a > 0 ? options.bg_base : g_theme->row;
    const Color bg_hover_base = options.bg_hover_base.a > 0 ? options.bg_hover_base : g_theme->row_hover;
    const Color border_base = options.border_base.a > 0 ? options.border_base : g_theme->border;
    const Color bg = with_alpha(lerp_color(bg_base, tone, options.bg_mix), options.bg_alpha);
    const Color bg_hover = with_alpha(lerp_color(bg_hover_base, tone, options.bg_hover_mix), options.bg_hover_alpha);
    const Color border = with_alpha(lerp_color(border_base, tone, options.border_mix), options.border_alpha);
    return {
        .layer = options.layer,
        .font_size = options.font_size,
        .border_width = options.border_width,
        .enabled = options.enabled,
        .bg = bg,
        .bg_hover = bg_hover,
        .text_color = options.text_color.a > 0 ? options.text_color : g_theme->text,
        .border_color = border,
        .disabled_bg = with_alpha(bg, options.disabled_bg_alpha),
        .disabled_bg_hover = with_alpha(bg, options.disabled_bg_alpha),
        .disabled_text_color = options.disabled_text_color.a > 0 ? options.disabled_text_color : g_theme->text_muted,
        .disabled_border_color = with_alpha(border, options.disabled_border_alpha),
    };
}

inline button_state toned_action_button(Rectangle rect, const char* label, Color tone,
                                        toned_action_button_options options = {}) {
    return action_button(rect, label, toned_action_button_to_action_button_options(tone, options));
}

inline button_state toned_row_action_button(Rectangle row,
                                            int slot_from_right,
                                            const char* label,
                                            Color tone,
                                            toned_action_button_options button_options = {},
                                            row_action_layout_options layout_options = {}) {
    return toned_action_button(row_action_rect(row, slot_from_right, layout_options),
                               label,
                               tone,
                               button_options);
}

inline Rectangle icon_rect(Rectangle rect, float inset) {
    const float size = std::max(1.0f, std::min(rect.width, rect.height) - inset * 2.0f);
    return {
        rect.x + (rect.width - size) * 0.5f,
        rect.y + (rect.height - size) * 0.5f,
        size,
        size,
    };
}

inline button_state icon_button(Rectangle rect, icon_draw_fn draw_icon, icon_button_options options = {}) {
    const auto choose_color = [](Color supplied, Color fallback) {
        return supplied.a > 0 ? supplied : fallback;
    };
    const bool hovered = options.enabled && is_hovered(rect, options.layer);
    const bool pressed = options.enabled && is_pressed(rect, options.layer);
    const bool clicked = options.enabled && is_clicked(rect, options.layer);
    const Color bg = options.enabled
        ? choose_color(options.bg, g_theme->row)
        : choose_color(options.disabled_bg, g_theme->section);
    const Color bg_hover = options.enabled
        ? choose_color(options.bg_hover, g_theme->row_hover)
        : choose_color(options.disabled_bg_hover, g_theme->section);
    const Color icon = options.icon_color.a > 0 ? options.icon_color : g_theme->text;
    const Color icon_hover = options.icon_hover_color.a > 0 ? options.icon_hover_color : g_theme->text;
    const Color disabled_icon = options.disabled_icon_color.a > 0 ? options.disabled_icon_color : g_theme->text_dim;
    const Color border = options.enabled
        ? choose_color((hovered && options.border_hover_color.a > 0) ? options.border_hover_color : options.border_color,
                       g_theme->border)
        : choose_color(options.disabled_border_color, g_theme->border_light);
    const Rectangle visual = pressed ? inset(rect, options.pressed_inset) : rect;
    const Color fill = lerp_color(bg, bg_hover, hovered ? 1.0f : 0.0f);
    draw_rect_f(visual, fill);
    draw_rect_lines(visual, options.border_width,
                    options.border_alpha_tracks_fill ? with_alpha(border, fill.a) : border);
    if (draw_icon != nullptr) {
        const Rectangle icon_visual = pressed && options.icon_pressed_inset >= 0.0f
            ? inset(rect, options.icon_pressed_inset)
            : visual;
        const Color icon_color = options.enabled
            ? (hovered ? icon_hover : icon)
            : disabled_icon;
        draw_icon(icon_rect(icon_visual, options.icon_inset), icon_color, options.icon_stroke_width);
    }
    return {hovered, pressed, clicked};
}

inline row_state row(Rectangle rect, row_options options = {}) {
    const bool hovered = is_hovered(rect, options.layer);
    const bool pressed = is_pressed(rect, options.layer);
    const bool clicked = is_clicked(rect, options.layer);
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    const Color bg = options.custom_colors ? options.bg : g_theme->row;
    const Color bg_hover = options.custom_colors ? options.bg_hover : g_theme->row_hover;
    const Color border_color = options.custom_colors ? options.border_color : g_theme->border;
    detail::draw_row_visual(rect, hovered, pressed, bg, bg_hover, border_color, options.border_width);
    return {hovered, pressed, clicked, visual};
}

inline void surface(Rectangle rect, Color default_fill, Color default_border_color,
                    float default_border_width, surface_options options = {}) {
    const Color fill = options.custom_colors ? options.fill : default_fill;
    const Color border_color = options.custom_colors ? options.border_color : default_border_color;
    const float border_width = options.border_width > 0.0f ? options.border_width : default_border_width;
    draw_rect_f(rect, fill);
    draw_rect_lines(rect, border_width, border_color);
}

inline void surface_fill(Rectangle rect, Color fill) {
    draw_rect_f(rect, fill);
}

inline button_state hover_surface(Rectangle rect, hover_surface_options options = {}) {
    const bool hovered = options.interactive && is_hovered(rect, options.layer);
    const bool pressed = options.interactive && is_pressed(rect, options.layer);
    const bool clicked = options.interactive && is_clicked(rect, options.layer);
    const Rectangle visual = pressed && options.pressed_inset > 0.0f ? inset(rect, options.pressed_inset) : rect;
    const Color fill = options.custom_colors ? options.fill : g_theme->row;
    const Color fill_hover = options.custom_colors ? options.fill_hover : g_theme->row_hover;
    const Color border_color = options.custom_colors ? options.border_color : g_theme->border_light;
    surface(visual, hovered ? fill_hover : fill, border_color, options.border_width);
    return {hovered, pressed, clicked};
}

inline void horizontal_gradient(Rectangle rect, Color left, Color right) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }
    DrawRectangleGradientH(static_cast<int>(rect.x),
                           static_cast<int>(rect.y),
                           std::max(1, static_cast<int>(std::ceil(rect.width))),
                           std::max(1, static_cast<int>(std::ceil(rect.height))),
                           left,
                           right);
}

inline void vertical_gradient(Rectangle rect, Color top, Color bottom) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }
    DrawRectangleGradientV(static_cast<int>(rect.x),
                           static_cast<int>(rect.y),
                           std::max(1, static_cast<int>(std::ceil(rect.width))),
                           std::max(1, static_cast<int>(std::ceil(rect.height))),
                           top,
                           bottom);
}

inline void rectangle_gradient(Rectangle rect,
                               Color top_left,
                               Color bottom_left,
                               Color top_right,
                               Color bottom_right) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }
    DrawRectangleGradientEx(rect, top_left, bottom_left, top_right, bottom_right);
}

inline void rounded_surface(Rectangle rect,
                            float roundness,
                            int segments,
                            Color fill,
                            Color border_color = {},
                            float border_width = 0.0f) {
    DrawRectangleRounded(rect, roundness, segments, fill);
    if (border_width > 0.0f && border_color.a > 0) {
        DrawRectangleRoundedLinesEx(rect, roundness, segments, border_width, border_color);
    }
}

inline void frame(Rectangle rect, Color border_color, float border_width = 1.0f) {
    draw_rect_lines(rect, border_width, border_color);
}

inline void divider(Rectangle rect, Color color) {
    draw_rect_f(rect, color);
}

inline void accent_bar(Rectangle rect, Color color) {
    draw_rect_f(rect, color);
}

inline void backdrop(Rectangle rect, Color color) {
    draw_rect_f(rect, color);
}

inline void bar_surface(Rectangle rect, Color fill, Color divider_color, float divider_height = 1.0f) {
    surface_fill(rect, fill);
    if (divider_height > 0.0f && divider_color.a > 0) {
        divider({rect.x, rect.y + rect.height - divider_height, rect.width, divider_height}, divider_color);
    }
}

inline void dim_outside_rect(Rectangle outer, Rectangle inner, Color color) {
    const float outer_right = outer.x + outer.width;
    const float outer_bottom = outer.y + outer.height;
    const float inner_right = inner.x + inner.width;
    const float inner_bottom = inner.y + inner.height;

    if (inner.y > outer.y) {
        surface_fill({outer.x, outer.y, outer.width, inner.y - outer.y}, color);
    }
    if (inner_bottom < outer_bottom) {
        surface_fill({outer.x, inner_bottom, outer.width, outer_bottom - inner_bottom}, color);
    }
    if (inner.x > outer.x) {
        surface_fill({outer.x, inner.y, inner.x - outer.x, inner.height}, color);
    }
    if (inner_right < outer_right) {
        surface_fill({inner_right, inner.y, outer_right - inner_right, inner.height}, color);
    }
}

inline void block_spectrum_bar(float x,
                               float baseline,
                               float bar_width,
                               float height,
                               float max_height,
                               float peak_height,
                               float block_height,
                               float block_gap,
                               Color base_low,
                               Color base_mid,
                               Color base_top,
                               Color peak_glow,
                               Color peak_color) {
    const float resolved_max_height = std::max(1.0f, max_height);
    const float block_step = block_height + block_gap;
    if (height > 0.5f) {
        for (float block_bottom = baseline; block_bottom > baseline - height; block_bottom -= block_step) {
            const float block_top = std::max(baseline - height, block_bottom - block_height);
            const float segment_height = block_bottom - block_top;
            if (segment_height > 0.5f) {
                const float color_t = std::clamp((baseline - block_top) / resolved_max_height, 0.0f, 1.0f);
                const Color block_color =
                    color_t < 0.6f
                        ? ::lerp_color(base_low, base_mid, color_t / 0.6f)
                        : ::lerp_color(base_mid, base_top, (color_t - 0.6f) / 0.4f);
                surface_fill({x, block_top, bar_width, segment_height}, block_color);
            }
        }
    }

    const float peak_y = baseline - std::clamp(peak_height, 0.0f, resolved_max_height) - 2.0f;
    surface_fill({x, peak_y - 1.0f, bar_width, 4.0f}, peak_glow);
    surface_fill({x, peak_y, bar_width, 2.0f}, peak_color);
}

inline void solid_spectrum_bar(Rectangle rect, Color bar, Color peak,
                               float peak_ratio = 0.08f,
                               float min_peak_height = 2.0f) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }
    surface_fill(rect, bar);
    surface_fill({rect.x, rect.y, rect.width, std::max(min_peak_height, rect.height * peak_ratio)}, peak);
}

inline void placeholder(Rectangle rect, const char* label, placeholder_options options = {}) {
    const Color fill = options.custom_colors ? options.fill : g_theme->section;
    const Color border_color = options.custom_colors ? options.border_color : g_theme->border_light;
    const Color text_color = options.custom_colors ? options.text_color : g_theme->text_muted;
    draw_rect_f(rect, fill);
    if (options.draw_border) {
        frame(rect, border_color, options.border_width);
    }
    if (label != nullptr && label[0] != '\0') {
        if (options.body_text) {
            draw_body_text_in_rect(label, options.font_size, rect, text_color, options.align);
        } else {
            draw_text_in_rect(label, options.font_size, rect, text_color, options.align);
        }
    }
}

inline void panel(Rectangle rect, surface_options options = {}) {
    surface(rect, g_theme->panel, g_theme->border, 2.0f, options);
}

inline void section(Rectangle rect, surface_options options = {}) {
    surface(rect, g_theme->section, g_theme->border_light, 1.5f, options);
}

inline void draw_tab_button_visual(Rectangle rect, const char* label,
                                   bool hovered, bool pressed,
                                   tab_button_options options) {
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    const Color bg = options.custom_colors ? options.bg : g_theme->row;
    const Color bg_hover = options.custom_colors ? options.bg_hover : g_theme->row_hover;
    const Color bg_selected = options.custom_colors ? options.bg_selected : g_theme->row_selected;
    const Color bg_selected_hover = options.custom_colors ? options.bg_selected_hover : g_theme->row_active;
    const Color border = options.custom_colors ? options.border : g_theme->border;
    const Color border_selected = options.custom_colors ? options.border_selected : g_theme->border_active;
    const Color text_color = options.custom_colors ? options.text_color : g_theme->text_secondary;
    const Color selected_text_color = options.custom_colors ? options.selected_text_color : g_theme->text;
    const Color underline_color = options.custom_colors ? options.underline_color : with_alpha(g_theme->accent, 235);

    if (options.style == tab_button_style::underline) {
        if (hovered || options.selected) {
            draw_rect_f(visual, options.selected ? bg_selected : bg_hover);
        }
        draw_text_in_rect(label != nullptr ? label : "",
                          options.font_size,
                          visual,
                          options.selected ? selected_text_color : text_color);
        if (options.selected) {
            draw_rect_f({
                visual.x + options.underline_inset_x,
                visual.y + visual.height - options.underline_height - 1.0f,
                std::max(0.0f, visual.width - options.underline_inset_x * 2.0f),
                options.underline_height,
            }, underline_color);
        }
        return;
    }

    detail::draw_row_visual(rect,
                            hovered,
                            pressed,
                            options.selected ? bg_selected : bg,
                            options.selected ? bg_selected_hover : bg_hover,
                            options.selected ? border_selected : border,
                            options.selected ? options.selected_border_width : options.border_width);
    draw_text_in_rect(label != nullptr ? label : "",
                      options.font_size,
                      visual,
                      options.selected ? selected_text_color : text_color);
}

inline button_state tab_button(Rectangle rect, const char* label, tab_button_options options = {}) {
    const bool hovered = options.interactive && is_hovered(rect, options.layer);
    const bool pressed = options.interactive && is_pressed(rect, options.layer);
    const bool clicked = options.interactive && is_clicked(rect, options.layer);
    draw_tab_button_visual(rect, label, hovered, pressed, options);
    return {hovered, pressed, clicked};
}

inline row_state selectable_row(Rectangle rect, bool selected,
                                float border_width = 2.0f) {
    return row(rect, {
        .layer = draw_layer::base,
        .border_width = border_width,
        .bg = selected ? g_theme->row_selected : g_theme->row,
        .bg_hover = selected ? g_theme->row_active : g_theme->row_hover,
        .border_color = selected ? g_theme->border_active : g_theme->border,
        .custom_colors = true,
    });
}

inline button_state toggle_row(Rectangle rect, const char* label, const char* description,
                               bool checked, toggle_row_options options = {}) {
    const bool hovered = is_hovered(rect, options.layer);
    const bool pressed = is_pressed(rect, options.layer);
    const bool clicked = is_clicked(rect, options.layer);
    const Rectangle visual = pressed ? inset(rect, options.pressed_inset) : rect;
    const Color bg = options.custom_colors ? options.bg : (checked ? g_theme->row_selected : g_theme->row);
    const Color bg_hover = options.custom_colors ? options.bg_hover : (checked ? g_theme->row_active : g_theme->row_hover);
    const Color border = options.custom_colors ? options.border_color : g_theme->border_light;
    const Color checked_border = options.custom_colors ? options.checked_border_color : g_theme->accent;
    const Color label_color = options.custom_colors ? options.label_color : g_theme->text;
    const Color description_color = options.custom_colors ? options.description_color : g_theme->text_muted;
    const Color switch_bg = options.custom_colors
        ? options.switch_bg
        : (checked ? with_alpha(g_theme->accent, 160) : with_alpha(g_theme->text_muted, 70));
    const Color switch_border = options.custom_colors
        ? options.switch_border_color
        : (checked ? g_theme->accent : g_theme->border_light);
    const Color knob_color = options.custom_colors
        ? options.knob_color
        : (checked ? g_theme->text : g_theme->text_secondary);

    surface(visual,
            hovered ? bg_hover : bg,
            checked ? checked_border : border,
            checked ? options.checked_border_width : options.border_width);

    const Rectangle label_rect = {
        visual.x + options.text_left_padding,
        visual.y + options.label_top_padding,
        std::max(0.0f, visual.width - options.text_right_reserved),
        24.0f,
    };
    const Rectangle description_rect = {
        visual.x + options.text_left_padding,
        visual.y + options.description_top_padding,
        std::max(0.0f, visual.width - options.text_right_reserved),
        18.0f,
    };
    draw_text_in_rect(label != nullptr ? label : "", options.label_font_size, label_rect, label_color, text_align::left);
    draw_text_in_rect(description != nullptr ? description : "", options.description_font_size,
                      description_rect, description_color, text_align::left);

    const Rectangle switch_track = {
        visual.x + visual.width - options.switch_right_inset - options.switch_width,
        visual.y + (visual.height - options.switch_height) * 0.5f,
        options.switch_width,
        options.switch_height,
    };
    surface(switch_track, switch_bg, switch_border, 1.0f);

    const float knob_x = checked
        ? switch_track.x + switch_track.width - options.knob_size - 2.0f
        : switch_track.x + 2.0f;
    const Rectangle knob = {
        knob_x,
        switch_track.y + (switch_track.height - options.knob_size) * 0.5f,
        options.knob_size,
        options.knob_size,
    };
    surface_fill(knob, knob_color);

    return {hovered, pressed, clicked};
}

inline button_state queued_button(Rectangle rect, const char* label, button_options options = {}) {
    const bool hovered = options.interactive && is_hovered(rect, options.layer);
    const bool pressed = options.interactive && is_pressed(rect, options.layer);
    const bool clicked = options.interactive && is_clicked(rect, options.layer);
    const std::string label_copy = label != nullptr ? label : "";
    const Color bg = options.custom_colors ? options.bg : g_theme->row;
    const Color bg_hover = options.custom_colors ? options.bg_hover : g_theme->row_hover;
    const Color text_color = options.custom_colors ? options.text_color : g_theme->text;
    const Color border_color = options.custom_border ? options.border_color : Color{};

    enqueue_draw_command(options.layer, [rect, hovered, pressed, label_copy, options, bg, bg_hover, text_color, border_color]() {
        detail::draw_button_visual(rect, hovered, pressed, label_copy.c_str(), options.font_size,
                                   bg, bg_hover, text_color, options.border_width, border_color);
    });

    return {hovered, pressed, clicked};
}

inline button_state queued_action_button(Rectangle rect, const char* label, action_button_options options = {}) {
    return queued_button(rect, label, action_button_to_button_options(options));
}

inline button_state queued_toned_action_button(Rectangle rect, const char* label, Color tone,
                                               toned_action_button_options options = {}) {
    return queued_action_button(rect, label, toned_action_button_to_action_button_options(tone, options));
}

inline row_state queued_row(Rectangle rect, row_options options = {}) {
    const bool hovered = is_hovered(rect, options.layer);
    const bool pressed = is_pressed(rect, options.layer);
    const bool clicked = is_clicked(rect, options.layer);
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    const Color bg = options.custom_colors ? options.bg : g_theme->row;
    const Color bg_hover = options.custom_colors ? options.bg_hover : g_theme->row_hover;
    const Color border_color = options.custom_colors ? options.border_color : g_theme->border;

    enqueue_draw_command(options.layer, [rect, hovered, pressed, bg, bg_hover, border_color, options]() {
        detail::draw_row_visual(rect, hovered, pressed, bg, bg_hover, border_color, options.border_width);
    });

    return {hovered, pressed, clicked, visual};
}

inline button_state queued_tab_button(Rectangle rect, const char* label, tab_button_options options = {}) {
    const bool hovered = options.interactive && is_hovered(rect, options.layer);
    const bool pressed = options.interactive && is_pressed(rect, options.layer);
    const bool clicked = options.interactive && is_clicked(rect, options.layer);
    const std::string label_copy = label != nullptr ? label : "";

    enqueue_draw_command(options.layer, [rect, label_copy, hovered, pressed, options]() {
        draw_tab_button_visual(rect, label_copy.c_str(), hovered, pressed, options);
    });

    return {hovered, pressed, clicked};
}

// ── パネル ──────────────────────────────────────────────

inline void queued_panel(Rectangle rect, draw_layer layer = draw_layer::base,
                         surface_options options = {}) {
    enqueue_draw_command(layer, [rect, options]() {
        panel(rect, options);
    });
}

inline void queued_section(Rectangle rect, draw_layer layer = draw_layer::base,
                           surface_options options = {}) {
    enqueue_draw_command(layer, [rect, options]() {
        section(rect, options);
    });
}

inline void enqueue_text_in_rect(const char* text, int font_size, Rectangle rect, Color color,
                                 text_align align = text_align::center,
                                 draw_layer layer = draw_layer::base) {
    const std::string text_copy = text != nullptr ? text : "";
    enqueue_draw_command(layer, [text_copy, font_size, rect, color, align]() {
        draw_text_in_rect(text_copy.c_str(), font_size, rect, color, align);
    });
}

inline void enqueue_body_text_in_rect(const char* text, int font_size, Rectangle rect, Color color,
                                      text_align align = text_align::center,
                                      draw_layer layer = draw_layer::base) {
    const std::string text_copy = text != nullptr ? text : "";
    enqueue_draw_command(layer, [text_copy, font_size, rect, color, align]() {
        draw_body_text_in_rect(text_copy.c_str(), font_size, rect, color, align);
    });
}

inline void enqueue_display_text_in_rect(const char* text, int font_size, Rectangle rect, Color color,
                                         text_align align = text_align::center,
                                         draw_layer layer = draw_layer::base) {
    const std::string text_copy = text != nullptr ? text : "";
    enqueue_draw_command(layer, [text_copy, font_size, rect, color, align]() {
        draw_display_text_in_rect(text_copy.c_str(), font_size, rect, color, align);
    });
}

// ── ラベル付き値 ────────────────────────────────────────

// 左にラベル、右に値を表示する行。設定画面やリザルト画面の統計行に使用。
inline void draw_label_value(Rectangle rect, const char* label, const char* value,
                             int font_size, Color label_color, Color value_color,
                             float label_width = 200.0f) {
    const Rectangle label_rect = {rect.x, rect.y, label_width, rect.height};
    const Rectangle value_rect = {rect.x + label_width, rect.y,
                                  rect.width - label_width, rect.height};
    draw_text_in_rect(label, font_size, label_rect, label_color, text_align::left);
    draw_text_in_rect(value, font_size, value_rect, value_color, text_align::left);
}

inline void draw_label_value_marquee(Rectangle rect, const char* label, const char* value,
                                     int font_size, Color label_color, Color value_color,
                                     double time, float label_width = 200.0f) {
    const Rectangle label_rect = {rect.x, rect.y, label_width, rect.height};
    const Rectangle value_rect = {rect.x + label_width, rect.y,
                                  rect.width - label_width, rect.height};
    draw_text_in_rect(label, font_size, label_rect, label_color, text_align::left);
    draw_marquee_text(value, value_rect.x, value_rect.y + 4.0f, font_size,
                      value_color, value_rect.width, time);
}

inline value_selector_layout make_value_selector_layout(Rectangle rect, value_selector_options options = {}) {
    const Rectangle content = inset(rect, edge_insets::symmetric(0.0f, options.content_padding));
    const rect_pair columns = split_columns(content, options.label_width);
    const Rectangle button_pair_area = place(columns.second,
                                             options.button_size * 2.0f + options.button_gap,
                                             options.button_size,
                                             anchor::center_right,
                                             anchor::center_right);
    Rectangle buttons[2];
    hstack(button_pair_area, options.button_size, options.button_gap, buttons);

    return {
        columns.first,
        {
            columns.second.x,
            columns.second.y,
            button_pair_area.x - columns.second.x - 16.0f,
            columns.second.height
        },
        buttons[0],
        buttons[1],
    };
}

inline selector_state value_selector(Rectangle rect, const char* label, const char* value,
                                     value_selector_options options = {}) {
    const row_state control_row = row(rect, {
        .layer = options.layer,
        .border_width = 2.0f,
        .bg = g_theme->row,
        .bg_hover = g_theme->row_hover,
        .border_color = g_theme->border,
        .custom_colors = true,
    });

    const value_selector_layout layout = make_value_selector_layout(control_row.visual, options);
    draw_text_in_rect(label, options.font_size, layout.label_rect, g_theme->text, text_align::left);
    draw_text_in_rect(value, options.font_size, layout.value_rect, g_theme->text_dim, text_align::right);
    const button_state left = button(layout.left_button_rect, "<", {
        .layer = options.layer,
        .font_size = options.font_size,
    });
    const button_state right = button(layout.right_button_rect, ">", {
        .layer = options.layer,
        .font_size = options.font_size,
    });
    return {control_row, left, right};
}

inline dropdown_state dropdown(Rectangle trigger_rect, Rectangle menu_rect,
                               const char* label, const char* value,
                               std::span<const char* const> items,
                               int selected_index, bool open,
                               dropdown_options options = {}) {
    const row_state trigger = row(trigger_rect, {
        .layer = options.trigger_layer,
        .border_width = 2.0f,
        .bg = open ? g_theme->row_selected : g_theme->row,
        .bg_hover = open ? g_theme->row_selected_hover : g_theme->row_hover,
        .border_color = open ? g_theme->border_active : g_theme->border,
        .custom_colors = true,
    });
    const Rectangle content = inset(trigger.visual, edge_insets::symmetric(0.0f, options.content_padding));
    const rect_pair columns = split_columns(content, options.label_width);
    const Rectangle arrow_rect = place(columns.second, 18.0f, columns.second.height,
                                       anchor::center_right, anchor::center_right);
    const Rectangle value_rect = {
        columns.second.x,
        columns.second.y,
        std::max(0.0f, arrow_rect.x - columns.second.x - 8.0f),
        columns.second.height
    };

    draw_text_in_rect(label, options.font_size, columns.first, g_theme->text, text_align::left);
    draw_text_in_rect(value, options.font_size, value_rect, g_theme->text_dim, text_align::right);
    draw_text_in_rect(open ? "^" : "v", options.font_size, arrow_rect, g_theme->text_dim);

    int clicked_index = -1;
    if (open) {
        section(menu_rect);
        Rectangle item_rect = {menu_rect.x + 6.0f, menu_rect.y + 6.0f, menu_rect.width - 12.0f, options.item_height};
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const row_state option_row = selectable_row(item_rect, i == selected_index, 1.5f);
            draw_text_in_rect(items[i], options.font_size, inset(option_row.visual, edge_insets::symmetric(0.0f, 12.0f)),
                              i == selected_index ? g_theme->text : g_theme->text_dim, text_align::left);
            if (option_row.clicked) {
                clicked_index = i;
            }
            item_rect.y += options.item_height + options.item_spacing;
        }
    }

    return {trigger, clicked_index};
}

inline dropdown_state queued_dropdown(Rectangle trigger_rect, Rectangle menu_rect,
                                      const char* label, const char* value,
                                      std::span<const char* const> items,
                                      int selected_index, bool open,
                                      dropdown_options options = {}) {
    if (open) {
        register_hit_region(menu_rect, options.menu_layer);
    }

    const bool trigger_pressed = is_pressed(trigger_rect, options.trigger_layer);
    const row_state trigger = {
        is_hovered(trigger_rect, options.trigger_layer),
        trigger_pressed,
        is_clicked(trigger_rect, options.trigger_layer),
        trigger_pressed ? inset(trigger_rect, 1.5f) : trigger_rect
    };

    std::vector<std::string> option_copies;
    option_copies.reserve(items.size());
    for (const char* option : items) {
        option_copies.emplace_back(option != nullptr ? option : "");
    }
    const std::string label_copy = label != nullptr ? label : "";
    const std::string value_copy = value != nullptr ? value : "";

    enqueue_draw_command(options.trigger_layer, [trigger_rect, trigger, label_copy, value_copy, options, open]() {
        const Rectangle trigger_content = inset(trigger.visual, edge_insets::symmetric(0.0f, options.content_padding));
        const rect_pair trigger_columns = split_columns(trigger_content, options.label_width);
        const Rectangle trigger_arrow_rect = place(trigger_columns.second, 18.0f, trigger_columns.second.height,
                                                   anchor::center_right, anchor::center_right);
        const Rectangle trigger_value_rect = {
            trigger_columns.second.x,
            trigger_columns.second.y,
            std::max(0.0f, trigger_arrow_rect.x - trigger_columns.second.x - 8.0f),
            trigger_columns.second.height
        };

        detail::draw_row_visual(trigger_rect, trigger.hovered, trigger.pressed,
                                open ? g_theme->row_selected : g_theme->row,
                                open ? g_theme->row_selected_hover : g_theme->row_hover,
                                open ? g_theme->border_active : g_theme->border,
                                2.0f);
        draw_text_in_rect(label_copy.c_str(), options.font_size, trigger_columns.first, g_theme->text, text_align::left);
        draw_text_in_rect(value_copy.c_str(), options.font_size, trigger_value_rect, g_theme->text_dim, text_align::right);
        draw_text_in_rect(open ? "^" : "v", options.font_size, trigger_arrow_rect, g_theme->text_dim);
    });

    int clicked_index = -1;
    if (open) {
        Rectangle item_rect = {menu_rect.x + 6.0f, menu_rect.y + 6.0f, menu_rect.width - 12.0f, options.item_height};
        std::vector<row_state> item_states;
        item_states.reserve(option_copies.size());

        for (int i = 0; i < static_cast<int>(option_copies.size()); ++i) {
            const bool item_pressed = is_pressed(item_rect, options.menu_layer);
            const row_state option_row = {
                is_hovered(item_rect, options.menu_layer),
                item_pressed,
                is_clicked(item_rect, options.menu_layer),
                item_pressed ? inset(item_rect, 1.5f) : item_rect
            };
            item_states.push_back(option_row);
            if (option_row.clicked) {
                clicked_index = i;
            }
            item_rect.y += options.item_height + options.item_spacing;
        }

        enqueue_draw_command(options.menu_layer, [menu_rect, options, selected_index, option_copies = std::move(option_copies),
                                          item_states = std::move(item_states)]() {
            section(menu_rect);
            Rectangle draw_item_rect = {menu_rect.x + 6.0f, menu_rect.y + 6.0f, menu_rect.width - 12.0f, options.item_height};
            for (int i = 0; i < static_cast<int>(option_copies.size()); ++i) {
                const row_state& option_row = item_states[static_cast<size_t>(i)];
                detail::draw_row_visual(draw_item_rect, option_row.hovered, option_row.pressed,
                                        i == selected_index ? g_theme->row_selected : g_theme->row,
                                        i == selected_index ? g_theme->row_active : g_theme->row_hover,
                                        i == selected_index ? g_theme->border_active : g_theme->border,
                                        1.5f);
                draw_text_in_rect(option_copies[static_cast<size_t>(i)].c_str(), options.font_size,
                                  inset(option_row.visual, edge_insets::symmetric(0.0f, 12.0f)),
                                  i == selected_index ? g_theme->text : g_theme->text_dim, text_align::left);
                draw_item_rect.y += options.item_height + options.item_spacing;
            }
        });
    }

    return {trigger, clicked_index};
}

// コンテキストメニュー（ドロップダウンメニュー）の描画追加と、クリックされた項目の位置を返す
inline context_menu_state context_menu(Rectangle menu_rect,
                                       std::span<const context_menu_item> items,
                                       context_menu_options options = {}) {
    register_hit_region(menu_rect, options.layer);

    int clicked_index = -1;
    Rectangle item_rect = {menu_rect.x + 6.0f, menu_rect.y + 6.0f, menu_rect.width - 12.0f, options.item_height};
    std::vector<std::string> item_labels;
    std::vector<bool> item_enabled;
    std::vector<context_menu_item::kind> item_kinds;
    std::vector<row_state> item_states;
    item_labels.reserve(items.size());
    item_enabled.reserve(items.size());
    item_kinds.reserve(items.size());
    item_states.reserve(items.size());

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const context_menu_item::kind kind = items[static_cast<size_t>(i)].item_kind;
        const bool enabled = items[static_cast<size_t>(i)].enabled && kind == context_menu_item::kind::action;
        const bool pressed = enabled && is_mouse_button_down(item_rect, MOUSE_BUTTON_LEFT, options.layer);
        const row_state state = {
            enabled && is_hovered(item_rect, options.layer),
            pressed,
            enabled && is_mouse_button_released(item_rect, MOUSE_BUTTON_LEFT, options.layer),
            pressed ? inset(item_rect, 1.5f) : item_rect
        };
        item_labels.emplace_back(items[static_cast<size_t>(i)].label != nullptr ? items[static_cast<size_t>(i)].label : "");
        item_enabled.push_back(enabled);
        item_kinds.push_back(kind);
        item_states.push_back(state);
        if (state.clicked) {
            clicked_index = i;
        }
        item_rect.y += options.item_height + options.item_spacing;
    }

    enqueue_draw_command(options.layer, [menu_rect, options,
                                 item_labels = std::move(item_labels),
                                 item_enabled = std::move(item_enabled),
                                 item_kinds = std::move(item_kinds),
                                 item_states = std::move(item_states)]() {
        section(menu_rect);
        Rectangle draw_item_rect = {menu_rect.x + 6.0f, menu_rect.y + 6.0f, menu_rect.width - 12.0f, options.item_height};
        for (int i = 0; i < static_cast<int>(item_labels.size()); ++i) {
            const bool enabled = item_enabled[static_cast<size_t>(i)];
            const context_menu_item::kind kind = item_kinds[static_cast<size_t>(i)];
            const row_state& state = item_states[static_cast<size_t>(i)];
            if (kind == context_menu_item::kind::header) {
                const Rectangle text_rect = inset(draw_item_rect, edge_insets::symmetric(0.0f, 12.0f));
                draw_text_in_rect(item_labels[static_cast<size_t>(i)].c_str(), std::max(12, options.font_size - 2),
                                  text_rect, g_theme->text_muted, text_align::left);
                draw_line_ex({draw_item_rect.x + 10.0f, draw_item_rect.y + draw_item_rect.height - 3.0f},
                             {draw_item_rect.x + draw_item_rect.width - 10.0f, draw_item_rect.y + draw_item_rect.height - 3.0f},
                             1.0f, g_theme->border_light);
            } else if (kind == context_menu_item::kind::separator) {
                draw_line_ex({draw_item_rect.x + 8.0f, draw_item_rect.y + draw_item_rect.height * 0.5f},
                             {draw_item_rect.x + draw_item_rect.width - 8.0f, draw_item_rect.y + draw_item_rect.height * 0.5f},
                             1.0f, g_theme->border_light);
            } else {
                detail::draw_row_visual(draw_item_rect, state.hovered, state.pressed,
                                        enabled ? g_theme->row : with_alpha(g_theme->row, 180),
                                        enabled ? g_theme->row_hover : with_alpha(g_theme->row, 180),
                                        enabled ? g_theme->border : g_theme->border_light,
                                        1.5f);
                draw_text_in_rect(item_labels[static_cast<size_t>(i)].c_str(), options.font_size,
                                  inset(state.visual, edge_insets::symmetric(0.0f, 12.0f)),
                                  enabled ? g_theme->text : g_theme->text_muted, text_align::left);
            }
            draw_item_rect.y += options.item_height + options.item_spacing;
        }
    });

    return {clicked_index};
}

inline void draw_header_block(Rectangle rect, const char* title, const char* subtitle,
                              int title_size = 34, int subtitle_size = 20,
                              float spacing = 8.0f) {
    const rect_pair rows = split_rows(rect, static_cast<float>(title_size), spacing);
    draw_text_in_rect(title, title_size, rows.first, g_theme->text, text_align::left);
    draw_text_in_rect(subtitle, subtitle_size, rows.second, g_theme->text_muted, text_align::left);
}

// ── プログレスバー ──────────────────────────────────────

// 水平プログレスバー。ヘルスゲージ等に使用する。
// ratio: 0.0〜1.0 の塗り割合。
inline void draw_progress_bar(Rectangle rect, float ratio,
                              Color bg, Color fill, Color border_color,
                              float border_width = 3.0f, float bar_inset = 4.0f) {
    draw_rect_f(rect, bg);
    draw_rect_lines(rect, border_width, border_color);

    if (ratio > 0.0f) {
        const Rectangle fill_area = inset(rect, bar_inset);
        const float fill_w = fill_area.width * std::clamp(ratio, 0.0f, 1.0f);
        draw_rect_f(fill_area.x, fill_area.y, fill_w, fill_area.height, fill);
    }
}

inline void progress_bar(Rectangle rect, float ratio, progress_bar_options options = {}) {
    const Color bg = options.custom_colors ? options.bg : g_theme->bg_alt;
    const Color fill = options.custom_colors ? options.fill : g_theme->accent;
    const Color border_color = options.custom_colors ? options.border_color : g_theme->border_light;
    draw_rect_f(rect, bg);
    if (ratio > 0.0f) {
        Rectangle fill_area = inset(rect, options.fill_inset);
        fill_area.width *= std::clamp(ratio, 0.0f, 1.0f);
        draw_rect_f(fill_area, fill);
    }
    draw_rect_lines(rect, options.border_width, border_color);
}

// ── スライダー ──────────────────────────────────────────

inline slider_layout make_slider_layout(Rectangle row_rect, float track_left_inset, float track_right_inset,
                                        float label_width = 200.0f, float content_padding = 18.0f,
                                        float track_top_offset = 26.0f) {
    const Rectangle content = inset(row_rect, edge_insets::symmetric(0.0f, content_padding));
    const Rectangle label_rect = {content.x, content.y, label_width, content.height};
    const float track_left = row_rect.x + track_left_inset;
    const float track_width = row_rect.width - track_left_inset - track_right_inset;
    return {
        label_rect,
        {track_left, row_rect.y + track_top_offset, track_width, 6.0f},
        {track_left, row_rect.y, track_width, track_top_offset}
    };
}

inline float slider_ratio_from_mouse(Rectangle row_rect, float track_left_inset, float track_right_inset,
                                     slider_options options = {}) {
    const slider_layout layout = make_slider_layout(row_rect, track_left_inset, track_right_inset,
                                                    options.label_width, options.content_padding,
                                                    options.track_top_offset);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    return std::clamp((mouse.x - layout.track_rect.x) / std::max(1.0f, layout.track_rect.width), 0.0f, 1.0f);
}

// スライダー行を描画する。track_left_inset / track_right_inset は row_rect 基準の相対指定。
inline float slider_relative(Rectangle row_rect, const char* label, const char* value_text,
                             float ratio, float track_left_inset, float track_right_inset,
                             slider_options options = {}) {
    const row_state slider_row = row(row_rect, {
        .layer = options.layer,
        .border_width = 2.0f,
        .bg = g_theme->row,
        .bg_hover = g_theme->row_hover,
        .border_color = g_theme->border,
        .custom_colors = true,
    });
    const slider_layout layout = make_slider_layout(slider_row.visual, track_left_inset, track_right_inset,
                                                    options.label_width, options.content_padding,
                                                    options.track_top_offset);
    const float clamped = std::clamp(ratio, 0.0f, 1.0f);

    draw_text_in_rect(label, options.font_size, layout.label_rect, g_theme->text, text_align::left);
    draw_rect_f(layout.track_rect, g_theme->slider_track);
    draw_rect_f(layout.track_rect.x, layout.track_rect.y,
                layout.track_rect.width * clamped, layout.track_rect.height, g_theme->slider_fill);

    const float knob_x = layout.track_rect.x + layout.track_rect.width * clamped;
    draw_rect_f(knob_x - 4.0f, layout.track_rect.y - 5.0f, 8.0f, 16.0f, g_theme->slider_knob);
    draw_text_in_rect(value_text, options.font_size, layout.value_rect, g_theme->text_dim, text_align::right);

    const bool drag_target_hovered = options.drag_blocked_by_layer
        ? is_hovered(row_rect, options.layer)
        : CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), row_rect);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && drag_target_hovered) {
        return slider_ratio_from_mouse(slider_row.visual, track_left_inset, track_right_inset, options);
    }
    return -1.0f;
}

inline void scrollbar(Rectangle track_rect, float content_height, float scroll_offset,
                      scrollbar_options options = {}) {
    const scroll_metrics metrics = vertical_scroll_metrics(track_rect, content_height, scroll_offset,
                                                           options.min_thumb_height);
    if (content_height <= track_rect.height) {
        return;
    }

    const Color track_color = options.custom_colors ? options.track_color : g_theme->scrollbar_track;
    const Color thumb_color = options.custom_colors ? options.thumb_color : g_theme->scrollbar_thumb;
    draw_rect_f(track_rect, track_color);
    draw_rect_f(metrics.thumb_rect, thumb_color);
}

inline scrollbar_interaction vertical_scrollbar(Rectangle track_rect, float content_height, float scroll_offset,
                                                bool& dragging, float& drag_offset,
                                                scrollbar_options options = {}) {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        dragging = false;
    }

    const scroll_metrics metrics = vertical_scroll_metrics(track_rect, content_height, scroll_offset,
                                                           options.min_thumb_height);
    if (metrics.max_scroll <= 0.0f) {
        return {0.0f, false, false};
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    float next_offset = std::clamp(scroll_offset, 0.0f, metrics.max_scroll);
    bool changed = false;

    const bool track_pressed = options.drag_blocked_by_layer
        ? is_hovered(track_rect, options.layer)
        : CheckCollisionPointRec(mouse, track_rect);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && track_pressed) {
        if (CheckCollisionPointRec(mouse, metrics.thumb_rect)) {
            dragging = true;
            drag_offset = mouse.y - metrics.thumb_rect.y;
        } else {
            const float thumb_half = metrics.thumb_rect.height * 0.5f;
            const float available = std::max(1.0f, track_rect.height - metrics.thumb_rect.height);
            const float thumb_top = std::clamp(mouse.y - thumb_half - track_rect.y, 0.0f, available);
            next_offset = metrics.max_scroll * (thumb_top / available);
            changed = true;
        }
    }

    if (dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const float available = std::max(1.0f, track_rect.height - metrics.thumb_rect.height);
        const float thumb_top = std::clamp(mouse.y - drag_offset - track_rect.y, 0.0f, available);
        next_offset = metrics.max_scroll * (thumb_top / available);
        changed = true;
    }

    next_offset = std::clamp(next_offset, 0.0f, metrics.max_scroll);
    return {next_offset, changed, dragging};
}

// ── オーバーレイ ────────────────────────────────────────

// 画面全体を覆う半透明オーバーレイ。ポーズ画面やフェードイン/アウトに使用する。
inline void draw_fullscreen_overlay(Color color) {
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, color);
}

inline void enqueue_fullscreen_overlay(Color color, draw_layer layer = draw_layer::overlay) {
    enqueue_draw_command(layer, [color]() {
        draw_fullscreen_overlay(color);
    });
}

}  // namespace ui
