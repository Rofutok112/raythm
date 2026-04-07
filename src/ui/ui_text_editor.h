#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "ui_draw.h"

namespace ui {

struct text_editor_state {
    std::vector<std::string> lines = {""};
    int cursor_line = 0;
    int cursor_col = 0;
    float scroll_offset = 0.0f;
    bool active = false;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
    double last_input_time = 0.0;
};

struct text_editor_result {
    bool changed = false;
    bool activated = false;
    bool deactivated = false;
};

std::string text_editor_get_text(const text_editor_state& state);
void text_editor_set_text(text_editor_state& state, const std::string& text);

text_editor_result draw_text_editor(Rectangle rect, text_editor_state& state,
                                    int font_size = 14, int max_lines = 500);

}  // namespace ui
