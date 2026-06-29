#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <string>
#include <string_view>

#include "platform/windows_input_source.h"
#include "raylib.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

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

struct text_input_options {
    const char* default_value = nullptr;
    draw_layer layer = draw_layer::base;
    int font_size = 16;
    size_t max_length = 32;
    text_input_filter filter = default_text_input_filter;
    float label_width = 84.0f;
    bool obscure_value = false;
    bool single_rect = false;
    bool plain_when_inactive = false;
    bool submit_deactivates = true;
    text_align single_rect_align = text_align::center;
};

struct search_text_input_options {
    draw_layer layer = draw_layer::base;
    int font_size = 13;
    size_t max_length = 80;
    text_input_filter filter = default_text_input_filter;
    float content_padding_x = 14.0f;
    float label_width = 108.0f;
    float label_gap = 14.0f;
    bool show_search_icon = true;
    float search_icon_width = 28.0f;
    float search_icon_gap = 8.0f;
    const char* search_icon_label = "Q";
    Color button_base = {};
    Color button_selected = {};
    unsigned char normal_row_alpha = 255;
    unsigned char hover_row_alpha = 255;
    unsigned char selected_row_alpha = 255;
    unsigned char alpha = 255;
    text_role role = text_role::ui_body;
    bool submit_deactivates = true;
};

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

inline void update_text_input_scroll(text_input_state& state, float viewport_width, int font_size,
                                     const std::string* visual_value = nullptr,
                                     size_t visual_cursor = static_cast<size_t>(-1)) {
    if (!state.active) {
        state.scroll_x = 0.0f;
        return;
    }

    const std::string& measured_value = visual_value != nullptr ? *visual_value : state.value;
    const size_t cursor = visual_cursor == static_cast<size_t>(-1) ? state.cursor : visual_cursor;
    const float cursor_x = text_input_prefix_width(measured_value, cursor, font_size);
    const float padding = 8.0f;
    const float max_scroll = std::max(0.0f,
                                      measure_text_size(measured_value, static_cast<float>(font_size)).x -
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

inline void draw_text_role_f(text_role role, const char* text, float x, float y, int font_size, Color color) {
    draw_text(role, text != nullptr ? text : "", {x, y}, static_cast<float>(font_size), 0.0f, color);
}

inline void draw_text_role_in_rect(text_role role, const char* text, int font_size, Rectangle rect,
                                   Color color, text_align align = text_align::center) {
    const char* resolved_text = text != nullptr ? text : "";
    const Vector2 pos = text_position(role, resolved_text, font_size, rect, align);
    draw_text_role_f(role, resolved_text, pos.x, pos.y, font_size, color);
}

inline void draw_marquee_text_role(const char* text,
                                   Rectangle clip_rect,
                                   int font_size,
                                   Color color,
                                   double time,
                                   text_role role = text_role::ui_body,
                                   text_align align = text_align::left) {
    if (text == nullptr || *text == '\0' || clip_rect.width <= 0.0f || clip_rect.height <= 0.0f) {
        return;
    }

    const float text_width = measure_text_size(role, text, static_cast<float>(font_size), 0.0f).x;
    const float draw_y = clip_rect.y + (clip_rect.height - text_layout_font_size(static_cast<float>(font_size))) * 0.5f;
    if (text_width <= clip_rect.width) {
        float draw_x = clip_rect.x;
        if (align == text_align::center) {
            draw_x = clip_rect.x + (clip_rect.width - text_width) * 0.5f;
        } else if (align == text_align::right) {
            draw_x = clip_rect.x + clip_rect.width - text_width;
        }
        scoped_clip_rect clip(clip_rect);
        draw_text_role_f(role, text, draw_x, draw_y, font_size, color);
        return;
    }

    constexpr float kScrollSpeed = 42.0f;
    constexpr float kPauseSeconds = 1.0f;
    const float overflow = text_width - clip_rect.width;
    const float travel_time = overflow / kScrollSpeed;
    const float cycle = travel_time + kPauseSeconds * 2.0f;
    const float cycle_t = static_cast<float>(std::fmod(time, static_cast<double>(std::max(cycle, 0.001f))));
    float offset = 0.0f;
    if (cycle_t > kPauseSeconds && cycle_t < kPauseSeconds + travel_time) {
        offset = (cycle_t - kPauseSeconds) * kScrollSpeed;
    } else if (cycle_t >= kPauseSeconds + travel_time) {
        offset = overflow;
    }
    scoped_clip_rect clip(clip_rect);
    draw_text_role_f(role, text, clip_rect.x - offset, draw_y, font_size, color);
}

inline text_input_result text_input_core(Rectangle rect, text_input_state& state,
                                         const char* label, const char* placeholder,
                                         const char* default_value,
                                         draw_layer layer,
                                         int font_size, size_t max_length,
                                         text_input_filter filter,
                                         float label_width,
                                         bool obscure_value,
                                         bool single_rect,
                                         bool plain_when_inactive,
                                         bool submit_deactivates,
                                         text_align single_rect_align) {
    text_input_result result;
    clamp_text_input_state(state);
    const auto visual_value_for_state = [&]() {
        if (!obscure_value || state.value.empty()) {
            return state.value;
        }
        return std::string(utf8_codepoint_count(state.value), '*');
    };
    const auto insert_text_at_cursor_for_display = [](const std::string& value,
                                                      size_t cursor,
                                                      const std::string& text) {
        if (text.empty()) {
            return value;
        }
        const size_t byte_index = utf8_codepoint_to_byte_index(value, cursor);
        return value.substr(0, byte_index) + text + value.substr(byte_index);
    };

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
    if (!single_rect) {
        detail::draw_row_visual(rect, hovered, pressed,
                                state.active ? g_theme->row_selected : g_theme->row,
                                state.active ? g_theme->row_selected_hover : g_theme->row_hover,
                                state.active ? g_theme->border_active : g_theme->border,
                                1.5f);
    }

    const Rectangle content_rect = inset(visual, edge_insets::symmetric(0.0f, 12.0f));
    const Rectangle label_rect = {content_rect.x, content_rect.y, label_width, content_rect.height};
    const Rectangle input_rect = single_rect
        ? visual
        : Rectangle{
            content_rect.x + label_width,
            content_rect.y + 4.0f,
            content_rect.width - label_width,
            content_rect.height - 8.0f
        };
    const Rectangle text_rect = {
        input_rect.x + (single_rect ? 8.0f : 10.0f),
        input_rect.y,
        std::max(0.0f, input_rect.width - (single_rect ? 16.0f : 20.0f)),
        input_rect.height
    };
    const auto active_text_offset = [&](const std::string& value) {
        if (!single_rect || single_rect_align != text_align::center) {
            return 0.0f;
        }
        const float text_width = text_input_prefix_width(value, utf8_codepoint_count(value), font_size);
        return std::max(0.0f, (text_rect.width - text_width) * 0.5f);
    };
    std::string composition_text;

    if (clicked) {
        result.clicked = true;
        if (!state.active) {
            result.activated = true;
        }
        state.active = true;

        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        if (contains_point(input_rect, mouse)) {
            const std::string visual_value = visual_value_for_state();
            const float local_x = mouse.x - text_rect.x - active_text_offset(visual_value) +
                                  state.scroll_x;
            state.cursor = text_input_cursor_from_mouse(visual_value, local_x, font_size);
            clear_text_input_selection(state);
            state.mouse_selecting = true;
        } else {
            state.cursor = utf8_codepoint_count(state.value);
            clear_text_input_selection(state);
        }
    } else if (state.active && is_mouse_button_pressed() && !hovered) {
        apply_default_if_empty();
        state.active = false;
        state.mouse_selecting = false;
        clear_text_input_selection(state);
        result.deactivated = true;
    }

    if (state.active && state.mouse_selecting && is_mouse_button_down()) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        const std::string visual_value = visual_value_for_state();
        const float local_x = mouse.x - text_rect.x - active_text_offset(visual_value) + state.scroll_x;
        const size_t mouse_cursor = text_input_cursor_from_mouse(visual_value, local_x, font_size);
        state.cursor = mouse_cursor;
        state.has_selection = state.cursor != state.selection_anchor;
    }

    if (state.mouse_selecting && is_mouse_button_released()) {
        state.mouse_selecting = false;
    }

    if (state.active && !IsWindowFocused()) {
        windows_input_source::instance().cancel_text_input();
        state.active = false;
        state.mouse_selecting = false;
        clear_text_input_selection(state);
        result.deactivated = true;
    }

    if (state.active) {
        windows_input_source::instance().request_text_input();
        const native_text_input_update text_update = windows_input_source::instance().drain_text_input();
        composition_text = obscure_value ? "" : text_update.composition_text;
        if (!text_update.committed_text.empty()) {
            if (state.has_selection) {
                result.changed = delete_text_input_selection(state) || result.changed;
            }
            result.changed = paste_text_input_at_cursor(state, text_update.committed_text, max_length, filter) ||
                             result.changed;
        }

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
            state.mouse_selecting = false;
            clear_text_input_selection(state);
            if (submit_deactivates) {
                state.active = false;
                result.deactivated = true;
            }
        }
    }

    if (!plain_when_inactive || state.active) {
        draw_rect_f(input_rect, state.active ? with_alpha(g_theme->panel, 255) : with_alpha(g_theme->section, 255));
        draw_rect_lines(input_rect, 1.5f, state.active ? g_theme->border_active : g_theme->border_light);
    }
    if (!single_rect) {
        draw_text_in_rect(label, font_size, label_rect,
                          state.active ? g_theme->text : g_theme->text_secondary, text_align::left);
    }

    std::string display_value = visual_value_for_state();
    const size_t composition_codepoints = utf8_codepoint_count(composition_text);
    const size_t display_cursor = state.active ? state.cursor + composition_codepoints : state.cursor;
    if (state.active && !composition_text.empty()) {
        display_value = insert_text_at_cursor_for_display(display_value, state.cursor, composition_text);
    }
    if (display_value.empty() && !state.active && placeholder != nullptr) {
        display_value = placeholder;
    }
    const std::string scroll_visual_value = visual_value_for_state();
    const std::string scroll_display_value = state.active && !composition_text.empty() && !obscure_value
        ? display_value
        : scroll_visual_value;
    update_text_input_scroll(state, text_rect.width, font_size,
                             state.active && (!composition_text.empty() || obscure_value) ? &scroll_display_value : nullptr,
                             display_cursor);

    const Color text_color = state.value.empty() && !state.active ? g_theme->text_hint : g_theme->text;
    const float layout_font_size = text_layout_font_size(static_cast<float>(font_size));
    const float text_y = text_rect.y + (text_rect.height - layout_font_size) * 0.5f + kTextInputBaselineNudge;
    const float active_offset_x = active_text_offset(display_value);

    if (single_rect && !state.active) {
        draw_text_in_rect(display_value.c_str(), font_size,
                          {text_rect.x, text_rect.y + kTextInputBaselineNudge, text_rect.width, text_rect.height},
                          text_color, single_rect_align);
    } else if (!state.active && !state.value.empty()) {
        draw_marquee_text(display_value.c_str(), text_rect.x, text_y, font_size, text_color,
                          text_rect.width, GetTime());
    } else if (!state.active) {
        draw_text_in_rect(display_value.c_str(), font_size,
                          {text_rect.x, text_rect.y + kTextInputBaselineNudge, text_rect.width, text_rect.height},
                          text_color, text_align::left);
    } else {
        scoped_clip_rect clip(input_rect);

        if (state.has_selection) {
            const auto [selection_start, selection_end] = text_input_selection_range(state);
            const float selection_x = text_rect.x + active_offset_x +
                                      text_input_prefix_width(display_value, selection_start, font_size) -
                                      state.scroll_x;
            const float selection_end_x = text_rect.x + active_offset_x +
                                          text_input_prefix_width(display_value, selection_end, font_size) -
                                          state.scroll_x;
            draw_rect_span({selection_x, input_rect.y + kTextInputSelectionInsetY,
                            selection_end_x - selection_x, input_rect.height - kTextInputSelectionInsetTotalY},
                           with_alpha(g_theme->row_selected, 255));
        }

        draw_text_f(display_value.c_str(), text_rect.x + active_offset_x - state.scroll_x, text_y,
                    font_size, g_theme->text);

        if (!composition_text.empty()) {
            const float composition_x = text_rect.x + active_offset_x +
                                        text_input_prefix_width(display_value, state.cursor, font_size) -
                                        state.scroll_x;
            const float composition_end_x = text_rect.x + active_offset_x +
                                            text_input_prefix_width(display_value, display_cursor, font_size) -
                                            state.scroll_x;
            draw_line_ex({composition_x, text_y + layout_font_size + 2.0f},
                         {composition_end_x, text_y + layout_font_size + 2.0f},
                         1.5f, g_theme->border_active);
        }

        const float cursor_x = text_rect.x + active_offset_x +
                               text_input_prefix_width(display_value, display_cursor, font_size) -
                               state.scroll_x;
        const Vector2 screen_pos = virtual_screen::virtual_to_screen({cursor_x, text_y + layout_font_size + 6.0f});
        const Vector2 input_top = virtual_screen::virtual_to_screen({input_rect.x, input_rect.y});
        const Vector2 input_bottom = virtual_screen::virtual_to_screen({input_rect.x, input_rect.y + input_rect.height});
        windows_input_source::instance().set_text_input_screen_position(static_cast<int>(std::round(screen_pos.x)),
                                                                        static_cast<int>(std::round(screen_pos.y)),
                                                                        static_cast<int>(std::round(input_top.y)),
                                                                        static_cast<int>(std::round(input_bottom.y)));

        const double blink = GetTime() * 1.6;
        if (std::fmod(blink, 1.0) < 0.6) {
            draw_rect_span({cursor_x, input_rect.y + kTextInputSelectionInsetY, kTextInputCursorWidth,
                            input_rect.height - kTextInputSelectionInsetTotalY},
                           g_theme->text);
        }
    }

    return result;
}

inline text_input_result text_input(Rectangle rect, text_input_state& state,
                                    const char* label, const char* placeholder,
                                    text_input_options options = {}) {
    return text_input_core(rect, state, label, placeholder,
                           options.default_value,
                           options.layer,
                           options.font_size,
                           options.max_length,
                           options.filter,
                           options.label_width,
                           options.obscure_value,
                           options.single_rect,
                           options.plain_when_inactive,
                           options.submit_deactivates,
                           options.single_rect_align);
}

inline text_input_result search_text_input(Rectangle rect,
                                           text_input_state& state,
                                           const char* label,
                                           const char* placeholder,
                                           search_text_input_options options = {}) {
    text_input_result result;
    clamp_text_input_state(state);
    const auto insert_text_at_cursor_for_display = [](const std::string& value,
                                                      size_t cursor,
                                                      const std::string& text) {
        if (text.empty()) {
            return value;
        }
        const size_t byte_index = utf8_codepoint_to_byte_index(value, cursor);
        return value.substr(0, byte_index) + text + value.substr(byte_index);
    };

    const bool hovered = is_hovered(rect, options.layer);
    const bool pressed = is_pressed(rect, options.layer);
    const bool clicked = is_clicked(rect, options.layer);
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    const unsigned char row_alpha = state.active ? options.selected_row_alpha
        : hovered ? options.hover_row_alpha
                  : options.normal_row_alpha;
    const Color base = options.button_base.a > 0 ? options.button_base : g_theme->row;
    const Color selected = options.button_selected.a > 0 ? options.button_selected : g_theme->row_selected;
    surface_fill(visual, with_alpha(state.active ? selected : base, row_alpha));
    frame(inset(visual, 1.0f),
          with_alpha(state.active ? g_theme->border_active : g_theme->border_light, options.alpha),
          1.2f);

    const Rectangle content_rect = inset(visual, edge_insets::symmetric(0.0f, options.content_padding_x));
    const bool show_label = label != nullptr && *label != '\0' && !state.active && state.value.empty();
    const bool show_search_icon = options.show_search_icon;
    const float leading_width = show_search_icon
        ? options.search_icon_width + options.search_icon_gap
        : 0.0f;
    const Rectangle icon_rect = {
        content_rect.x,
        content_rect.y,
        options.search_icon_width,
        content_rect.height,
    };
    const Rectangle label_rect = {content_rect.x, content_rect.y, options.label_width, content_rect.height};
    const float text_x = (show_label ? content_rect.x + options.label_width + options.label_gap : content_rect.x) +
                         leading_width;
    const float text_width = std::max(
        0.0f,
        (show_label ? content_rect.width - options.label_width - options.label_gap : content_rect.width) -
            leading_width);
    const Rectangle text_rect = {text_x, content_rect.y, text_width, content_rect.height};

    if (clicked) {
        result.clicked = true;
        if (!state.active) {
            result.activated = true;
        }
        state.active = true;

        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        if (contains_point(text_rect, mouse)) {
            const float local_x = mouse.x - text_rect.x + state.scroll_x;
            state.cursor = text_input_cursor_from_mouse(state.value, local_x, options.font_size);
            clear_text_input_selection(state);
            state.mouse_selecting = true;
        } else {
            state.cursor = utf8_codepoint_count(state.value);
            clear_text_input_selection(state);
        }
    } else if (state.active && is_mouse_button_pressed() && !hovered) {
        state.active = false;
        state.mouse_selecting = false;
        clear_text_input_selection(state);
        result.deactivated = true;
    }

    if (state.active && state.mouse_selecting && is_mouse_button_down()) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        const float local_x = mouse.x - text_rect.x + state.scroll_x;
        const size_t mouse_cursor = text_input_cursor_from_mouse(state.value, local_x, options.font_size);
        state.cursor = mouse_cursor;
        state.has_selection = state.cursor != state.selection_anchor;
    }

    if (state.mouse_selecting && is_mouse_button_released()) {
        state.mouse_selecting = false;
    }

    std::string composition_text;
    if (state.active) {
        windows_input_source::instance().request_text_input();
        const native_text_input_update text_update = windows_input_source::instance().drain_text_input();
        composition_text = text_update.composition_text;
        if (!text_update.committed_text.empty()) {
            if (state.has_selection) {
                result.changed = delete_text_input_selection(state) || result.changed;
            }
            result.changed = paste_text_input_at_cursor(state, text_update.committed_text, options.max_length,
                                                        options.filter) ||
                             result.changed;
        }

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
                result.changed = paste_text_input_at_cursor(state, clipboard, options.max_length, options.filter) ||
                                 result.changed;
            }
        }

        int codepoint = GetCharPressed();
        while (codepoint > 0) {
            if (state.has_selection) {
                result.changed = delete_text_input_selection(state) || result.changed;
            }
            if (utf8_codepoint_count(state.value) < options.max_length &&
                (options.filter == nullptr || options.filter(codepoint, state.value))) {
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
            result.submitted = true;
            state.mouse_selecting = false;
            clear_text_input_selection(state);
            if (options.submit_deactivates) {
                state.active = false;
                result.deactivated = true;
            }
        }
    }

    std::string display_value = state.value;
    const size_t composition_codepoints = utf8_codepoint_count(composition_text);
    const size_t display_cursor = state.active ? state.cursor + composition_codepoints : state.cursor;
    if (state.active && !composition_text.empty()) {
        display_value = insert_text_at_cursor_for_display(display_value, state.cursor, composition_text);
    }
    const std::string scroll_display_value = display_value;
    update_text_input_scroll(state,
                             text_rect.width - 8.0f,
                             options.font_size,
                             state.active && !composition_text.empty() ? &scroll_display_value : nullptr,
                             display_cursor);

    if (show_search_icon) {
        draw_text_role_in_rect(options.role,
                               options.search_icon_label,
                               18,
                               icon_rect,
                               with_alpha(g_theme->text_secondary, options.alpha));
    }

    if (show_label) {
        draw_text_role_in_rect(options.role,
                               label,
                               options.font_size,
                               label_rect,
                               with_alpha(g_theme->text_secondary, options.alpha),
                               text_align::left);
    }

    if (display_value.empty() && !state.active && placeholder != nullptr) {
        display_value = placeholder;
    }

    const Color text_color = with_alpha(state.value.empty() && !state.active ? g_theme->text_hint : g_theme->text,
                                        options.alpha);
    const float layout_font_size = text_layout_font_size(static_cast<float>(options.font_size));
    const float text_y = text_rect.y + (text_rect.height - layout_font_size) * 0.5f + 2.0f;

    if (!state.active && !state.value.empty()) {
        draw_marquee_text_role(display_value.c_str(), text_rect, options.font_size, text_color, GetTime(), options.role);
    } else if (!state.active) {
        draw_text_role_f(options.role, display_value.c_str(), text_rect.x, text_y, options.font_size, text_color);
    } else {
        scoped_clip_rect clip(text_rect);

        if (state.has_selection) {
            const auto [selection_start, selection_end] = text_input_selection_range(state);
            const float selection_x = text_rect.x +
                                      text_input_prefix_width(display_value, selection_start, options.font_size) -
                                      state.scroll_x;
            const float selection_end_x = text_rect.x +
                                          text_input_prefix_width(display_value, selection_end, options.font_size) -
                                          state.scroll_x;
            draw_rect_span({selection_x, text_rect.y + 7.0f, selection_end_x - selection_x, text_rect.height - 14.0f},
                           with_alpha(g_theme->row_selected, options.alpha));
        }

        draw_text_role_f(options.role,
                         display_value.c_str(),
                         text_rect.x - state.scroll_x,
                         text_y,
                         options.font_size,
                         with_alpha(g_theme->text, options.alpha));

        if (!composition_text.empty()) {
            const float composition_x = text_rect.x +
                                        text_input_prefix_width(display_value, state.cursor, options.font_size) -
                                        state.scroll_x;
            const float composition_end_x =
                text_rect.x + text_input_prefix_width(display_value, display_cursor, options.font_size) -
                state.scroll_x;
            draw_line_ex({composition_x, text_y + layout_font_size + 2.0f},
                         {composition_end_x, text_y + layout_font_size + 2.0f},
                         1.5f, with_alpha(g_theme->border_active, options.alpha));
        }

        const float cursor_x = text_rect.x +
                               text_input_prefix_width(display_value, display_cursor, options.font_size) -
                               state.scroll_x;
        const Vector2 screen_pos = virtual_screen::virtual_to_screen({cursor_x, text_y + layout_font_size + 6.0f});
        const Vector2 input_top = virtual_screen::virtual_to_screen({text_rect.x, text_rect.y});
        const Vector2 input_bottom = virtual_screen::virtual_to_screen({text_rect.x, text_rect.y + text_rect.height});
        windows_input_source::instance().set_text_input_screen_position(static_cast<int>(std::round(screen_pos.x)),
                                                                        static_cast<int>(std::round(screen_pos.y)),
                                                                        static_cast<int>(std::round(input_top.y)),
                                                                        static_cast<int>(std::round(input_bottom.y)));

        const double blink = GetTime() * 1.6;
        if (std::fmod(blink, 1.0) < 0.6) {
            draw_rect_span({cursor_x, text_rect.y + 8.0f, 1.5f, text_rect.height - 16.0f},
                           with_alpha(g_theme->text, options.alpha));
        }
    }

    return result;
}

}  // namespace ui
