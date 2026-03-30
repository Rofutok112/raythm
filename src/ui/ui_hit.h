#pragma once

#include "raylib.h"
#include "virtual_screen.h"

// ヒットテストユーティリティ。
// virtual_screen の仮想マウス座標を使用して判定する。
namespace ui {

// 仮想マウスが rect 内にあるか。
inline bool is_hovered(Rectangle rect) {
    return CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), rect);
}

// ホバー中かつマウスボタン押下中か。
inline bool is_pressed(Rectangle rect) {
    return is_hovered(rect) && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
}

// ホバー中かつマウスボタンを離した瞬間か（クリック判定）。
inline bool is_clicked(Rectangle rect) {
    return is_hovered(rect) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

}  // namespace ui
