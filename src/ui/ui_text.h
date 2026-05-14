#pragma once

#include "raylib.h"
#include "localization/localization.h"
#include "ui_coord.h"
#include "ui_font.h"
#include "ui_layout.h"

// テキスト配置ユーティリティ。
// MeasureText を使用するため constexpr 不可（ランタイム専用）。
namespace ui {

// rect 内でテキストを描画する座標 (x, y) を返す。
// 水平方向は align に従い、垂直方向は中央揃え。
inline Vector2 text_position(text_role role, const char* text, int font_size, Rectangle rect,
                             text_align align = text_align::center) {
    const float layout_font_size = text_layout_font_size(static_cast<float>(font_size));
    const float text_width = measure_text_size(role, text, static_cast<float>(font_size), 0.0f).x;
    float x;
    switch (align) {
        case text_align::left:
            x = rect.x;
            break;
        case text_align::center:
            x = rect.x + (rect.width - text_width) * 0.5f;
            break;
        case text_align::right:
            x = rect.x + rect.width - text_width;
            break;
    }
    const float y = rect.y + (rect.height - layout_font_size) * 0.5f;
    return {x, y};
}

inline Vector2 text_position(const char* text, int font_size, Rectangle rect,
                             text_align align = text_align::center) {
    text = localization::tr_literal(text);
    return text_position(text_role::ui_body, text, font_size, rect, align);
}

// rect 内にテキストを描画する。水平方向は align、垂直方向は中央揃え。
inline void draw_text_in_rect(const char* text, int font_size, Rectangle rect,
                              Color color, text_align align = text_align::center) {
    text = localization::tr_literal(text);
    const Vector2 pos = text_position(text_role::ui_body, text, font_size, rect, align);
    draw_text_f(text, pos.x, pos.y, font_size, color);
}

inline Vector2 body_text_position(const char* text, int font_size, Rectangle rect,
                                  text_align align = text_align::center) {
    text = localization::tr_literal(text);
    return text_position(text_role::ui_body, text, font_size, rect, align);
}

inline void draw_body_text_in_rect(const char* text, int font_size, Rectangle rect,
                                   Color color, text_align align = text_align::center) {
    text = localization::tr_literal(text);
    const Vector2 pos = text_position(text_role::ui_body, text, font_size, rect, align);
    draw_text_body(text, pos, static_cast<float>(font_size), 0.0f, color);
}

inline Vector2 display_text_position(const char* text, int font_size, Rectangle rect,
                                     text_align align = text_align::center) {
    return text_position(text_role::display, text, font_size, rect, align);
}

inline void draw_display_text_in_rect(const char* text, int font_size, Rectangle rect,
                                      Color color, text_align align = text_align::center) {
    const Vector2 pos = display_text_position(text, font_size, rect, align);
    draw_text_display(text, pos, static_cast<float>(font_size), 0.0f, color);
}

}  // namespace ui
