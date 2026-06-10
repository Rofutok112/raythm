#pragma once

#include "raylib.h"

namespace loading_screen_view {

struct layout {
    Rectangle title_rect;
    Rectangle detail_rect;
    Rectangle bar_rect;
    Rectangle hint_rect;
};

[[nodiscard]] layout default_layout();
[[nodiscard]] layout default_layout_with_hint();

struct config {
    const char* title = "raythm";
    const char* message = "";
    const char* hint = nullptr;
    float progress = 0.0f;
    bool error = false;
    layout geometry = default_layout();
};

void draw(const config& config);

}  // namespace loading_screen_view
