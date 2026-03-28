#pragma once

#include "raylib.h"

// 仮想解像度（1280x720）で描画し、実際のウィンドウサイズにスケーリングする。
// メニューシーンなど2D UIはすべてこの仮想座標系で描画する。
namespace virtual_screen {

constexpr int kDesignWidth = 1280;
constexpr int kDesignHeight = 720;

// InitWindow() の後に呼ぶ。RenderTexture を確保する。
void init();

// CloseWindow() の前に呼ぶ。RenderTexture を解放する。
void cleanup();

// 仮想スクリーンへの描画を開始する。以降の Draw 呼び出しは RenderTexture に書き込まれる。
void begin();

// 仮想スクリーンへの描画を終了する。
void end();

// 仮想スクリーンの内容を実際のウィンドウにスケーリング描画する。
// use_alpha が true の場合、透過ブレンドで描画する（3D の上に HUD を重ねる用途）。
void draw_to_screen(bool use_alpha = false);

// 物理スクリーンのマウス座標を仮想座標に変換する。
Vector2 get_virtual_mouse();

}  // namespace virtual_screen
