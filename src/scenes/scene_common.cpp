#include "scene_common.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "theme.h"
#include "ui_clip.h"
#include "ui_coord.h"
#include "ui_draw.h"
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
    ui::rounded_surface({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, g_theme->panel, accent, 3.0f);

    ui::draw_text_f(title, 130.0f, 130.0f, 44, accent);
    ui::draw_text_f(subtitle, 130.0f, 190.0f, 24, g_theme->text_secondary);
}

void draw_scene_background(const ui_theme& theme) {
    ClearBackground(theme.bg);
    ui::vertical_gradient({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                          theme.bg, theme.bg_alt);
}

Color difficulty_level_color(float level) {
    if (level <= 0.0f) {
        return g_theme->text_muted;
    }

    struct level_stop {
        float level;
        Color color;
    };
    static constexpr level_stop kStops[] = {
        {1.0f, {80, 188, 82, 255}},
        {3.0f, {186, 211, 43, 255}},
        {5.0f, {232, 166, 41, 255}},
        {6.5f, {235, 92, 58, 255}},
        {7.7f, {223, 49, 120, 255}},
        {8.5f, {158, 65, 223, 255}},
        {10.0f, {106, 54, 168, 255}},
        {12.0f, {58, 38, 84, 255}},
    };

    if (level <= kStops[0].level) {
        return kStops[0].color;
    }
    for (size_t i = 1; i < std::size(kStops); ++i) {
        if (level <= kStops[i].level) {
            const level_stop& from = kStops[i - 1];
            const level_stop& to = kStops[i];
            const float t = std::clamp((level - from.level) / (to.level - from.level), 0.0f, 1.0f);
            return lerp_color(from.color, to.color, t);
        }
    }
    return kStops[std::size(kStops) - 1].color;
}

void draw_difficulty_level_badge(float level, Rectangle rect, int font_size, unsigned char alpha) {
    if (rect.width <= 0.0f || rect.height <= 0.0f || alpha == 0) {
        return;
    }
    const Color base = difficulty_level_color(level);
    const Color accent = lerp_color(base, WHITE, level >= 10.0f ? 0.22f : 0.0f);
    const Color fill = with_alpha(accent, static_cast<unsigned char>((static_cast<int>(alpha) * 28) / 255));
    const Color border = with_alpha(accent, static_cast<unsigned char>((static_cast<int>(alpha) * 190) / 255));
    const Color text_color = with_alpha(accent, alpha);

    ui::surface(rect, fill, border, 1.2f);

    const char* label = level <= 0.0f ? "Lv...." : TextFormat("Lv.%.1f", level);
    const Vector2 text_size = ui::measure_text_size(label, static_cast<float>(font_size), 0.0f);
    ui::draw_text_auto(label,
                       {rect.x + (rect.width - text_size.x) * 0.5f,
                        rect.y + (rect.height - text_size.y) * 0.5f - 1.0f},
                       static_cast<float>(font_size), 0.0f, text_color);
}

void draw_marquee_text(const char* text, Rectangle clip_rect, int font_size, Color color, double time,
                       ui::text_align align) {
    if (text == nullptr || *text == '\0' || clip_rect.width <= 0.0f || clip_rect.height <= 0.0f) {
        return;
    }

    const float text_width = measure_text_width(text, font_size);
    float draw_x = clip_rect.x;
    if (text_width <= clip_rect.width) {
        switch (align) {
        case ui::text_align::left:
            draw_x = clip_rect.x;
            break;
        case ui::text_align::center:
            draw_x = clip_rect.x + (clip_rect.width - text_width) * 0.5f;
            break;
        case ui::text_align::right:
            draw_x = clip_rect.x + clip_rect.width - text_width;
            break;
        }
    }
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
                      font_size, color, time, ui::text_align::left);
}
