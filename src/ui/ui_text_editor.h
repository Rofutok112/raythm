#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "ui_draw.h"

namespace ui {

struct text_editor_style {
    int font_size = 14;
    float line_spacing = 4.0f;
    float letter_spacing = 0.0f;
};

struct text_editor_span {
    std::string text;
    Color color;
};

using text_editor_highlighter = std::vector<text_editor_span>(*)(const std::string&);

struct text_editor_completion_item {
    std::string label;
    std::string insert_text;
};

struct text_editor_completion_result {
    int replace_start = 0;
    int replace_end = 0;
    std::vector<text_editor_completion_item> items;
};

using text_editor_completer = text_editor_completion_result(*)(const std::vector<std::string>&, int, int);

struct text_editor_cursor {
    int line = 0;
    int col = 0;
    bool operator==(const text_editor_cursor& o) const { return line == o.line && col == o.col; }
    bool operator!=(const text_editor_cursor& o) const { return !(*this == o); }
    bool operator<(const text_editor_cursor& o) const { return line < o.line || (line == o.line && col < o.col); }
};

struct text_editor_error_marker {
    int line = 0;         // 1-based line number
    int col_start = 0;    // 0-based column (0 = whole line)
    int col_end = 0;      // 0-based end column (0 = whole line)
    std::string message;
};

struct text_editor_state {
    std::vector<std::string> lines = {""};
    int cursor_line = 0;
    int cursor_col = 0;
    float scroll_offset = 0.0f;
    bool active = false;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
    double last_input_time = 0.0;
    // Selection
    bool has_selection = false;
    text_editor_cursor sel_anchor = {};
    bool mouse_selecting = false;
    // Inline error markers (set externally)
    std::vector<text_editor_error_marker> error_markers;
    int completion_index = 0;
};

struct text_editor_result {
    bool changed = false;
    bool activated = false;
    bool deactivated = false;
};

std::string text_editor_get_text(const text_editor_state& state);
void text_editor_set_text(text_editor_state& state, const std::string& text);

text_editor_result draw_text_editor(Rectangle rect, text_editor_state& state,
                                    int max_lines = 500,
                                    const text_editor_style& style = {},
                                    text_editor_highlighter highlighter = nullptr,
                                    text_editor_completer completer = nullptr);

}  // namespace ui
