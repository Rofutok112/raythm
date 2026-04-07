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
constexpr float kLineSpacing = 4.0f;
constexpr float kTextPadLeft = 4.0f;

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
                                    int font_size, int max_lines) {
    text_editor_result result;
    const float line_height = font_size + kLineSpacing;
    const float gutter_width = static_cast<float>(MeasureText("000", font_size)) + kGutterPadding * 2;

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
                float w = static_cast<float>(MeasureText(line.substr(0, c).c_str(), font_size));
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
        float num_width = static_cast<float>(MeasureText(line_num_text, font_size));
        float num_x = rect.x + gutter_width - kGutterPadding - num_width;
        DrawText(line_num_text, static_cast<int>(num_x), static_cast<int>(y + kLineSpacing * 0.5f),
                 font_size, g_theme->text_dim);

        // Line text
        if (!state.lines[i].empty()) {
            DrawText(state.lines[i].c_str(),
                     static_cast<int>(text_rect.x + kTextPadLeft),
                     static_cast<int>(y + kLineSpacing * 0.5f),
                     font_size, g_theme->text);
        }
    }

    // Cursor (blinking)
    if (state.active) {
        double blink = GetTime() - state.last_input_time;
        bool show_cursor = (std::fmod(blink, 1.0) < 0.6) || blink < 0.4;
        if (show_cursor) {
            const std::string& cur_line = state.lines[state.cursor_line];
            std::string before_cursor = cur_line.substr(0, state.cursor_col);
            float cursor_x = text_rect.x + kTextPadLeft +
                            static_cast<float>(MeasureText(before_cursor.c_str(), font_size));
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
