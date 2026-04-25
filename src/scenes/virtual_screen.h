#pragma once

#include "raylib.h"

// 仮想解像度（1920x1080）で描画し、実際のウィンドウサイズにスケーリングする。
// メニューシーンなど2D UIはすべてこの仮想座標系で描画する。
namespace virtual_screen {

constexpr int kDesignWidth = 1920;
constexpr int kDesignHeight = 1080;
constexpr int kUiRenderScale = 2;

// InitWindow() の後に呼ぶ。RenderTexture を確保する。
void init();

// CloseWindow() の前に呼ぶ。RenderTexture を解放する。
void cleanup();

// 仮想スクリーンへの描画を開始する。以降の Draw 呼び出しは RenderTexture に書き込まれる。
void begin();

// UI 品質優先の高解像度仮想スクリーンへの描画を開始する。
// 論理座標は 1920x1080 のまま維持しつつ、内部では高解像度 RT に描画する。
void begin_ui();

// 仮想スクリーンへの描画を終了する。
void end();

// 仮想スクリーンの内容を実際のウィンドウにスケーリング描画する。
// use_alpha が true の場合、透過ブレンドで描画する（3D の上に HUD を重ねる用途）。
void draw_to_screen(bool use_alpha = false);

// 物理スクリーンのマウス座標を仮想座標に変換する。
Vector2 get_virtual_mouse();

// 1920x1080 の論理座標が、実画面へどれだけ拡大されているかを返す。
float design_to_screen_scale();

// 現在の仮想スクリーン描画倍率を返す。通常描画は 1、高品質 UI 描画は 2。
float current_render_scale();

// 現在の描画先 RenderTexture の実ピクセルサイズを返す。
int current_render_width();
int current_render_height();

}  // namespace virtual_screen
