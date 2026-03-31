#include "scene_common.h"

#include <algorithm>
#include <cmath>

#include "theme.h"

namespace {

void draw_text_clipped(const char* text, float x, float y, int font_size, Color color, Rectangle clip_rect) {
    if (text == nullptr || *text == '\0' || clip_rect.width <= 0.0f || clip_rect.height <= 0.0f) {
        return;
    }

    const Font font = GetFontDefault();
    const float font_size_f = static_cast<float>(font_size);
    const float spacing = font_size_f / static_cast<float>(font.baseSize);
    const int scissor_x = static_cast<int>(std::floor(clip_rect.x));
    const int scissor_y = static_cast<int>(std::floor(clip_rect.y));
    const int scissor_width = std::max(1, static_cast<int>(std::ceil(clip_rect.x + clip_rect.width) - std::floor(clip_rect.x)));
    const int scissor_height = std::max(1, static_cast<int>(std::ceil(clip_rect.y + clip_rect.height) - std::floor(clip_rect.y)));

    BeginScissorMode(scissor_x, scissor_y, scissor_width, scissor_height);
    DrawTextEx(font, text, {x, y}, font_size_f, spacing, color);
    EndScissorMode();
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

void draw_marquee_text(const char* text, Rectangle clip_rect, int font_size, Color color, double time) {
    if (text == nullptr || *text == '\0' || clip_rect.width <= 0.0f || clip_rect.height <= 0.0f) {
        return;
    }

    const float text_width = static_cast<float>(MeasureText(text, font_size));
    const float draw_x = clip_rect.x;
    const float draw_y = clip_rect.y;

    // 収まるならそのまま描画
    if (text_width <= clip_rect.width) {
        draw_text_clipped(text, draw_x, draw_y, font_size, color, clip_rect);
        return;
    }

    // アニメーションサイクル:
    //   [0, pause]         → 先頭を表示（停止）
    //   [pause, pause+dur] → 左にスクロール
    //   [pause+dur, end]   → 末尾を表示（停止）
    //   以降ループ
    constexpr double kPauseDuration = 1.5;
    constexpr float kScrollSpeed = 60.0f;

    const float overflow = text_width - clip_rect.width;
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

    draw_text_clipped(text, draw_x - offset, draw_y, font_size, color, clip_rect);
}

void draw_marquee_text(const char* text, int x, int y, int font_size, Color color, float max_width, double time) {
    draw_marquee_text(text, {static_cast<float>(x), static_cast<float>(y), max_width, static_cast<float>(font_size)},
                      font_size, color, time);
}
