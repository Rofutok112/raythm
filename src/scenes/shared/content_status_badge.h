#pragma once

#include "data_models.h"
#include "raylib.h"
#include "theme.h"
#include "ui_draw.h"

namespace content_status_badge {

inline const char* label(content_status status) {
    switch (status) {
        case content_status::official: return "Official";
        case content_status::community: return "Community";
        case content_status::update: return "Update";
        case content_status::modified: return "Modified";
        case content_status::checking: return "Checking";
        case content_status::local: return "Local";
    }
    return "Local";
}

inline Color color(content_status status) {
    const auto& theme = *g_theme;
    switch (status) {
        case content_status::official: return theme.success;
        case content_status::community: return theme.rank_c;
        case content_status::update: return theme.accent;
        case content_status::modified: return theme.slow;
        case content_status::checking: return theme.text_secondary;
        case content_status::local: return theme.text_muted;
    }
    return theme.text_muted;
}

inline void draw(Rectangle rect, content_status status, unsigned char alpha, int font_size = 12) {
    const Color accent = color(status);
    const Color fill = with_alpha(accent, static_cast<unsigned char>((static_cast<int>(alpha) * 28) / 255));
    const Color border = with_alpha(accent, static_cast<unsigned char>((static_cast<int>(alpha) * 190) / 255));
    const Color text = with_alpha(accent, alpha);
    DrawRectangleRounded(rect, 0.35f, 6, fill);
    DrawRectangleRoundedLinesEx(rect, 0.35f, 6, 1.2f, border);
    ui::draw_text_in_rect(label(status), font_size, rect, text, ui::text_align::center);
}

}  // namespace content_status_badge
