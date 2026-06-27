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
