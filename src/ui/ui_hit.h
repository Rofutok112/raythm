#pragma once

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

inline std::vector<hit_region>& hit_regions() {
    static std::vector<hit_region> regions;
    return regions;
}

inline void begin_hit_regions() {
    hit_regions().clear();
}

inline void register_hit_region(Rectangle rect, draw_layer layer) {
    hit_regions().push_back({rect, static_cast<int>(layer)});
}

inline bool is_blocked_by_higher_layer(Rectangle rect, draw_layer layer) {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    if (!CheckCollisionPointRec(mouse, rect)) {
        return false;
    }

    const int requested_layer = static_cast<int>(layer);
    for (const hit_region& region : hit_regions()) {
        if (region.layer <= requested_layer) {
            continue;
        }
        if (CheckCollisionPointRec(mouse, region.rect)) {
            return true;
        }
    }
    return false;
}

// 仮想マウスが rect 内にあるか。
inline bool is_hovered(Rectangle rect, draw_layer layer = draw_layer::base) {
    return CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), rect) &&
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

}  // namespace ui
