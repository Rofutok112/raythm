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

void draw_squiggly_line(float x0, float x1, float y, Color color) {
    constexpr float wave_width = 4.0f;
    constexpr float wave_height = 2.0f;
    float x = x0;
    while (x < x1) {
        float nx = std::min(x + wave_width * 0.5f, x1);
        float y0 = y;
        float y1 = y + ((static_cast<int>((x - x0) / (wave_width * 0.5f)) % 2 == 0) ? wave_height : -wave_height);
        DrawLineEx({x, y0}, {nx, y1}, 1.5f, color);
        x = nx;
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

bool replace_range_on_line(text_editor_state& state, int line_index, int col_start, int col_end,
                           const std::string& text) {
    if (line_index < 0 || line_index >= static_cast<int>(state.lines.size())) {
        return false;
    }
    std::string& line = state.lines[line_index];
    const int start = std::clamp(col_start, 0, static_cast<int>(line.size()));
    const int end = std::clamp(col_end, start, static_cast<int>(line.size()));
    line.replace(static_cast<size_t>(start), static_cast<size_t>(end - start), text);
    state.cursor_line = line_index;
    state.cursor_col = start + static_cast<int>(text.size());
    state.has_selection = false;
    return true;
}

text_editor_cursor cursor_of(const text_editor_state& state) {
    return {state.cursor_line, state.cursor_col};
}

std::pair<text_editor_cursor, text_editor_cursor> selection_range(const text_editor_state& state) {
    text_editor_cursor a = state.sel_anchor;
    text_editor_cursor b = cursor_of(state);
    if (b < a) std::swap(a, b);
    return {a, b};
}

std::string get_selection_text(const text_editor_state& state) {
    if (!state.has_selection) return {};
    auto [from, to] = selection_range(state);
    if (from.line == to.line) {
        return state.lines[from.line].substr(from.col, to.col - from.col);
    }
    std::string result = state.lines[from.line].substr(from.col) + "\n";
    for (int i = from.line + 1; i < to.line; i++) {
        result += state.lines[i] + "\n";
    }
    result += state.lines[to.line].substr(0, to.col);
    return result;
}

void delete_selection(text_editor_state& state) {
    if (!state.has_selection) return;
    auto [from, to] = selection_range(state);
    if (from.line == to.line) {
        state.lines[from.line].erase(from.col, to.col - from.col);
    } else {
        std::string merged = state.lines[from.line].substr(0, from.col) +
                             state.lines[to.line].substr(to.col);
        state.lines.erase(state.lines.begin() + from.line, state.lines.begin() + to.line + 1);
        state.lines.insert(state.lines.begin() + from.line, merged);
    }
    state.cursor_line = from.line;
    state.cursor_col = from.col;
    state.has_selection = false;
    if (state.lines.empty()) state.lines.push_back("");
}

void start_selection_if_shift(text_editor_state& state, bool shift) {
    if (shift) {
        if (!state.has_selection) {
            state.has_selection = true;
            state.sel_anchor = cursor_of(state);
        }
    } else {
        state.has_selection = false;
    }
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
                                    text_editor_highlighter highlighter,
                                    text_editor_completer completer) {
    text_editor_result result;
    bool should_ensure_cursor_visible = false;
    const float line_height = static_cast<float>(style.font_size) + style.line_spacing;
    const float gutter_width = measure_text_width("000", style) + kGutterPadding * 2;
    constexpr int kMaxCompletionItems = 8;
    constexpr float kCompletionRowHeight = 26.0f;
    constexpr float kCompletionPadX = 8.0f;
    constexpr float kCompletionMinWidth = 180.0f;
    constexpr float kCompletionOffsetX = 18.0f;
    const float completion_font_size = static_cast<float>(std::max(style.font_size - 2, 12));
    const float completion_letter_spacing = std::max(style.letter_spacing, 1.0f);

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
    text_editor_completion_result completion;
    if (completer != nullptr && state.active && !state.mouse_selecting) {
        completion = completer(state.lines, state.cursor_line, state.cursor_col);
        if (!completion.items.empty()) {
            if (state.completion_index < 0) {
                state.completion_index = 0;
            }
            state.completion_index = std::min(state.completion_index,
                                              static_cast<int>(completion.items.size()) - 1);
        } else {
            state.completion_index = 0;
        }
    } else {
        state.completion_index = 0;
    }

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

    // Helper: compute line/col from mouse position
    auto mouse_to_cursor = [&](Vector2 m) -> text_editor_cursor {
        int line = static_cast<int>((m.y - text_rect.y + state.scroll_offset) / line_height);
        line = std::clamp(line, 0, static_cast<int>(state.lines.size()) - 1);
        float rel_x = m.x - text_rect.x - kTextPadLeft;
        int col = 0;
        if (rel_x > 0) {
            const std::string& ln = state.lines[line];
            for (int c = 0; c <= static_cast<int>(ln.size()); c++) {
                float w = measure_line_prefix_width(ln, c, style, highlighter);
                if (w > rel_x) break;
                col = c;
            }
        }
        return {line, col};
    };

    // Click to set cursor position + start mouse selection
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, text_rect)) {
        auto pos = mouse_to_cursor(mouse);
        state.cursor_line = pos.line;
        state.cursor_col = pos.col;
        state.sel_anchor = pos;
        state.has_selection = false;
        state.mouse_selecting = true;
        state.last_input_time = GetTime();
        should_ensure_cursor_visible = true;
    }

    // Drag to extend selection
    if (state.mouse_selecting && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        auto pos = mouse_to_cursor(mouse);
        state.cursor_line = pos.line;
        state.cursor_col = pos.col;
        if (pos != state.sel_anchor) {
            state.has_selection = true;
        }
        state.last_input_time = GetTime();
        should_ensure_cursor_visible = true;
    }

    // Release mouse button ends drag
    if (state.mouse_selecting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        state.mouse_selecting = false;
    }

    // Keyboard input
    if (state.active) {
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const bool has_completion = !completion.items.empty();
        bool completion_navigation_handled = false;

        auto accept_completion = [&]() -> bool {
            if (!has_completion || state.completion_index < 0 ||
                state.completion_index >= static_cast<int>(completion.items.size())) {
                return false;
            }
            if (replace_range_on_line(state, state.cursor_line,
                                      completion.replace_start, completion.replace_end,
                                      completion.items[static_cast<size_t>(state.completion_index)].insert_text)) {
                result.changed = true;
                state.last_input_time = GetTime();
                should_ensure_cursor_visible = true;
                return true;
            }
            return false;
        };

        // Select all: Ctrl+A
        if (ctrl && IsKeyPressed(KEY_A)) {
            state.sel_anchor = {0, 0};
            state.cursor_line = static_cast<int>(state.lines.size()) - 1;
            state.cursor_col = static_cast<int>(state.lines.back().size());
            state.has_selection = true;
            state.last_input_time = GetTime();
            should_ensure_cursor_visible = true;
        }

        // Copy: Ctrl+C
        if (ctrl && IsKeyPressed(KEY_C) && state.has_selection) {
            SetClipboardText(get_selection_text(state).c_str());
        }

        // Cut: Ctrl+X
        if (ctrl && IsKeyPressed(KEY_X) && state.has_selection) {
            SetClipboardText(get_selection_text(state).c_str());
            delete_selection(state);
            result.changed = true;
            state.last_input_time = GetTime();
        }

        if (has_completion && IsKeyPressed(KEY_UP)) {
            state.completion_index =
                (state.completion_index + static_cast<int>(completion.items.size()) - 1) %
                static_cast<int>(completion.items.size());
            state.last_input_time = GetTime();
            completion_navigation_handled = true;
        } else if (has_completion && IsKeyPressed(KEY_DOWN)) {
            state.completion_index =
                (state.completion_index + 1) % static_cast<int>(completion.items.size());
            state.last_input_time = GetTime();
            completion_navigation_handled = true;
        } else if (has_completion && (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ENTER))) {
            if (accept_completion()) {
                completion = completer != nullptr
                    ? completer(state.lines, state.cursor_line, state.cursor_col)
                    : text_editor_completion_result{};
            }
        } else {

            // Character input (replace selection if active)
            int codepoint = GetCharPressed();
            while (codepoint > 0) {
                if (codepoint >= 32 && codepoint <= 126) {
                    if (state.has_selection) {
                        delete_selection(state);
                    }
                    auto& line = state.lines[state.cursor_line];
                    line.insert(line.begin() + state.cursor_col, static_cast<char>(codepoint));
                    state.cursor_col++;
                    result.changed = true;
                    state.last_input_time = GetTime();
                    should_ensure_cursor_visible = true;
                }
                codepoint = GetCharPressed();
            }

            // Paste (replace selection if active)
            if (ctrl && IsKeyPressed(KEY_V)) {
                const char* clipboard = GetClipboardText();
                if (clipboard != nullptr && clipboard[0] != '\0') {
                    if (state.has_selection) {
                        delete_selection(state);
                    }
                    if (insert_text_at_cursor(state, clipboard, max_lines)) {
                        result.changed = true;
                        state.last_input_time = GetTime();
                        should_ensure_cursor_visible = true;
                    }
                }
            }

            // Tab → 2 spaces (replace selection if active)
            if (IsKeyPressed(KEY_TAB)) {
                if (state.has_selection) {
                    delete_selection(state);
                }
                auto& line = state.lines[state.cursor_line];
                line.insert(state.cursor_col, "  ");
                state.cursor_col += 2;
                result.changed = true;
                state.last_input_time = GetTime();
                should_ensure_cursor_visible = true;
            }

            // Enter → new line (replace selection if active)
            if (IsKeyPressed(KEY_ENTER) && static_cast<int>(state.lines.size()) < max_lines) {
                if (state.has_selection) {
                    delete_selection(state);
                }
                auto& line = state.lines[state.cursor_line];
                std::string remainder = line.substr(state.cursor_col);
                line.erase(state.cursor_col);
                state.lines.insert(state.lines.begin() + state.cursor_line + 1, remainder);
                state.cursor_line++;
                state.cursor_col = 0;
                result.changed = true;
                state.last_input_time = GetTime();
                should_ensure_cursor_visible = true;
            }
        }

        // Backspace (delete selection or single char)
        if (key_action(KEY_BACKSPACE)) {
            if (state.has_selection) {
                delete_selection(state);
                result.changed = true;
            } else if (state.cursor_col > 0) {
                auto& line = state.lines[state.cursor_line];
                line.erase(state.cursor_col - 1, 1);
                state.cursor_col--;
                result.changed = true;
            } else if (state.cursor_line > 0) {
                int prev_len = static_cast<int>(state.lines[state.cursor_line - 1].size());
                state.lines[state.cursor_line - 1] += state.lines[state.cursor_line];
                state.lines.erase(state.lines.begin() + state.cursor_line);
                state.cursor_line--;
                state.cursor_col = prev_len;
                result.changed = true;
            }
            state.last_input_time = GetTime();
            should_ensure_cursor_visible = true;
        }

        // Delete (delete selection or single char)
        if (key_action(KEY_DELETE)) {
            if (state.has_selection) {
                delete_selection(state);
                result.changed = true;
            } else {
                auto& line = state.lines[state.cursor_line];
                if (state.cursor_col < static_cast<int>(line.size())) {
                    line.erase(state.cursor_col, 1);
                    result.changed = true;
                } else if (state.cursor_line + 1 < static_cast<int>(state.lines.size())) {
                    line += state.lines[state.cursor_line + 1];
                    state.lines.erase(state.lines.begin() + state.cursor_line + 1);
                    result.changed = true;
                }
            }
            state.last_input_time = GetTime();
            should_ensure_cursor_visible = true;
        }

        // Arrow keys (with Shift for selection)
        if (key_action(KEY_LEFT)) {
            start_selection_if_shift(state, shift);
            if (state.cursor_col > 0) {
                state.cursor_col--;
            } else if (state.cursor_line > 0) {
                state.cursor_line--;
                state.cursor_col = static_cast<int>(state.lines[state.cursor_line].size());
            }
            if (!shift) state.has_selection = false;
            state.last_input_time = GetTime();
            should_ensure_cursor_visible = true;
        }
        if (key_action(KEY_RIGHT)) {
            start_selection_if_shift(state, shift);
            int line_len = static_cast<int>(state.lines[state.cursor_line].size());
            if (state.cursor_col < line_len) {
                state.cursor_col++;
            } else if (state.cursor_line + 1 < static_cast<int>(state.lines.size())) {
                state.cursor_line++;
                state.cursor_col = 0;
            }
            if (!shift) state.has_selection = false;
            state.last_input_time = GetTime();
            should_ensure_cursor_visible = true;
        }
        if (!completion_navigation_handled && key_action(KEY_UP)) {
            start_selection_if_shift(state, shift);
            if (state.cursor_line > 0) {
                state.cursor_line--;
                int line_len = static_cast<int>(state.lines[state.cursor_line].size());
                state.cursor_col = std::min(state.cursor_col, line_len);
            }
            if (!shift) state.has_selection = false;
            state.last_input_time = GetTime();
            should_ensure_cursor_visible = true;
        }
        if (!completion_navigation_handled && key_action(KEY_DOWN)) {
            start_selection_if_shift(state, shift);
            if (state.cursor_line + 1 < static_cast<int>(state.lines.size())) {
                state.cursor_line++;
                int line_len = static_cast<int>(state.lines[state.cursor_line].size());
                state.cursor_col = std::min(state.cursor_col, line_len);
            }
            if (!shift) state.has_selection = false;
            state.last_input_time = GetTime();
            should_ensure_cursor_visible = true;
        }

        // Home / End (with Shift for selection)
        if (IsKeyPressed(KEY_HOME)) {
            start_selection_if_shift(state, shift);
            state.cursor_col = 0;
            if (!shift) state.has_selection = false;
            state.last_input_time = GetTime();
            should_ensure_cursor_visible = true;
        }
        if (IsKeyPressed(KEY_END)) {
            start_selection_if_shift(state, shift);
            state.cursor_col = static_cast<int>(state.lines[state.cursor_line].size());
            if (!shift) state.has_selection = false;
            state.last_input_time = GetTime();
            should_ensure_cursor_visible = true;
        }

        clamp_cursor(state);
        if (should_ensure_cursor_visible) {
            ensure_cursor_visible(state, line_height, rect.height);
        }
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

    // Selection range (computed once for rendering)
    text_editor_cursor sel_from = {}, sel_to = {};
    if (state.has_selection) {
        auto [a, b] = selection_range(state);
        sel_from = a;
        sel_to = b;
    }

    for (int i = first_visible; i <= last_visible; i++) {
        float y = rect.y + i * line_height - state.scroll_offset;

        // Selection highlight
        if (state.has_selection && i >= sel_from.line && i <= sel_to.line) {
            int line_len = static_cast<int>(state.lines[i].size());
            int sel_start_col = (i == sel_from.line) ? sel_from.col : 0;
            int sel_end_col = (i == sel_to.line) ? sel_to.col : line_len;
            float x0 = text_rect.x + kTextPadLeft +
                        measure_line_prefix_width(state.lines[i], sel_start_col, style, highlighter);
            float x1 = text_rect.x + kTextPadLeft +
                        measure_line_prefix_width(state.lines[i], sel_end_col, style, highlighter);
            if (i != sel_to.line && sel_end_col == line_len) {
                x1 += measure_text_width(" ", style);  // extend past line end
            }
            if (x1 > x0) {
                DrawRectangle(static_cast<int>(x0), static_cast<int>(y),
                              static_cast<int>(x1 - x0), static_cast<int>(line_height),
                              Color{60, 120, 220, 80});
            }
        }

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

    // Error squiggly underlines
    for (const auto& marker : state.error_markers) {
        int line_idx = marker.line - 1;  // markers are 1-based
        if (line_idx < first_visible || line_idx > last_visible) continue;
        if (line_idx < 0 || line_idx >= static_cast<int>(state.lines.size())) continue;

        float y = rect.y + line_idx * line_height - state.scroll_offset;
        float underline_y = y + style.line_spacing * 0.5f + static_cast<float>(style.font_size) + 1.0f;

        const std::string& ln = state.lines[line_idx];
        int c0 = marker.col_start;
        int c1 = marker.col_end;
        // If col range is zero/invalid, underline the whole line
        if (c0 == 0 && c1 == 0) {
            // Find first non-space char
            c0 = 0;
            while (c0 < static_cast<int>(ln.size()) && (ln[c0] == ' ' || ln[c0] == '\t')) c0++;
            c1 = static_cast<int>(ln.size());
        }
        if (c1 <= c0) c1 = std::max(c0 + 1, static_cast<int>(ln.size()));
        c1 = std::min(c1, static_cast<int>(ln.size()));

        float x0 = text_rect.x + kTextPadLeft + measure_line_prefix_width(ln, c0, style, highlighter);
        float x1 = text_rect.x + kTextPadLeft + measure_line_prefix_width(ln, c1, style, highlighter);
        if (x1 <= x0) x1 = x0 + measure_text_width("x", style);  // minimum width

        draw_squiggly_line(x0, x1, underline_y, Color{255, 80, 80, 220});

        // Check hover for tooltip
        Rectangle squiggly_area = {x0, y, x1 - x0, line_height};
        if (CheckCollisionPointRec(mouse, squiggly_area) && !marker.message.empty()) {
            constexpr float kTooltipPad = 6.0f;
            constexpr float kTooltipFontSize = 12.0f;
            constexpr float kTooltipLetterSpacing = 0.0f;
            const Vector2 tooltip_size = MeasureTextEx(GetFontDefault(), marker.message.c_str(),
                                                       kTooltipFontSize, kTooltipLetterSpacing);
            float tw = tooltip_size.x + kTooltipPad * 2.0f;
            float th = tooltip_size.y + kTooltipPad * 2.0f;
            float tx = std::min(mouse.x, rect.x + rect.width - tw - 4.0f);
            tx = std::max(tx, rect.x + 4.0f);
            float ty = underline_y + 4.0f;
            DrawRectangleRounded({tx, ty, tw, th}, 0.2f, 4, Color{40, 40, 40, 230});
            DrawRectangleRoundedLinesEx({tx, ty, tw, th}, 0.2f, 4, 1.0f, Color{255, 80, 80, 180});
            DrawTextEx(GetFontDefault(), marker.message.c_str(),
                       {tx + kTooltipPad, ty + kTooltipPad},
                       kTooltipFontSize, kTooltipLetterSpacing, Color{255, 200, 200, 255});
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

    if (!completion.items.empty()) {
        const std::string& cur_line = state.lines[state.cursor_line];
        float cursor_x = text_rect.x + kTextPadLeft +
                         measure_line_prefix_width(cur_line, state.cursor_col, style, highlighter);
        float cursor_y = rect.y + state.cursor_line * line_height - state.scroll_offset;
        const int visible_items = std::min(static_cast<int>(completion.items.size()), kMaxCompletionItems);

        float menu_width = kCompletionMinWidth;
        for (int i = 0; i < visible_items; ++i) {
            menu_width = std::max(menu_width,
                                  MeasureTextEx(GetFontDefault(),
                                                completion.items[static_cast<size_t>(i)].label.c_str(),
                                                completion_font_size, completion_letter_spacing).x +
                                  kCompletionPadX * 2.0f);
        }
        Rectangle menu_rect = {
            std::min(cursor_x + kCompletionOffsetX, text_rect.x + text_rect.width - menu_width - 4.0f),
            std::min(cursor_y + line_height + 2.0f,
                     rect.y + rect.height - visible_items * kCompletionRowHeight - 4.0f),
            menu_width,
            visible_items * kCompletionRowHeight
        };
        if (menu_rect.y < rect.y + 4.0f) {
            menu_rect.y = rect.y + 4.0f;
        }

        DrawRectangleRec(menu_rect, with_alpha(g_theme->panel, 245));
        DrawRectangleLinesEx(menu_rect, 1.0f, g_theme->border_light);

        for (int i = 0; i < visible_items; ++i) {
            Rectangle item_rect = {
                menu_rect.x,
                menu_rect.y + i * kCompletionRowHeight,
                menu_rect.width,
                kCompletionRowHeight
            };
            const bool hovered_item = CheckCollisionPointRec(mouse, item_rect);
            const bool selected_item = i == state.completion_index;
            DrawRectangleRec(item_rect,
                             selected_item ? g_theme->row_selected :
                             (hovered_item ? g_theme->row_hover : BLANK));
            DrawTextEx(GetFontDefault(),
                       completion.items[static_cast<size_t>(i)].label.c_str(),
                       {item_rect.x + kCompletionPadX, item_rect.y + 4.0f},
                       completion_font_size, completion_letter_spacing,
                       selected_item ? g_theme->text : g_theme->text_secondary);

            if (hovered_item && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                state.completion_index = i;
                if (replace_range_on_line(state, state.cursor_line,
                                          completion.replace_start, completion.replace_end,
                                          completion.items[static_cast<size_t>(i)].insert_text)) {
                    result.changed = true;
                    state.last_input_time = GetTime();
                }
            }
        }
    }

    return result;
}

}  // namespace ui
