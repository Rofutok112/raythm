#pragma once

#include "raylib.h"

// 仮想解像度。すべての2D UIはこの座標系で描画される。
// 実際の表示は virtual_screen を通じてウィンドウサイズにスケーリングされる。
constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;

// メニュー系シーン共通のフレーム（グラデーション背景・角丸枠・タイトル文字列）を描画する。
void draw_scene_frame(const char* title, const char* subtitle, Color accent);

// テキストが max_width に収まらない場合、自動的に左右にスクロールするマーキー表示を行う。
// 収まる場合はそのまま描画する。time にはアニメーションの基準時刻（GetTime() 等）を渡す。
// 描画状態を関数外へ持ち出さないよう、内部ではシザー状態を変更しない。
void draw_marquee_text(const char* text, int x, int y, int font_size, Color color, float max_width, double time);
