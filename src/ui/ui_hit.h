#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "raylib.h"
#include "virtual_screen.h"

// ヒットテストユーティリティ。
// virtual_screen の仮想マウス座標を使用して判定する。
namespace ui {

enum class draw_layer : int {
    base = 0,
    overlay = 100,
    modal = 200,
    debug = 300,
};

struct hit_region {
    Rectangle rect = {};
    int layer = 0;
};

inline constexpr int kNoActiveDragIndex = -1;

struct indexed_drag_state {
    int active_index = kNoActiveDragIndex;
};

struct indexed_drag_result {
    int active_index = kNoActiveDragIndex;
    bool started = false;
    bool dragging = false;
    bool released = false;
};

inline std::vector<hit_region>& hit_regions() {
    static std::vector<hit_region> regions;
    return regions;
}

inline void begin_hit_regions() {
    hit_regions().clear();
}

inline bool contains_point(Rectangle rect, Vector2 point) {
    return CheckCollisionPointRec(point, rect);
}

inline bool contains_any_point(std::span<const Rectangle> rects, Vector2 point) {
    for (const Rectangle rect : rects) {
        if (contains_point(rect, point)) {
            return true;
        }
    }
    return false;
}

inline bool is_mouse_button_pressed_outside(std::span<const Rectangle> rects,
                                            Vector2 point,
                                            int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonPressed(mouse_button) && !contains_any_point(rects, point);
}

inline bool is_mouse_button_pressed_outside(Rectangle rect,
                                            Vector2 point,
                                            int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonPressed(mouse_button) && !contains_point(rect, point);
}

inline bool is_mouse_button_released_outside(std::span<const Rectangle> rects,
                                             Vector2 point,
                                             int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonReleased(mouse_button) && !contains_any_point(rects, point);
}

inline bool is_mouse_button_released_outside(Rectangle rect,
                                             Vector2 point,
                                             int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonReleased(mouse_button) && !contains_point(rect, point);
}

inline bool is_any_mouse_button_pressed(std::span<const int> mouse_buttons) {
    for (const int mouse_button : mouse_buttons) {
        if (IsMouseButtonPressed(mouse_button)) {
            return true;
        }
    }
    return false;
}

inline bool is_any_mouse_button_pressed_outside(std::span<const Rectangle> rects,
                                                Vector2 point,
                                                std::span<const int> mouse_buttons) {
    return is_any_mouse_button_pressed(mouse_buttons) && !contains_any_point(rects, point);
}

inline bool is_any_mouse_button_pressed_outside(Rectangle rect,
                                                Vector2 point,
                                                std::span<const int> mouse_buttons) {
    return is_any_mouse_button_pressed(mouse_buttons) && !contains_point(rect, point);
}

inline bool is_mouse_button_down(int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonDown(mouse_button);
}

inline bool is_mouse_button_pressed(int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonPressed(mouse_button);
}

inline bool is_mouse_button_released(int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonReleased(mouse_button);
}

inline float mouse_wheel_move() {
    return GetMouseWheelMove();
}

inline bool is_escape_pressed() {
    return IsKeyPressed(KEY_ESCAPE);
}

inline bool is_key_pressed(int key) {
    return IsKeyPressed(key);
}

inline bool is_key_down(int key) {
    return IsKeyDown(key);
}

inline bool is_any_key_pressed(std::span<const int> keys) {
    for (const int key : keys) {
        if (is_key_pressed(key)) {
            return true;
        }
    }
    return false;
}

inline bool is_enter_pressed() {
    return is_key_pressed(KEY_ENTER);
}

inline bool is_tab_pressed() {
    return is_key_pressed(KEY_TAB);
}

inline bool is_shift_down() {
    return is_key_down(KEY_LEFT_SHIFT) || is_key_down(KEY_RIGHT_SHIFT);
}

inline bool is_left_pressed() {
    constexpr int keys[] = {KEY_LEFT, KEY_A};
    return is_any_key_pressed(keys);
}

inline bool is_right_pressed() {
    constexpr int keys[] = {KEY_RIGHT, KEY_D};
    return is_any_key_pressed(keys);
}

inline bool is_up_pressed() {
    constexpr int keys[] = {KEY_UP, KEY_W};
    return is_any_key_pressed(keys);
}

inline bool is_down_pressed() {
    constexpr int keys[] = {KEY_DOWN, KEY_S};
    return is_any_key_pressed(keys);
}

inline bool is_cancel_pressed(int mouse_button = MOUSE_BUTTON_RIGHT) {
    return is_escape_pressed() || IsMouseButtonPressed(mouse_button);
}

inline bool is_mouse_button_down_or_released(int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonDown(mouse_button) || IsMouseButtonReleased(mouse_button);
}

inline void register_hit_region(Rectangle rect, draw_layer layer) {
    hit_regions().push_back({rect, static_cast<int>(layer)});
}

inline bool is_blocked_by_higher_layer(Rectangle rect, draw_layer layer) {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    if (!contains_point(rect, mouse)) {
        return false;
    }

    const int requested_layer = static_cast<int>(layer);
    for (const hit_region& region : hit_regions()) {
        if (region.layer <= requested_layer) {
            continue;
        }
        if (contains_point(region.rect, mouse)) {
            return true;
        }
    }
    return false;
}

// 仮想マウスが rect 内にあるか。
inline bool is_hovered(Rectangle rect, draw_layer layer = draw_layer::base) {
    return contains_point(rect, virtual_screen::get_virtual_mouse()) &&
           !is_blocked_by_higher_layer(rect, layer);
}

inline bool is_mouse_button_released_outside(Rectangle rect,
                                             draw_layer layer,
                                             int mouse_button = MOUSE_BUTTON_LEFT) {
    return IsMouseButtonReleased(mouse_button) && !is_hovered(rect, layer);
}

inline bool is_mouse_button_down(Rectangle rect, int mouse_button, draw_layer layer = draw_layer::base) {
    return is_hovered(rect, layer) && IsMouseButtonDown(mouse_button);
}

inline bool is_mouse_button_pressed(Rectangle rect, int mouse_button, draw_layer layer = draw_layer::base) {
    return is_hovered(rect, layer) && IsMouseButtonPressed(mouse_button);
}

inline bool is_mouse_button_released(Rectangle rect, int mouse_button, draw_layer layer = draw_layer::base) {
    return is_hovered(rect, layer) && IsMouseButtonReleased(mouse_button);
}

// ホバー中かつマウスボタン押下中か。
inline bool is_pressed(Rectangle rect, draw_layer layer = draw_layer::base) {
    return is_mouse_button_down(rect, MOUSE_BUTTON_LEFT, layer);
}

// ホバー中かつマウスボタンを離した瞬間か（クリック判定）。
inline bool is_clicked(Rectangle rect, draw_layer layer = draw_layer::base) {
    return is_mouse_button_released(rect, MOUSE_BUTTON_LEFT, layer);
}

inline void reset_indexed_drag(indexed_drag_state& state) {
    state.active_index = kNoActiveDragIndex;
}

inline indexed_drag_result update_indexed_drag(std::span<const Rectangle> hit_rects,
                                               indexed_drag_state& state,
                                               draw_layer layer = draw_layer::base,
                                               int mouse_button = MOUSE_BUTTON_LEFT) {
    bool released = false;
    if (IsMouseButtonReleased(mouse_button)) {
        released = state.active_index != kNoActiveDragIndex;
        state.active_index = kNoActiveDragIndex;
    }

    bool started = false;
    if (IsMouseButtonPressed(mouse_button)) {
        for (int i = 0; i < static_cast<int>(hit_rects.size()); ++i) {
            if (is_hovered(hit_rects[static_cast<std::size_t>(i)], layer)) {
                state.active_index = i;
                started = true;
                break;
            }
        }
    }

    return {
        state.active_index,
        started,
        state.active_index != kNoActiveDragIndex && IsMouseButtonDown(mouse_button),
        released,
    };
}

}  // namespace ui
