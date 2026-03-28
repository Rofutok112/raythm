#include "scene_common.h"

#include <algorithm>
#include <cmath>

void draw_scene_frame(const char* title, const char* subtitle, Color accent) {
    ClearBackground({18, 24, 38, 255});

    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, {28, 36, 54, 255}, {12, 16, 26, 255});
    DrawRectangleRounded({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, {245, 247, 250, 235});
    DrawRectangleRoundedLinesEx({80.0f, 80.0f, 1120.0f, 560.0f}, 0.04f, 8, 3.0f, accent);

    DrawText(title, 130, 130, 44, accent);
    DrawText(subtitle, 130, 190, 24, DARKGRAY);
}

void draw_marquee_text(const char* text, int x, int y, int font_size, Color color, float max_width, double time,
                       const Rectangle* parent_clip) {
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

    // マーキー自体のクリップ領域
    float clip_x = static_cast<float>(x);
    float clip_y = static_cast<float>(y);
    float clip_r = clip_x + max_width;
    float clip_b = clip_y + static_cast<float>(font_size + 4);

    // 親のクリップ領域が指定されていれば交差を取る
    if (parent_clip != nullptr) {
        clip_x = std::max(clip_x, parent_clip->x);
        clip_y = std::max(clip_y, parent_clip->y);
        clip_r = std::min(clip_r, parent_clip->x + parent_clip->width);
        clip_b = std::min(clip_b, parent_clip->y + parent_clip->height);
        if (clip_r <= clip_x || clip_b <= clip_y) {
            return;
        }
    }

    BeginScissorMode(static_cast<int>(clip_x), static_cast<int>(clip_y),
                     static_cast<int>(clip_r - clip_x), static_cast<int>(clip_b - clip_y));
    DrawText(text, x - static_cast<int>(offset), y, font_size, color);
    EndScissorMode();

    // 親のシザーを復元する
    if (parent_clip != nullptr) {
        BeginScissorMode(static_cast<int>(parent_clip->x), static_cast<int>(parent_clip->y),
                         static_cast<int>(parent_clip->width), static_cast<int>(parent_clip->height));
    }
}
