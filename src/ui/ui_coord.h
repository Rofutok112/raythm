#pragma once

#include <algorithm>
#include <cmath>

#include "raylib.h"

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

// float 座標で DrawText を呼び出す。
inline void draw_text_f(const char* text, float x, float y, int font_size, Color color) {
    DrawText(text, to_i(x), to_i(y), font_size, color);
}

// float 座標で DrawLine を呼び出す。
inline void draw_line_f(float x1, float y1, float x2, float y2, Color color) {
    DrawLine(to_i(x1), to_i(y1), to_i(x2), to_i(y2), color);
}

// float 座標で DrawRectangle を呼び出す。
inline void draw_rect_f(float x, float y, float w, float h, Color color) {
    DrawRectangle(to_i(x), to_i(y), to_i(w), to_i(h), color);
}

// Rectangle で DrawRectangle を呼び出す。
inline void draw_rect_f(Rectangle rect, Color color) {
    DrawRectangle(to_i(rect.x), to_i(rect.y), to_i(rect.width), to_i(rect.height), color);
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
// floor/ceil で端数を広めに取り、幅・高さは最低 1 を保証する。
inline void begin_scissor_rect(Rectangle rect) {
    const int sx = static_cast<int>(std::floor(rect.x));
    const int sy = static_cast<int>(std::floor(rect.y));
    const int sw = std::max(1, static_cast<int>(std::ceil(rect.x + rect.width) - std::floor(rect.x)));
    const int sh = std::max(1, static_cast<int>(std::ceil(rect.y + rect.height) - std::floor(rect.y)));
    BeginScissorMode(sx, sy, sw, sh);
}

}  // namespace ui
