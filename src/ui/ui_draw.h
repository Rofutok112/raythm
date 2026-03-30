#pragma once

#include "raylib.h"
#include "theme.h"
#include "ui_hit.h"
#include "ui_layout.h"
#include "ui_text.h"

// 繰り返し使用される UI 描画パターンのユーティリティ。
// theme.h の g_theme を参照するため、テーマ設定後に使用すること。
namespace ui {

// ── ボタン ──────────────────────────────────────────────

// ボタンの描画状態。draw_button が返し、呼び出し側でクリック判定に使用する。
struct button_state {
    bool hovered;
    bool pressed;
    bool clicked;
};

// 標準ボタンを描画する。hover で色変化、press で 1.5px 押し込み、テキスト中央揃え。
// 戻り値の clicked でアクション判定できる。
inline button_state draw_button(Rectangle rect, const char* label, int font_size,
                                float border_width = 2.0f) {
    const bool hovered = is_hovered(rect);
    const bool pressed = is_pressed(rect);
    const bool clicked = is_clicked(rect);
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;

    DrawRectangleRec(visual, lerp_color(g_theme->row, g_theme->row_hover, hovered ? 1.0f : 0.0f));
    DrawRectangleLinesEx(visual, border_width, g_theme->border);
    draw_text_in_rect(label, font_size, visual, g_theme->text);

    return {hovered, pressed, clicked};
}

// カスタム色のボタン。選択状態やアクティブ状態の表現に使用する。
inline button_state draw_button_colored(Rectangle rect, const char* label, int font_size,
                                        Color bg, Color bg_hover, Color text_color,
                                        float border_width = 2.0f) {
    const bool hovered = is_hovered(rect);
    const bool pressed = is_pressed(rect);
    const bool clicked = is_clicked(rect);
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;

    DrawRectangleRec(visual, lerp_color(bg, bg_hover, hovered ? 1.0f : 0.0f));
    DrawRectangleLinesEx(visual, border_width, g_theme->border);
    draw_text_in_rect(label, font_size, visual, text_color);

    return {hovered, pressed, clicked};
}

// ── パネル ──────────────────────────────────────────────

// メインパネル（panel 背景 + border ボーダー、2px）。
inline void draw_panel(Rectangle rect) {
    DrawRectangleRec(rect, g_theme->panel);
    DrawRectangleLinesEx(rect, 2.0f, g_theme->border);
}

// セクションパネル（section 背景 + border_light ボーダー、1.5px）。
inline void draw_section(Rectangle rect) {
    DrawRectangleRec(rect, g_theme->section);
    DrawRectangleLinesEx(rect, 1.5f, g_theme->border_light);
}

// ── ラベル付き値 ────────────────────────────────────────

// 左にラベル、右に値を表示する行。設定画面やリザルト画面の統計行に使用。
inline void draw_label_value(Rectangle rect, const char* label, const char* value,
                             int font_size, Color label_color, Color value_color,
                             float label_width = 200.0f) {
    const Rectangle label_rect = {rect.x, rect.y, label_width, rect.height};
    const Rectangle value_rect = {rect.x + label_width, rect.y,
                                  rect.width - label_width, rect.height};
    draw_text_in_rect(label, font_size, label_rect, label_color, text_align::left);
    draw_text_in_rect(value, font_size, value_rect, value_color, text_align::left);
}

// ── プログレスバー ──────────────────────────────────────

// 水平プログレスバー。ヘルスゲージ等に使用する。
// ratio: 0.0〜1.0 の塗り割合。
inline void draw_progress_bar(Rectangle rect, float ratio,
                              Color bg, Color fill, Color border_color,
                              float border_width = 3.0f, float bar_inset = 4.0f) {
    DrawRectangleRec(rect, bg);
    DrawRectangleLinesEx(rect, border_width, border_color);

    if (ratio > 0.0f) {
        const Rectangle fill_area = inset(rect, bar_inset);
        const float fill_w = fill_area.width * std::clamp(ratio, 0.0f, 1.0f);
        DrawRectangle(static_cast<int>(fill_area.x), static_cast<int>(fill_area.y),
                      static_cast<int>(fill_w), static_cast<int>(fill_area.height), fill);
    }
}

// ── スライダー ──────────────────────────────────────────

// スライダー行を描画する。行背景 + ラベル + トラック + 塗り + つまみ + 値テキスト。
// ratio: 0.0〜1.0 の現在値。
// row_rect: 行全体の Rectangle。
// track_left: トラック開始X座標（row_rect.x からの相対ではなく絶対座標）。
// track_width: トラックの幅。
// 戻り値: マウスがトラック上にある場合のドラッグ ratio（0.0〜1.0）。ドラッグ中でなければ -1.0f。
inline float draw_slider(Rectangle row_rect, const char* label, const char* value_text,
                         float ratio, float track_left, float track_width,
                         int font_size = 22, float track_top_offset = 26.0f) {
    // 行背景
    DrawRectangleRec(row_rect, g_theme->row);
    DrawRectangleLinesEx(row_rect, 2.0f, g_theme->border);

    // ラベル（左寄せ）
    const Rectangle label_rect = {row_rect.x + 18.0f, row_rect.y, 200.0f, row_rect.height};
    draw_text_in_rect(label, font_size, label_rect, g_theme->text, text_align::left);

    // トラック
    const Rectangle track = {track_left, row_rect.y + track_top_offset, track_width, 6.0f};
    const float clamped = std::clamp(ratio, 0.0f, 1.0f);
    DrawRectangleRec(track, g_theme->slider_track);
    DrawRectangle(static_cast<int>(track.x), static_cast<int>(track.y),
                  static_cast<int>(track.width * clamped), static_cast<int>(track.height),
                  g_theme->slider_fill);

    // つまみ
    const int knob_x = static_cast<int>(track.x + track.width * clamped);
    DrawRectangle(knob_x - 6, static_cast<int>(track.y - 8.0f), 12, 22, g_theme->slider_knob);

    // 値テキスト（右上）
    const Rectangle value_rect = {track.x, row_rect.y, track.width, track_top_offset};
    draw_text_in_rect(value_text, font_size, value_rect, g_theme->text_dim, text_align::right);

    // ドラッグ判定: マウスが押されていてトラック行内にある場合、ratio を返す
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        if (CheckCollisionPointRec(mouse, row_rect)) {
            return std::clamp((mouse.x - track.x) / track.width, 0.0f, 1.0f);
        }
    }
    return -1.0f;
}

// ── オーバーレイ ────────────────────────────────────────

// 画面全体を覆う半透明オーバーレイ。ポーズ画面やフェードイン/アウトに使用する。
inline void draw_fullscreen_overlay(Color color) {
    DrawRectangle(0, 0, 1280, 720, color);
}

}  // namespace ui
