#pragma once

#include "raylib.h"
#include "ui_layout.h"

// テキスト配置ユーティリティ。
// MeasureText を使用するため constexpr 不可（ランタイム専用）。
namespace ui {

// rect 内でテキストを描画する座標 (x, y) を返す。
// 水平方向は align に従い、垂直方向は中央揃え。
inline Vector2 text_position(const char* text, int font_size, Rectangle rect,
                             text_align align = text_align::center) {
    const int text_width = MeasureText(text, font_size);
    float x;
    switch (align) {
        case text_align::left:
            x = rect.x;
            break;
        case text_align::center:
            x = rect.x + (rect.width - static_cast<float>(text_width)) * 0.5f;
            break;
        case text_align::right:
            x = rect.x + rect.width - static_cast<float>(text_width);
            break;
    }
    const float y = rect.y + (rect.height - static_cast<float>(font_size)) * 0.5f;
    return {x, y};
}

// rect 内にテキストを描画する。水平方向は align、垂直方向は中央揃え。
inline void draw_text_in_rect(const char* text, int font_size, Rectangle rect,
                              Color color, text_align align = text_align::center) {
    const Vector2 pos = text_position(text, font_size, rect, align);
    DrawText(text, static_cast<int>(pos.x), static_cast<int>(pos.y), font_size, color);
}

}  // namespace ui
