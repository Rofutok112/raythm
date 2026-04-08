#include "ui_text_editor.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "ui_coord.h"
#include "ui_layout.h"
#include "virtual_screen.h"

namespace ui {

namespace {

constexpr float kGutterPadding = 6.0f;
constexpr float kScrollbarWidth = 8.0f;
constexpr float kTextPadLeft = 4.0f;

float measure_text_width(const std::string& text, const text_editor_style& style) {
    if (text.empty()) {
        return 0.0f;
    }
    return MeasureTextEx(GetFontDefault(), text.c_str(),
                         static_cast<float>(style.font_size), style.letter_spacing).x;
}

float measure_line_prefix_width(const std::string& line, int prefix_len,
                                const text_editor_style& style, text_editor_highlighter highlighter) {
    const int clamped_prefix_len = std::clamp(prefix_len, 0, static_cast<int>(line.size()));
    if (clamped_prefix_len == 0) {
        return 0.0f;
    }
    return measure_text_width(line.substr(0, static_cast<size_t>(clamped_prefix_len)), style);
}

void draw_line_text(const std::string& line, float x, float y,
                    const text_editor_style& style, text_editor_highlighter highlighter) {
    if (line.empty()) {
        return;
    }
    if (highlighter == nullptr) {
        DrawTextEx(GetFontDefault(), line.c_str(), {x, y},
                   static_cast<float>(style.font_size), style.letter_spacing, g_theme->text);
        return;
    }

    const std::vector<text_editor_span> spans = highlighter(line);
    int consumed = 0;
    for (const auto& span : spans) {
        if (!span.text.empty()) {
            const float cursor_x = x + measure_line_prefix_width(line, consumed, style, nullptr);
            DrawTextEx(GetFontDefault(), span.text.c_str(), {cursor_x, y},
                       static_cast<float>(style.font_size), style.letter_spacing, span.color);
            consumed += static_cast<int>(span.text.size());
        }
    }
}

void clamp_cursor(text_editor_state& state) {
    state.cursor_line = std::clamp(state.cursor_line, 0,
                                   std::max(0, static_cast<int>(state.lines.size()) - 1));
    int line_len = static_cast<int>(state.lines[state.cursor_line].size());
    state.cursor_col = std::clamp(state.cursor_col, 0, line_len);
}

void ensure_cursor_visible(text_editor_state& state, float line_height, float view_height) {
    float cursor_y = state.cursor_line * line_height;
    if (cursor_y < state.scroll_offset) {
        state.scroll_offset = cursor_y;
    }
    if (cursor_y + line_height > state.scroll_offset + view_height) {
        state.scroll_offset = cursor_y + line_height - view_height;
    }
    float max_scroll = std::max(0.0f, static_cast<float>(state.lines.size()) * line_height - view_height);
    state.scroll_offset = std::clamp(state.scroll_offset, 0.0f, max_scroll);
}

bool key_action(int key) {
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

bool insert_text_at_cursor(text_editor_state& state, const std::string& text, int max_lines) {
    if (text.empty()) {
        return false;
    }

    std::string normalized;
    normalized.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                continue;
            }
            normalized += '\n';
        } else {
            normalized += text[i];
        }
    }

    if (normalized.empty()) {
        return false;
    }

    std::vector<std::string> chunks;
    std::istringstream stream(normalized);
    std::string part;
    while (std::getline(stream, part)) {
        chunks.push_back(part);
    }
    if (!normalized.empty() && normalized.back() == '\n') {
        chunks.push_back("");
    }
    if (chunks.empty()) {
        chunks.push_back("");
    }

    auto& line = state.lines[state.cursor_line];
    const std::string before = line.substr(0, state.cursor_col);
    const std::string after = line.substr(state.cursor_col);

    if (chunks.size() == 1) {
        line = before + chunks[0] + after;
        state.cursor_col += static_cast<int>(chunks[0].size());
        return true;
    }

    const int available_new_lines = std::max(0, max_lines - static_cast<int>(state.lines.size()));
    const int allowed_extra_lines = std::min(static_cast<int>(chunks.size()) - 1, available_new_lines);

    line = before + chunks[0];
    int insert_at = state.cursor_line + 1;
    for (int i = 1; i <= allowed_extra_lines; ++i) {
        state.lines.insert(state.lines.begin() + insert_at, chunks[static_cast<size_t>(i)]);
        ++insert_at;
    }

    const int last_chunk_index = allowed_extra_lines;
    state.lines[state.cursor_line + allowed_extra_lines] += after;
    state.cursor_line += allowed_extra_lines;
    state.cursor_col = static_cast<int>(chunks[static_cast<size_t>(last_chunk_index)].size());
    return true;
}

} // anonymous namespace

std::string text_editor_get_text(const text_editor_state& state) {
    std::string result;
    for (size_t i = 0; i < state.lines.size(); i++) {
        if (i > 0) result += '\n';
        result += state.lines[i];
    }
    return result;
}

void text_editor_set_text(text_editor_state& state, const std::string& text) {
    state.lines.clear();
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') line.pop_back();
        state.lines.push_back(std::move(line));
    }
    if (state.lines.empty()) state.lines.push_back("");
    state.cursor_line = 0;
    state.cursor_col = 0;
    state.scroll_offset = 0.0f;
}

text_editor_result draw_text_editor(Rectangle rect, text_editor_state& state,
                                    int max_lines,
                                    const text_editor_style& style,
                                    text_editor_highlighter highlighter) {
    text_editor_result result;
    const float line_height = static_cast<float>(style.font_size) + style.line_spacing;
    const float gutter_width = measure_text_width("000", style) + kGutterPadding * 2;

    const Rectangle scrollbar_rect = {
        rect.x + rect.width - kScrollbarWidth, rect.y, kScrollbarWidth, rect.height
    };
    const Rectangle gutter_rect = {rect.x, rect.y, gutter_width, rect.height};
    const Rectangle text_rect = {
        rect.x + gutter_width, rect.y,
        rect.width - gutter_width - kScrollbarWidth, rect.height
    };

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool hovered = CheckCollisionPointRec(mouse, rect);

    // Activation / deactivation
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hovered) {
            if (!state.active) {
                state.active = true;
                result.activated = true;
            }
        } else if (state.active) {
            state.active = false;
            result.deactivated = true;
        }
    }

    // Click to set cursor position
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, text_rect)) {
        int clicked_line = static_cast<int>((mouse.y - text_rect.y + state.scroll_offset) / line_height);
        clicked_line = std::clamp(clicked_line, 0, static_cast<int>(state.lines.size()) - 1);
        state.cursor_line = clicked_line;

        // Approximate column from x position
        float rel_x = mouse.x - text_rect.x - kTextPadLeft;
        if (rel_x <= 0) {
                state.cursor_col = 0;
            } else {
                const std::string& line = state.lines[clicked_line];
                int col = 0;
                for (int c = 0; c <= static_cast<int>(line.size()); c++) {
                    float w = measure_line_prefix_width(line, c, style, highlighter);
                    if (w > rel_x) break;
                    col = c;
                }
            state.cursor_col = col;
        }
        state.last_input_time = GetTime();
    }

    // Keyboard input
    if (state.active) {
        // Character input
        int codepoint = GetCharPressed();
        while (codepoint > 0) {
            if (codepoint >= 32 && codepoint <= 126) {
                auto& line = state.lines[state.cursor_line];
                line.insert(line.begin() + state.cursor_col, static_cast<char>(codepoint));
                state.cursor_col++;
                result.changed = true;
                state.last_input_time = GetTime();
            }
            codepoint = GetCharPressed();
        }

        // Paste
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V)) {
            const char* clipboard = GetClipboardText();
            if (clipboard != nullptr && clipboard[0] != '\0') {
                if (insert_text_at_cursor(state, clipboard, max_lines)) {
                    result.changed = true;
                    state.last_input_time = GetTime();
                }
            }
        }
        if (IsKeyPressed(KEY_INSERT) && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))) {
            const char* clipboard = GetClipboardText();
            if (clipboard != nullptr && clipboard[0] != '\0') {
                if (insert_text_at_cursor(state, clipboard, max_lines)) {
                    result.changed = true;
                    state.last_input_time = GetTime();
                }
            }
        }

        // Tab → 2 spaces
        if (IsKeyPressed(KEY_TAB)) {
            auto& line = state.lines[state.cursor_line];
            line.insert(state.cursor_col, "  ");
            state.cursor_col += 2;
            result.changed = true;
            state.last_input_time = GetTime();
        }

        // Enter → new line
        if (IsKeyPressed(KEY_ENTER) && static_cast<int>(state.lines.size()) < max_lines) {
            auto& line = state.lines[state.cursor_line];
            std::string remainder = line.substr(state.cursor_col);
            line.erase(state.cursor_col);
            state.lines.insert(state.lines.begin() + state.cursor_line + 1, remainder);
            state.cursor_line++;
            state.cursor_col = 0;
            result.changed = true;
            state.last_input_time = GetTime();
        }

        // Backspace
        if (key_action(KEY_BACKSPACE)) {
            if (state.cursor_col > 0) {
                auto& line = state.lines[state.cursor_line];
                line.erase(state.cursor_col - 1, 1);
                state.cursor_col--;
                result.changed = true;
            } else if (state.cursor_line > 0) {
                // Merge with previous line
                int prev_len = static_cast<int>(state.lines[state.cursor_line - 1].size());
                state.lines[state.cursor_line - 1] += state.lines[state.cursor_line];
                state.lines.erase(state.lines.begin() + state.cursor_line);
                state.cursor_line--;
                state.cursor_col = prev_len;
                result.changed = true;
            }
            state.last_input_time = GetTime();
        }

        // Delete
        if (key_action(KEY_DELETE)) {
            auto& line = state.lines[state.cursor_line];
            if (state.cursor_col < static_cast<int>(line.size())) {
                line.erase(state.cursor_col, 1);
                result.changed = true;
            } else if (state.cursor_line + 1 < static_cast<int>(state.lines.size())) {
                line += state.lines[state.cursor_line + 1];
                state.lines.erase(state.lines.begin() + state.cursor_line + 1);
                result.changed = true;
            }
            state.last_input_time = GetTime();
        }

        // Arrow keys
        if (key_action(KEY_LEFT)) {
            if (state.cursor_col > 0) {
                state.cursor_col--;
            } else if (state.cursor_line > 0) {
                state.cursor_line--;
                state.cursor_col = static_cast<int>(state.lines[state.cursor_line].size());
            }
            state.last_input_time = GetTime();
        }
        if (key_action(KEY_RIGHT)) {
            int line_len = static_cast<int>(state.lines[state.cursor_line].size());
            if (state.cursor_col < line_len) {
                state.cursor_col++;
            } else if (state.cursor_line + 1 < static_cast<int>(state.lines.size())) {
                state.cursor_line++;
                state.cursor_col = 0;
            }
            state.last_input_time = GetTime();
        }
        if (key_action(KEY_UP)) {
            if (state.cursor_line > 0) {
                state.cursor_line--;
                int line_len = static_cast<int>(state.lines[state.cursor_line].size());
                state.cursor_col = std::min(state.cursor_col, line_len);
            }
            state.last_input_time = GetTime();
        }
        if (key_action(KEY_DOWN)) {
            if (state.cursor_line + 1 < static_cast<int>(state.lines.size())) {
                state.cursor_line++;
                int line_len = static_cast<int>(state.lines[state.cursor_line].size());
                state.cursor_col = std::min(state.cursor_col, line_len);
            }
            state.last_input_time = GetTime();
        }

        // Home / End
        if (IsKeyPressed(KEY_HOME)) {
            state.cursor_col = 0;
            state.last_input_time = GetTime();
        }
        if (IsKeyPressed(KEY_END)) {
            state.cursor_col = static_cast<int>(state.lines[state.cursor_line].size());
            state.last_input_time = GetTime();
        }

        clamp_cursor(state);
        ensure_cursor_visible(state, line_height, rect.height);
    }

    // Mouse wheel scroll
    if (hovered) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            state.scroll_offset -= wheel * line_height * 3.0f;
            float content_h = static_cast<float>(state.lines.size()) * line_height;
            float max_scroll = std::max(0.0f, content_h - rect.height);
            state.scroll_offset = std::clamp(state.scroll_offset, 0.0f, max_scroll);
        }
    }

    // Scrollbar interaction
    float content_height = static_cast<float>(state.lines.size()) * line_height;
    auto sb = update_vertical_scrollbar(scrollbar_rect, content_height, state.scroll_offset,
                                        state.scrollbar_dragging, state.scrollbar_drag_offset);
    if (sb.changed) {
        state.scroll_offset = sb.scroll_offset;
    }

    // ---- Rendering ----
    begin_scissor_rect(rect);

    // Background
    DrawRectangleRec(rect, g_theme->section);
    DrawRectangleLinesEx(rect, 1.5f, state.active ? g_theme->border_active : g_theme->border_light);

    // Gutter background
    Rectangle gutter_bg = {rect.x + 1.5f, rect.y + 1.5f, gutter_width - 1.5f, rect.height - 3.0f};
    DrawRectangleRec(gutter_bg, with_alpha(g_theme->panel, 200));

    // Visible line range
    int first_visible = std::max(0, static_cast<int>(state.scroll_offset / line_height));
    int last_visible = std::min(static_cast<int>(state.lines.size()) - 1,
                                static_cast<int>((state.scroll_offset + rect.height) / line_height));

    for (int i = first_visible; i <= last_visible; i++) {
        float y = rect.y + i * line_height - state.scroll_offset;

        // Line number (right-aligned in gutter)
        const char* line_num_text = TextFormat("%3d", i + 1);
        float num_width = measure_text_width(line_num_text, style);
        float num_x = rect.x + gutter_width - kGutterPadding - num_width;
        DrawText(line_num_text, static_cast<int>(num_x), static_cast<int>(y + style.line_spacing * 0.5f),
                 style.font_size, g_theme->text_dim);

        // Line text
        if (!state.lines[i].empty()) {
            draw_line_text(state.lines[i],
                           text_rect.x + kTextPadLeft,
                           y + style.line_spacing * 0.5f,
                           style, highlighter);
        }
    }

    // Cursor (blinking)
    if (state.active) {
        double blink = GetTime() - state.last_input_time;
        bool show_cursor = (std::fmod(blink, 1.0) < 0.6) || blink < 0.4;
        if (show_cursor) {
            const std::string& cur_line = state.lines[state.cursor_line];
            float cursor_x = text_rect.x + kTextPadLeft +
                            measure_line_prefix_width(cur_line, state.cursor_col, style, highlighter);
            float cursor_y = rect.y + state.cursor_line * line_height - state.scroll_offset;
            DrawRectangle(static_cast<int>(cursor_x), static_cast<int>(cursor_y + 1),
                          2, static_cast<int>(line_height - 2), g_theme->text);
        }
    }

    // Scrollbar
    draw_scrollbar(scrollbar_rect, content_height, state.scroll_offset,
                   g_theme->scrollbar_track, g_theme->scrollbar_thumb);

    EndScissorMode();

    return result;
}

}  // namespace ui
