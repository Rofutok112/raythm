#pragma once

#include "raylib.h"

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;

// メニュー系シーン共通のフレーム（グラデーション背景・角丸枠・タイトル文字列）を描画する。
void draw_scene_frame(const char* title, const char* subtitle, Color accent);
