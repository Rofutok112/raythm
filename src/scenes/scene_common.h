#pragma once

#include "raylib.h"

struct ui_theme;

// 仮想解像度。すべての2D UIはこの座標系で描画される。
// 実際の表示は virtual_screen を通じてウィンドウサイズにスケーリングされる。
constexpr int kScreenWidth = 1920;
constexpr int kScreenHeight = 1080;

// メニュー系シーン共通のフレーム（グラデーション背景・角丸枠・タイトル文字列）を描画する。
void draw_scene_frame(const char* title, const char* subtitle, Color accent);

// テーマ色に基づく全画面背景を描画する。
void draw_scene_background(const ui_theme& theme);

// clip_rect 内でピクセル単位にクリップしながらマーキー表示する。
void draw_marquee_text(const char* text, Rectangle clip_rect, int font_size, Color color, double time);

// テキストが max_width に収まらない場合、自動的に左右にスクロールするマーキー表示を行う。
// 収まる場合はそのまま描画する。time にはアニメーションの基準時刻（GetTime() 等）を渡す。
// Rectangle ベース API への後方互換ラッパー。
void draw_marquee_text(const char* text, float x, float y, int font_size, Color color, float max_width, double time);
