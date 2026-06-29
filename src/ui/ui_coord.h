#pragma once

#include <algorithm>
#include <cmath>

#include "raylib.h"
#include "ui_font.h"
#include "virtual_screen.h"

// 描画境界の座標変換ヘルパー。
// float → int の static_cast を一元管理し、呼び出し側の記述を簡潔にする。
namespace ui {

// 整数座標ペア。
struct ipoint {
    int x;
    int y;
};

// float を int へ切り捨て変換する（描画位置座標用）。
inline int to_i(float v) {
    return static_cast<int>(v);
}

// float を int へ四捨五入変換する。
inline int round_to_i(float v) {
    return static_cast<int>(std::lround(v));
}

// Vector2 を ipoint へ変換する。
inline ipoint to_point(Vector2 v) {
    return {static_cast<int>(v.x), static_cast<int>(v.y)};
}

// 1920x1080 の論理ピクセル境界に揃える。
inline float snap_pixel(float value) {
    return std::round(value);
}

inline float snap_stroke_width(float width) {
    const float screen_scale = std::max(0.001f, virtual_screen::design_to_screen_scale());
    const float min_visible_width = std::ceil(1.0f / screen_scale);
    return std::max(min_visible_width, std::round(width));
}

inline Rectangle snap_rect(Rectangle rect) {
    const float x = snap_pixel(rect.x);
    const float y = snap_pixel(rect.y);
    const float right = snap_pixel(rect.x + rect.width);
    const float bottom = snap_pixel(rect.y + rect.height);
    return {x, y, std::max(1.0f, right - x), std::max(1.0f, bottom - y)};
}

// float 座標で DrawText を呼び出す。
inline void draw_text_f(const char* text, float x, float y, int font_size, Color color) {
    if (text == nullptr || *text == '\0') {
        return;
    }
    draw_text_auto(text, {x, y}, static_cast<float>(font_size), 0.0f, color);
}

inline Vector2 measure_default_text(const char* text, float font_size, float spacing = 1.0f) {
    if (text == nullptr || *text == '\0') {
        return {};
    }
    return MeasureTextEx(GetFontDefault(), text, font_size, spacing);
}

inline void draw_default_text_pro(const char* text,
                                  Vector2 position,
                                  Vector2 origin,
                                  float rotation,
                                  float font_size,
                                  Color color,
                                  float spacing = 1.0f) {
    if (text == nullptr || *text == '\0') {
        return;
    }
    DrawTextPro(GetFontDefault(), text, position, origin, rotation, font_size, spacing, color);
}

// float 座標で DrawLine を呼び出す。
inline void draw_line_f(float x1, float y1, float x2, float y2, Color color) {
    DrawLineEx({snap_pixel(x1), snap_pixel(y1)},
               {snap_pixel(x2), snap_pixel(y2)},
               snap_stroke_width(1.0f), color);
}

inline void draw_line_ex(Vector2 start, Vector2 end, float thick, Color color) {
    DrawLineEx({snap_pixel(start.x), snap_pixel(start.y)},
               {snap_pixel(end.x), snap_pixel(end.y)},
               snap_stroke_width(thick), color);
}

inline void draw_triangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    DrawTriangle({snap_pixel(a.x), snap_pixel(a.y)},
                 {snap_pixel(b.x), snap_pixel(b.y)},
                 {snap_pixel(c.x), snap_pixel(c.y)},
                 color);
}

inline void draw_rect_pro(Rectangle rect, Vector2 origin, float rotation, Color color) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }
    DrawRectanglePro(rect, origin, rotation, color);
}

inline Rectangle texture_source_rect(const Texture2D& texture, bool flip_y = false) {
    const float width = static_cast<float>(texture.width);
    const float height = static_cast<float>(texture.height);
    return {0.0f, flip_y ? height : 0.0f, width, flip_y ? -height : height};
}

inline void draw_texture(const Texture2D& texture, Rectangle source, Rectangle dest,
                         Color tint = WHITE, Vector2 origin = {0.0f, 0.0f}, float rotation = 0.0f) {
    if (texture.id == 0 || source.width == 0.0f || source.height == 0.0f ||
        dest.width <= 0.0f || dest.height <= 0.0f) {
        return;
    }
    DrawTexturePro(texture, source, dest, origin, rotation, tint);
}

inline void draw_texture(const Texture2D& texture, Rectangle dest, Color tint = WHITE) {
    draw_texture(texture, texture_source_rect(texture), dest, tint);
}

// float 座標で DrawRectangle を呼び出す。
inline void draw_rect_f(float x, float y, float w, float h, Color color) {
    const Rectangle rect = snap_rect({x, y, w, h});
    DrawRectangleRec(rect, color);
}

// Rectangle で DrawRectangle を呼び出す。
inline void draw_rect_f(Rectangle rect, Color color) {
    DrawRectangleRec(snap_rect(rect), color);
}

inline void draw_rect_lines(Rectangle rect, float line_thick, Color color) {
    DrawRectangleLinesEx(snap_rect(rect), snap_stroke_width(line_thick), color);
}

// 位置は floor、右端と下端は ceil で広めに取りたい矩形描画用。
inline void draw_rect_span(Rectangle rect, Color color) {
    const int x = static_cast<int>(std::floor(rect.x));
    const int y = static_cast<int>(std::floor(rect.y));
    const int w = std::max(1, static_cast<int>(std::ceil(rect.x + rect.width) - std::floor(rect.x)));
    const int h = std::max(1, static_cast<int>(std::ceil(rect.y + rect.height) - std::floor(rect.y)));
    DrawRectangle(x, y, w, h, color);
}

// Rectangle から安全なシザー領域を開始する。
// floor/ceil で端数を広めに取り、空矩形は画面外 1px に逃がして描画を無効化する。
inline void begin_scissor_rect(Rectangle rect) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        BeginScissorMode(virtual_screen::current_render_width(), virtual_screen::current_render_height(), 1, 1);
        return;
    }

    const float render_scale = virtual_screen::current_render_scale();
    const Rectangle scaled = {
        rect.x * render_scale,
        rect.y * render_scale,
        rect.width * render_scale,
        rect.height * render_scale,
    };
    const int sx = static_cast<int>(std::floor(scaled.x));
    const int sy = static_cast<int>(std::floor(scaled.y));
    const int sw = std::max(1, static_cast<int>(std::ceil(scaled.x + scaled.width) - std::floor(scaled.x)));
    const int sh = std::max(1, static_cast<int>(std::ceil(scaled.y + scaled.height) - std::floor(scaled.y)));
    BeginScissorMode(sx, sy, sw, sh);
}

}  // namespace ui
