#include "scene_common.h"

#include <algorithm>
#include <cmath>

#include "theme.h"

namespace {

float measure_glyph_advance(const Font& font, int glyph_index, float scale) {
    const int advance = font.glyphs[glyph_index].advanceX;
    const float fallback_width = font.recs[glyph_index].width;
    return static_cast<float>(advance != 0 ? advance : fallback_width) * scale;
}

void draw_text_clipped_horizontally(const char* text, float x, int y, int font_size, Color color, float clip_left, float clip_right) {
    const Font font = GetFontDefault();
    const float scale = static_cast<float>(font_size) / static_cast<float>(font.baseSize);
    const float letter_spacing = scale;
    float cursor_x = x;
    const char* ptr = text;

    while (*ptr != '\0') {
        int codepoint_bytes = 0;
        const int codepoint = GetCodepointNext(ptr, &codepoint_bytes);
        if (codepoint_bytes <= 0) {
            break;
        }

        ptr += codepoint_bytes;

        if (codepoint == '\n') {
            break;
        }

        const int glyph_index = GetGlyphIndex(font, codepoint);
        const float advance = measure_glyph_advance(font, glyph_index, scale);
        const float step = advance + letter_spacing;
        const float glyph_left = cursor_x;
        const float glyph_right = cursor_x + step;

        if (glyph_right > clip_left && glyph_left < clip_right) {
            DrawTextCodepoint(font, codepoint, {cursor_x, static_cast<float>(y)}, static_cast<float>(font_size), color);
        }

        cursor_x += step;
        if (cursor_x >= clip_right) {
            break;
        }
    }
}

}  // namespace

void draw_scene_frame(const char* title, const char* subtitle, Color accent) {
    ClearBackground(g_theme->bg);

    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, g_theme->bg, g_theme->bg_alt);
    DrawRectangleRounded({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, g_theme->panel);
    DrawRectangleRoundedLinesEx({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, 3.0f, accent);

    DrawText(title, 130, 130, 44, accent);
    DrawText(subtitle, 130, 190, 24, g_theme->text_secondary);
}

void draw_marquee_text(const char* text, int x, int y, int font_size, Color color, float max_width, double time) {
    if (text == nullptr || *text == '\0' || max_width <= 0.0f) {
        return;
    }

    const float text_width = static_cast<float>(MeasureText(text, font_size));

    // 収まるならそのまま描画
    if (text_width <= max_width) {
        DrawText(text, x, y, font_size, color);
        return;
    }

    // アニメーションサイクル:
    //   [0, pause]         → 先頭を表示（停止）
    //   [pause, pause+dur] → 左にスクロール
    //   [pause+dur, end]   → 末尾を表示（停止）
    //   以降ループ
    constexpr double kPauseDuration = 1.5;
    constexpr float kScrollSpeed = 60.0f;

    const float overflow = text_width - max_width;
    const double scroll_duration = static_cast<double>(overflow / kScrollSpeed);
    const double cycle = kPauseDuration + scroll_duration + kPauseDuration;
    const double t = std::fmod(std::fabs(time), cycle);

    float offset = 0.0f;
    if (t < kPauseDuration) {
        offset = 0.0f;
    } else if (t < kPauseDuration + scroll_duration) {
        offset = static_cast<float>(t - kPauseDuration) * kScrollSpeed;
    } else {
        offset = overflow;
    }

    draw_text_clipped_horizontally(text, static_cast<float>(x) - offset, y, font_size, color,
                                   static_cast<float>(x), static_cast<float>(x) + max_width);
}
