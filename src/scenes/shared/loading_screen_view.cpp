#include "shared/loading_screen_view.h"

#include <algorithm>

#include "theme.h"
#include "ui_draw.h"

namespace {

constexpr Rectangle kDefaultPanelRect{690.0f, 702.0f, 540.0f, 112.0f};
constexpr Rectangle kHintPanelRect{690.0f, 702.0f, 540.0f, 122.0f};

}  // namespace

namespace loading_screen_view {

layout default_layout() {
    return {
        {kDefaultPanelRect.x, kDefaultPanelRect.y, kDefaultPanelRect.width, 38.0f},
        {kDefaultPanelRect.x, kDefaultPanelRect.y + 36.0f, kDefaultPanelRect.width, 28.0f},
        {kDefaultPanelRect.x + 2.0f, kDefaultPanelRect.y + 82.0f, kDefaultPanelRect.width - 4.0f, 8.0f},
        {kDefaultPanelRect.x, kDefaultPanelRect.y + 100.0f, kDefaultPanelRect.width, 28.0f},
    };
}

layout default_layout_with_hint() {
    return {
        {kHintPanelRect.x, kHintPanelRect.y, kHintPanelRect.width, 38.0f},
        {kHintPanelRect.x, kHintPanelRect.y + 38.0f, kHintPanelRect.width, 30.0f},
        {kHintPanelRect.x + 2.0f, kHintPanelRect.y + 84.0f, kHintPanelRect.width - 4.0f, 8.0f},
        {
            kHintPanelRect.x - 120.0f,
            kHintPanelRect.y + 100.0f,
            kHintPanelRect.width + 240.0f,
            28.0f,
        },
    };
}

void draw(const config& config) {
    const float progress = std::clamp(config.progress, 0.0f, 1.0f);
    const Color tone = config.error ? g_theme->error : g_theme->accent;
    const Color detail_color = config.error ? g_theme->error : g_theme->text_muted;

    ui::draw_display_text_in_rect(config.title, 28, config.geometry.title_rect, g_theme->text);
    ui::draw_text_in_rect(config.message, 18, config.geometry.detail_rect, detail_color);
    ui::draw_progress_bar(config.geometry.bar_rect,
                          progress,
                          with_alpha(g_theme->row, 180),
                          tone,
                          with_alpha(g_theme->border, 180),
                          1.5f,
                          1.5f);

    if (config.hint != nullptr && config.hint[0] != '\0') {
        ui::draw_text_in_rect(config.hint, 15, config.geometry.hint_rect, g_theme->text_hint);
    }
}

}  // namespace loading_screen_view
