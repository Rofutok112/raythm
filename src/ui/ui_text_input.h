#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <string>
#include <string_view>

#include "raylib.h"
#include "ui_draw.h"

namespace ui {

inline constexpr float kTextInputBaselineNudge = 2.25f;
inline constexpr float kTextInputSelectionInsetY = 7.5f;
inline constexpr float kTextInputSelectionInsetTotalY = 15.0f;
inline constexpr float kTextInputCursorWidth = 2.25f;

using text_input_filter = bool (*)(int codepoint, const std::string& current_value);

struct text_input_state {
    std::string value;
    bool active = false;
    size_t cursor = 0;
    bool has_selection = false;
    size_t selection_anchor = 0;
    bool mouse_selecting = false;
    float scroll_x = 0.0f;
};

struct text_input_result {
    bool clicked = false;
    bool changed = false;
    bool submitted = false;
    bool activated = false;
    bool deactivated = false;
    bool defaulted = false;
};

inline bool default_text_input_filter(int codepoint, const std::string&) {
    return codepoint >= 32 && codepoint != 127;
}

inline void append_codepoint_utf8(std::string& value, int codepoint) {
    int codepoint_size = 0;
    const char* encoded = CodepointToUTF8(codepoint, &codepoint_size);
    if (encoded == nullptr || codepoint_size <= 0) {
        return;
    }
    value.append(encoded, static_cast<size_t>(codepoint_size));
}

inline size_t utf8_codepoint_count(const std::string& value) {
    size_t count = 0;
    for (unsigned char c : value) {
        if ((c & 0xC0u) != 0x80u) {
            ++count;
        }
    }
    return count;
}

inline size_t utf8_codepoint_to_byte_index(const std::string& value, size_t codepoint_index) {
    if (codepoint_index == 0) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if ((c & 0xC0u) != 0x80u) {
            if (count == codepoint_index) {
                return i;
            }
            ++count;
        }
    }
    return value.size();
}

inline std::string utf8_substr_codepoints(const std::string& value, size_t start_codepoint,
                                          size_t end_codepoint) {
    const size_t start = utf8_codepoint_to_byte_index(value, start_codepoint);
    const size_t end = utf8_codepoint_to_byte_index(value, end_codepoint);
    return value.substr(start, end - start);
}

inline void pop_last_utf8_codepoint(std::string& value) {
    if (value.empty()) {
        return;
    }

    size_t next_size = value.size() - 1;
    while (next_size > 0 &&
           (static_cast<unsigned char>(value[next_size]) & 0xC0u) == 0x80u) {
        --next_size;
    }
    value.resize(next_size);
}

inline bool decode_next_utf8_codepoint(std::string_view text, size_t offset,
                                       int& codepoint, size_t& next_offset) {
    if (offset >= text.size()) {
        return false;
    }

    const unsigned char c0 = static_cast<unsigned char>(text[offset]);
    if (c0 < 0x80u) {
        codepoint = c0;
        next_offset = offset + 1;
        return true;
    }

    if ((c0 & 0xE0u) == 0xC0u && offset + 1 < text.size()) {
        const unsigned char c1 = static_cast<unsigned char>(text[offset + 1]);
        codepoint = ((c0 & 0x1Fu) << 6) | (c1 & 0x3Fu);
        next_offset = offset + 2;
        return true;
    }

    if ((c0 & 0xF0u) == 0xE0u && offset + 2 < text.size()) {
        const unsigned char c1 = static_cast<unsigned char>(text[offset + 1]);
        const unsigned char c2 = static_cast<unsigned char>(text[offset + 2]);
        codepoint = ((c0 & 0x0Fu) << 12) | ((c1 & 0x3Fu) << 6) | (c2 & 0x3Fu);
        next_offset = offset + 3;
        return true;
    }

    if ((c0 & 0xF8u) == 0xF0u && offset + 3 < text.size()) {
        const unsigned char c1 = static_cast<unsigned char>(text[offset + 1]);
        const unsigned char c2 = static_cast<unsigned char>(text[offset + 2]);
        const unsigned char c3 = static_cast<unsigned char>(text[offset + 3]);
        codepoint = ((c0 & 0x07u) << 18) | ((c1 & 0x3Fu) << 12) |
                    ((c2 & 0x3Fu) << 6) | (c3 & 0x3Fu);
        next_offset = offset + 4;
        return true;
    }

    codepoint = c0;
    next_offset = offset + 1;
    return false;
}

inline void clamp_text_input_state(text_input_state& state) {
    const size_t codepoint_count = utf8_codepoint_count(state.value);
    state.cursor = std::min(state.cursor, codepoint_count);
    state.selection_anchor = std::min(state.selection_anchor, codepoint_count);
    if (!state.has_selection || state.selection_anchor == state.cursor) {
        state.has_selection = false;
        state.selection_anchor = state.cursor;
    }
    if (!state.active) {
        state.mouse_selecting = false;
    }
    state.scroll_x = std::max(0.0f, state.scroll_x);
}

inline std::pair<size_t, size_t> text_input_selection_range(const text_input_state& state) {
    return {
        std::min(state.selection_anchor, state.cursor),
        std::max(state.selection_anchor, state.cursor)
    };
}

inline void clear_text_input_selection(text_input_state& state) {
    state.has_selection = false;
    state.selection_anchor = state.cursor;
}

inline bool delete_text_input_selection(text_input_state& state) {
    if (!state.has_selection) {
        return false;
    }

    const auto [start, end] = text_input_selection_range(state);
    const size_t start_byte = utf8_codepoint_to_byte_index(state.value, start);
    const size_t end_byte = utf8_codepoint_to_byte_index(state.value, end);
    state.value.erase(start_byte, end_byte - start_byte);
    state.cursor = start;
    clear_text_input_selection(state);
    return true;
}

inline std::string selected_text_input_text(const text_input_state& state) {
    if (!state.has_selection) {
        return {};
    }
    const auto [start, end] = text_input_selection_range(state);
    return utf8_substr_codepoints(state.value, start, end);
}

inline bool insert_codepoint_at_cursor(text_input_state& state, int codepoint) {
    int codepoint_size = 0;
    const char* encoded = CodepointToUTF8(codepoint, &codepoint_size);
    if (encoded == nullptr || codepoint_size <= 0) {
        return false;
    }

    const size_t byte_index = utf8_codepoint_to_byte_index(state.value, state.cursor);
    state.value.insert(byte_index, encoded, static_cast<size_t>(codepoint_size));
    ++state.cursor;
    clear_text_input_selection(state);
    return true;
}

inline bool paste_text_input_at_cursor(text_input_state& state, std::string_view text,
                                       size_t max_length, text_input_filter filter) {
    bool changed = delete_text_input_selection(state);
    size_t offset = 0;
    while (offset < text.size() && utf8_codepoint_count(state.value) < max_length) {
        int codepoint = 0;
        size_t next_offset = offset + 1;
        decode_next_utf8_codepoint(text, offset, codepoint, next_offset);
        offset = next_offset;
        if (filter != nullptr && !filter(codepoint, state.value)) {
            continue;
        }
        changed = insert_codepoint_at_cursor(state, codepoint) || changed;
    }
    return changed;
}

inline float text_input_prefix_width(const std::string& value, size_t codepoint_index, int font_size) {
    const size_t byte_index = utf8_codepoint_to_byte_index(value, codepoint_index);
    return measure_text_size(value.substr(0, byte_index), static_cast<float>(font_size)).x;
}

inline size_t text_input_cursor_from_mouse(const std::string& value, float local_x, int font_size) {
    const size_t codepoint_count = utf8_codepoint_count(value);
    if (local_x <= 0.0f || codepoint_count == 0) {
        return 0;
    }

    float previous_width = 0.0f;
    for (size_t codepoint_index = 1; codepoint_index <= codepoint_count; ++codepoint_index) {
        const float width = text_input_prefix_width(value, codepoint_index, font_size);
        if (local_x <= width) {
            return (local_x - previous_width) <= (width - local_x)
                       ? codepoint_index - 1
                       : codepoint_index;
        }
        previous_width = width;
    }

    return codepoint_count;
}

inline void update_text_input_scroll(text_input_state& state, float viewport_width, int font_size) {
    if (!state.active) {
        state.scroll_x = 0.0f;
        return;
    }

    const float cursor_x = text_input_prefix_width(state.value, state.cursor, font_size);
    const float padding = 8.0f;
    const float max_scroll = std::max(0.0f,
                                      measure_text_size(state.value, static_cast<float>(font_size)).x -
                                          viewport_width + padding);

    if (cursor_x - state.scroll_x < padding) {
        state.scroll_x = std::max(0.0f, cursor_x - padding);
    } else if (cursor_x - state.scroll_x > viewport_width - padding) {
        state.scroll_x = cursor_x - viewport_width + padding;
    }

    state.scroll_x = std::clamp(state.scroll_x, 0.0f, max_scroll);
}

inline void move_text_input_cursor(text_input_state& state, size_t next_cursor, bool selecting) {
    const size_t codepoint_count = utf8_codepoint_count(state.value);
    const size_t clamped_cursor = std::min(next_cursor, codepoint_count);
    if (selecting) {
        if (!state.has_selection) {
            state.has_selection = true;
            state.selection_anchor = state.cursor;
        }
        state.cursor = clamped_cursor;
        if (state.cursor == state.selection_anchor) {
            clear_text_input_selection(state);
        }
        return;
    }

    state.cursor = clamped_cursor;
    clear_text_input_selection(state);
}

inline bool text_input_key_action(int key) {
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

inline text_input_result draw_text_input(Rectangle rect, text_input_state& state,
                                         const char* label, const char* placeholder,
                                         const char* default_value = nullptr,
                                         draw_layer layer = draw_layer::base,
                                         int font_size = 16, size_t max_length = 32,
                                         text_input_filter filter = default_text_input_filter,
                                         float label_width = 84.0f) {
    text_input_result result;
    clamp_text_input_state(state);

    const auto apply_default_if_empty = [&]() {
        if (default_value != nullptr && state.value.empty()) {
            state.value = default_value;
            state.cursor = utf8_codepoint_count(state.value);
            clear_text_input_selection(state);
            state.scroll_x = 0.0f;
            result.changed = true;
            result.defaulted = true;
        }
    };

    const bool hovered = is_hovered(rect, layer);
    const bool pressed = is_pressed(rect, layer);
    const bool clicked = is_clicked(rect, layer);
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    detail::draw_row_visual(rect, hovered, pressed,
                            state.active ? g_theme->row_selected : g_theme->row,
                            state.active ? g_theme->row_selected_hover : g_theme->row_hover,
                            state.active ? g_theme->border_active : g_theme->border,
                            1.5f);

    const Rectangle content_rect = inset(visual, edge_insets::symmetric(0.0f, 12.0f));
    const Rectangle label_rect = {content_rect.x, content_rect.y, label_width, content_rect.height};
    const Rectangle input_rect = {
        content_rect.x + label_width,
        content_rect.y + 4.0f,
        content_rect.width - label_width,
        content_rect.height - 8.0f
    };
    const Rectangle text_rect = {
        input_rect.x + 10.0f,
        input_rect.y,
        std::max(0.0f, input_rect.width - 20.0f),
        input_rect.height
    };

    if (clicked) {
        result.clicked = true;
        if (!state.active) {
            result.activated = true;
        }
        state.active = true;

        if (CheckCollisionPointRec(GetMousePosition(), input_rect)) {
            const float local_x = GetMousePosition().x - text_rect.x + state.scroll_x;
            state.cursor = text_input_cursor_from_mouse(state.value, local_x, font_size);
            clear_text_input_selection(state);
            state.mouse_selecting = true;
        } else {
            state.cursor = utf8_codepoint_count(state.value);
            clear_text_input_selection(state);
        }
    } else if (state.active && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !hovered) {
        apply_default_if_empty();
        state.active = false;
        state.mouse_selecting = false;
        clear_text_input_selection(state);
        result.deactivated = true;
    }

    if (state.active && state.mouse_selecting && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = GetMousePosition();
        const float local_x = mouse.x - text_rect.x + state.scroll_x;
        const size_t mouse_cursor = text_input_cursor_from_mouse(state.value, local_x, font_size);
        state.cursor = mouse_cursor;
        state.has_selection = state.cursor != state.selection_anchor;
    }

    if (state.mouse_selecting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        state.mouse_selecting = false;
    }

    if (state.active) {
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        if (ctrl && IsKeyPressed(KEY_A)) {
            state.selection_anchor = 0;
            state.cursor = utf8_codepoint_count(state.value);
            state.has_selection = state.cursor > 0;
        }

        if (ctrl && IsKeyPressed(KEY_C) && state.has_selection) {
            SetClipboardText(selected_text_input_text(state).c_str());
        }

        if (ctrl && IsKeyPressed(KEY_X) && state.has_selection) {
            SetClipboardText(selected_text_input_text(state).c_str());
            result.changed = delete_text_input_selection(state) || result.changed;
        }

        if (ctrl && IsKeyPressed(KEY_V)) {
            const char* clipboard = GetClipboardText();
            if (clipboard != nullptr) {
                result.changed = paste_text_input_at_cursor(state, clipboard, max_length, filter) || result.changed;
            }
        }

        int codepoint = GetCharPressed();
        while (codepoint > 0) {
            if (state.has_selection) {
                result.changed = delete_text_input_selection(state) || result.changed;
            }
            if (utf8_codepoint_count(state.value) < max_length &&
                (filter == nullptr || filter(codepoint, state.value))) {
                result.changed = insert_codepoint_at_cursor(state, codepoint) || result.changed;
            }
            codepoint = GetCharPressed();
        }

        if (text_input_key_action(KEY_BACKSPACE)) {
            if (state.has_selection) {
                result.changed = delete_text_input_selection(state) || result.changed;
            } else if (state.cursor > 0) {
                const size_t end_byte = utf8_codepoint_to_byte_index(state.value, state.cursor);
                const size_t start_byte = utf8_codepoint_to_byte_index(state.value, state.cursor - 1);
                state.value.erase(start_byte, end_byte - start_byte);
                --state.cursor;
                clear_text_input_selection(state);
                result.changed = true;
            }
        }

        if (text_input_key_action(KEY_DELETE)) {
            if (state.has_selection) {
                result.changed = delete_text_input_selection(state) || result.changed;
            } else if (state.cursor < utf8_codepoint_count(state.value)) {
                const size_t start_byte = utf8_codepoint_to_byte_index(state.value, state.cursor);
                const size_t end_byte = utf8_codepoint_to_byte_index(state.value, state.cursor + 1);
                state.value.erase(start_byte, end_byte - start_byte);
                result.changed = true;
            }
        }

        if (text_input_key_action(KEY_LEFT)) {
            if (state.has_selection && !shift) {
                move_text_input_cursor(state, text_input_selection_range(state).first, false);
            } else if (state.cursor > 0) {
                move_text_input_cursor(state, state.cursor - 1, shift);
            }
        }

        if (text_input_key_action(KEY_RIGHT)) {
            if (state.has_selection && !shift) {
                move_text_input_cursor(state, text_input_selection_range(state).second, false);
            } else if (state.cursor < utf8_codepoint_count(state.value)) {
                move_text_input_cursor(state, state.cursor + 1, shift);
            }
        }

        if (text_input_key_action(KEY_HOME)) {
            move_text_input_cursor(state, 0, shift);
        }

        if (text_input_key_action(KEY_END)) {
            move_text_input_cursor(state, utf8_codepoint_count(state.value), shift);
        }

        if (IsKeyPressed(KEY_ENTER)) {
            apply_default_if_empty();
            result.submitted = true;
            state.active = false;
            state.mouse_selecting = false;
            clear_text_input_selection(state);
            result.deactivated = true;
        }
    }

    update_text_input_scroll(state, text_rect.width, font_size);

    draw_rect_f(input_rect, state.active ? with_alpha(g_theme->panel, 255) : with_alpha(g_theme->section, 255));
    draw_rect_lines(input_rect, 1.5f, state.active ? g_theme->border_active : g_theme->border_light);
    draw_text_in_rect(label, font_size, label_rect,
                      state.active ? g_theme->text : g_theme->text_secondary, text_align::left);

    std::string display_value = state.value;
    if (display_value.empty() && !state.active && placeholder != nullptr) {
        display_value = placeholder;
    }

    const Color text_color = state.value.empty() && !state.active ? g_theme->text_hint : g_theme->text;
    const float layout_font_size = text_layout_font_size(static_cast<float>(font_size));
    const float text_y = text_rect.y + (text_rect.height - layout_font_size) * 0.5f + kTextInputBaselineNudge;

    if (!state.active && !state.value.empty()) {
        draw_marquee_text(display_value.c_str(), text_rect.x, text_y, font_size, text_color,
                          text_rect.width, GetTime());
    } else if (!state.active) {
        draw_text_in_rect(display_value.c_str(), font_size,
                          {text_rect.x, text_rect.y + kTextInputBaselineNudge, text_rect.width, text_rect.height},
                          text_color, text_align::left);
    } else {
        begin_scissor_rect(input_rect);

        if (state.has_selection) {
            const auto [selection_start, selection_end] = text_input_selection_range(state);
            const float selection_x = text_rect.x +
                                      text_input_prefix_width(state.value, selection_start, font_size) -
                                      state.scroll_x;
            const float selection_end_x = text_rect.x +
                                          text_input_prefix_width(state.value, selection_end, font_size) -
                                          state.scroll_x;
            draw_rect_span({selection_x, input_rect.y + kTextInputSelectionInsetY,
                            selection_end_x - selection_x, input_rect.height - kTextInputSelectionInsetTotalY},
                           with_alpha(g_theme->row_selected, 255));
        }

        draw_text_f(state.value.c_str(), text_rect.x - state.scroll_x, text_y, font_size, g_theme->text);

        const double blink = GetTime() * 1.6;
        if (std::fmod(blink, 1.0) < 0.6) {
            const float cursor_x = text_rect.x +
                                   text_input_prefix_width(state.value, state.cursor, font_size) -
                                   state.scroll_x;
            draw_rect_span({cursor_x, input_rect.y + kTextInputSelectionInsetY, kTextInputCursorWidth,
                            input_rect.height - kTextInputSelectionInsetTotalY},
                           g_theme->text);
        }

        EndScissorMode();
    }

    return result;
}

}  // namespace ui
