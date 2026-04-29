#include "scene_common.h"

#include <algorithm>
#include <cmath>

#include "theme.h"
#include "ui_clip.h"
#include "ui_coord.h"
#include "ui/ui_font.h"

namespace {

float measure_text_width(const char* text, int font_size) {
    if (text == nullptr || *text == '\0') {
        return 0.0f;
    }
    return ui::measure_text_size(text, static_cast<float>(font_size), 0.0f).x;
}

void draw_text_clipped(const char* text, float x, float y, int font_size, Color color, Rectangle clip_rect) {
    if (text == nullptr || *text == '\0' || clip_rect.width <= 0.0f || clip_rect.height <= 0.0f) {
        return;
    }

    ui::scoped_clip_rect clip_scope(clip_rect);
    ui::draw_text_auto(text, {x, y}, static_cast<float>(font_size), 0.0f, color);
}

}  // namespace

void draw_scene_frame(const char* title, const char* subtitle, Color accent) {
    draw_scene_background(*g_theme);
    DrawRectangleRounded({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, g_theme->panel);
    DrawRectangleRoundedLinesEx({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, 3.0f, accent);

    ui::draw_text_f(title, 130.0f, 130.0f, 44, accent);
    ui::draw_text_f(subtitle, 130.0f, 190.0f, 24, g_theme->text_secondary);
}

void draw_scene_background(const ui_theme& theme) {
    ClearBackground(theme.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, theme.bg, theme.bg_alt);
}

void draw_marquee_text(const char* text, Rectangle clip_rect, int font_size, Color color, double time) {
    if (text == nullptr || *text == '\0' || clip_rect.width <= 0.0f || clip_rect.height <= 0.0f) {
        return;
    }

    const float text_width = measure_text_width(text, font_size);
    const float draw_x = clip_rect.x;
    const float draw_y = clip_rect.y;
    constexpr float kEdgeSlack = 2.0f;

    // 収まるならそのまま描画
    if (text_width <= clip_rect.width + kEdgeSlack) {
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

    const float overflow = std::max(0.0f, text_width - clip_rect.width + kEdgeSlack);
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

void draw_marquee_text(const char* text, float x, float y, int font_size, Color color, float max_width, double time) {
    draw_marquee_text(text, {x, y, max_width, ui::text_layout_font_size(static_cast<float>(font_size))},
                      font_size, color, time);
}
