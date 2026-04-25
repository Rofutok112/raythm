#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"
#include "theme.h"
#include "ui_font.h"
#include "ui_layout.h"
#include "ui_text.h"

namespace ui {

enum class notice_tone {
    info,
    success,
    error,
};

struct notice_entry {
    std::string message;
    notice_tone tone = notice_tone::info;
    float remaining_seconds = 0.0f;
    float lifetime_seconds = 0.0f;
};

struct notice_queue {
    std::vector<notice_entry> items;
};

inline void clear_notices(notice_queue& queue) {
    queue.items.clear();
}

inline void push_notice(notice_queue& queue, std::string message, notice_tone tone, float lifetime_seconds = 2.0f) {
    if (message.empty()) {
        return;
    }
    queue.items.push_back({
        .message = std::move(message),
        .tone = tone,
        .remaining_seconds = lifetime_seconds,
        .lifetime_seconds = lifetime_seconds,
    });
}

inline void tick_notices(notice_queue& queue, float dt) {
    for (notice_entry& notice : queue.items) {
        notice.remaining_seconds = std::max(0.0f, notice.remaining_seconds - dt);
    }
    std::erase_if(queue.items, [](const notice_entry& notice) {
        return notice.remaining_seconds <= 0.0f;
    });
}

inline Color notice_color(notice_tone tone) {
    switch (tone) {
    case notice_tone::success:
        return g_theme->success;
    case notice_tone::error:
        return g_theme->error;
    case notice_tone::info:
    default:
        return g_theme->accent;
    }
}

inline unsigned char notice_alpha(const notice_entry& notice) {
    if (notice.lifetime_seconds <= 0.0f) {
        return 255;
    }

    constexpr float kFadeWindowSeconds = 0.18f;
    const float fade_in = std::min(1.0f, (notice.lifetime_seconds - notice.remaining_seconds) / kFadeWindowSeconds);
    const float fade_out = std::min(1.0f, notice.remaining_seconds / kFadeWindowSeconds);
    const float alpha = std::clamp(std::min(fade_in, fade_out), 0.0f, 1.0f);
    return static_cast<unsigned char>(160.0f + 95.0f * alpha);
}

inline void draw_notice_queue_bottom_right(const notice_queue& queue, Rectangle bounds,
                                           float right_margin = 36.0f, float bottom_margin = 24.0f,
                                           float max_width = 690.0f, float min_width = 210.0f,
                                           float vertical_gap = 15.0f) {
    if (queue.items.empty()) {
        return;
    }

    constexpr float kHorizontalPadding = 18.0f;
    constexpr float kVerticalPadding = 10.5f;
    constexpr int kFontSize = 18;
    const float line_height = text_layout_font_size(static_cast<float>(kFontSize));
    float bottom_y = bounds.y + bounds.height - bottom_margin;

    for (auto it = queue.items.rbegin(); it != queue.items.rend(); ++it) {
        const notice_entry& notice = *it;
        const Color tone = notice_color(notice.tone);
        const unsigned char alpha = notice_alpha(notice);
        const Vector2 measured = measure_text_size(notice.message, static_cast<float>(kFontSize));
        const float width = std::clamp(measured.x + kHorizontalPadding * 2.0f, min_width, max_width);
        const float height = std::max(45.0f,
                                      std::max(measured.y, line_height) + kVerticalPadding * 2.0f);
        const Rectangle rect = {
            bounds.x + bounds.width - right_margin - width,
            bottom_y - height,
            width,
            height
        };

        const Color background = with_alpha(lerp_color(g_theme->panel, tone, 0.12f), alpha);
        const Color border = with_alpha(lerp_color(g_theme->border, tone, 0.45f), alpha);
        const Color text_color = with_alpha(tone, alpha);

        draw_rect_f(rect, background);
        draw_rect_lines(rect, 2.0f, border);
        draw_text_in_rect(notice.message.c_str(), kFontSize,
                          inset(rect, edge_insets::symmetric(kVerticalPadding, kHorizontalPadding)),
                          text_color, text_align::right);

        bottom_y = rect.y - vertical_gap;
    }
}

}  // namespace ui
