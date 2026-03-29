#pragma once

#include <algorithm>

#include "raylib.h"

// UI 全体で使用するカラーパレット。ライト/ダークモードの切り替えに対応する。
// 色コードの直接指定を廃止し、すべてこのパレット経由で参照する。
struct ui_theme {
    // --- 背景 ---
    Color bg;                   // シーン全体の背景 / グラデーション開始色
    Color bg_alt;               // グラデーション終了色

    // --- パネル・カード ---
    Color panel;                // メインパネル / サイドバー背景
    Color section;              // パネル内のセクション背景

    // --- インタラクティブ要素 ---
    Color row;                  // ボタン・行の通常状態
    Color row_hover;            // ホバー状態
    Color row_selected;         // 選択状態
    Color row_selected_hover;   // 選択 + ホバー
    Color row_active;           // アクティブタブ背景
    Color row_list_hover;       // 未選択リスト項目のホバー

    // --- ボーダー ---
    Color border;               // 主要ボーダー
    Color border_light;         // 軽いボーダー（セクション区切り等）
    Color border_active;        // アクティブ要素のボーダー
    Color border_image;         // 画像枠のボーダー

    // --- テキスト ---
    Color text;                 // 主要テキスト
    Color text_secondary;       // やや薄いテキスト
    Color text_muted;           // 控えめなテキスト（チャート作者、ステータス等）
    Color text_dim;             // ラベル・値テキスト
    Color text_hint;            // ヒント・操作案内

    // --- コントロール ---
    Color slider_track;         // スライダーのトラック
    Color slider_fill;          // スライダーの塗り部分
    Color slider_knob;          // スライダーのつまみ
    Color scrollbar_track;      // スクロールバーのトラック
    Color scrollbar_thumb;      // スクロールバーのつまみ

    // --- プレイ画面 HUD ---
    Color hud_score;
    Color hud_time;
    Color hud_fps;
    Color hud_health_label;
    Color hud_health_bg;
    Color hud_health_border;
    Color hud_combo;
    Color hud_combo_label;
    Color hud_failure_text;

    // --- プレイ画面 ゲーム要素 ---
    Color lane;
    Color lane_wire;
    Color judge_line;
    Color judge_line_glow;
    Color note_color;
    Color note_outline;
    Color pause_overlay;
    Color pause_panel;

    // --- アクセント・ステータス ---
    Color accent;               // チャートラベル等のアクセント色
    Color combo;                // コンボ数表示
    Color error;                // エラー・失敗（赤）
    Color success;              // 成功・有効（緑）
    Color fast;                 // Fast 表示（青）
    Color slow;                 // Slow 表示（橙）
    Color health_high;          // ゲージ 70% 以上
    Color health_low;           // ゲージ 70% 未満
    Color all_perfect;          // ALL PERFECT 称号
    Color full_combo;           // FULL COMBO 称号

    // --- 判定色 ---
    Color judge_perfect;
    Color judge_great;
    Color judge_good;
    Color judge_bad;
    Color judge_miss;

    // --- ランク色 ---
    Color rank_ss;
    Color rank_s;
    Color rank_a;
    Color rank_b;
    Color rank_c;
    Color rank_f;
};

// ライトテーマ
inline constexpr ui_theme kLightTheme = {
    // bg
    .bg = {255, 255, 255, 255},
    .bg_alt = {241, 243, 246, 255},
    // panel
    .panel = {248, 249, 251, 255},
    .section = {240, 242, 246, 255},
    // interactive
    .row = {243, 245, 248, 255},
    .row_hover = {228, 233, 239, 255},
    .row_selected = {223, 228, 234, 255},
    .row_selected_hover = {214, 220, 227, 255},
    .row_active = {210, 216, 224, 255},
    .row_list_hover = {236, 240, 245, 255},
    // border
    .border = {206, 210, 218, 255},
    .border_light = {216, 220, 228, 255},
    .border_active = {182, 186, 194, 255},
    .border_image = {196, 200, 208, 255},
    // text
    .text = {0, 0, 0, 255},
    .text_secondary = {80, 80, 80, 255},
    .text_muted = {132, 136, 146, 255},
    .text_dim = {96, 100, 108, 255},
    .text_hint = {160, 164, 172, 255},
    // controls
    .slider_track = {214, 219, 226, 255},
    .slider_fill = {132, 136, 146, 255},
    .slider_knob = {72, 72, 72, 255},
    .scrollbar_track = {226, 230, 236, 255},
    .scrollbar_thumb = {172, 178, 188, 255},
    // hud
    .hud_score = {230, 232, 238, 255},
    .hud_time = {214, 218, 228, 255},
    .hud_fps = {160, 166, 178, 255},
    .hud_health_label = {72, 78, 90, 255},
    .hud_health_bg = {235, 238, 242, 255},
    .hud_health_border = {96, 104, 116, 255},
    .hud_combo = {240, 244, 250, 255},
    .hud_combo_label = {209, 214, 224, 255},
    .hud_failure_text = {244, 246, 250, 255},
    // game elements
    .lane = {182, 186, 194, 255},
    .lane_wire = {120, 130, 148, 180},
    .judge_line = {90, 150, 190, 110},
    .judge_line_glow = {180, 230, 255, 200},
    .note_color = {233, 238, 244, 255},
    .note_outline = {120, 128, 138, 255},
    .pause_overlay = {3, 6, 10, 150},
    .pause_panel = {248, 249, 251, 245},
    // accent/status
    .accent = {124, 58, 237, 255},
    .combo = {100, 160, 255, 255},
    .error = {220, 38, 38, 255},
    .success = {14, 146, 108, 255},
    .fast = {50, 120, 220, 255},
    .slow = {220, 120, 50, 255},
    .health_high = {99, 204, 161, 255},
    .health_low = {228, 109, 98, 255},
    .all_perfect = {200, 50, 200, 255},
    .full_combo = {14, 146, 108, 255},
    // judge
    .judge_perfect = {239, 244, 255, 255},
    .judge_great = {123, 211, 255, 255},
    .judge_good = {141, 211, 173, 255},
    .judge_bad = {255, 190, 92, 255},
    .judge_miss = {255, 107, 107, 255},
    // rank
    .rank_ss = {200, 50, 200, 255},
    .rank_s = {200, 50, 200, 255},
    .rank_a = {220, 50, 50, 255},
    .rank_b = {50, 100, 220, 255},
    .rank_c = {210, 180, 30, 255},
    .rank_f = {120, 120, 120, 255},
};

// ダークテーマ
inline constexpr ui_theme kDarkTheme = {
    // bg
    .bg = {24, 26, 30, 255},
    .bg_alt = {18, 20, 24, 255},
    // panel
    .panel = {32, 34, 38, 255},
    .section = {40, 42, 48, 255},
    // interactive
    .row = {44, 46, 52, 255},
    .row_hover = {56, 60, 68, 255},
    .row_selected = {52, 56, 64, 255},
    .row_selected_hover = {62, 66, 76, 255},
    .row_active = {58, 62, 72, 255},
    .row_list_hover = {48, 52, 58, 255},
    // border
    .border = {60, 64, 72, 255},
    .border_light = {52, 56, 64, 255},
    .border_active = {80, 84, 92, 255},
    .border_image = {64, 68, 76, 255},
    // text
    .text = {230, 232, 238, 255},
    .text_secondary = {180, 184, 192, 255},
    .text_muted = {150, 154, 162, 255},
    .text_dim = {160, 164, 172, 255},
    .text_hint = {120, 124, 132, 255},
    // controls
    .slider_track = {52, 56, 62, 255},
    .slider_fill = {150, 154, 162, 255},
    .slider_knob = {200, 200, 200, 255},
    .scrollbar_track = {42, 46, 52, 255},
    .scrollbar_thumb = {90, 94, 102, 255},
    // hud
    .hud_score = {220, 222, 228, 255},
    .hud_time = {180, 184, 194, 255},
    .hud_fps = {130, 136, 148, 255},
    .hud_health_label = {180, 186, 198, 255},
    .hud_health_bg = {36, 38, 44, 255},
    .hud_health_border = {120, 128, 140, 255},
    .hud_combo = {220, 224, 230, 255},
    .hud_combo_label = {140, 144, 154, 255},
    .hud_failure_text = {220, 222, 228, 255},
    // game elements
    .lane = {60, 64, 72, 255},
    .lane_wire = {80, 90, 108, 180},
    .judge_line = {70, 130, 170, 130},
    .judge_line_glow = {120, 180, 220, 200},
    .note_color = {200, 208, 220, 255},
    .note_outline = {160, 168, 178, 255},
    .pause_overlay = {0, 0, 0, 180},
    .pause_panel = {32, 34, 38, 245},
    // accent/status
    .accent = {158, 100, 255, 255},
    .combo = {120, 180, 255, 255},
    .error = {240, 60, 60, 255},
    .success = {40, 180, 130, 255},
    .fast = {80, 150, 240, 255},
    .slow = {240, 150, 70, 255},
    .health_high = {99, 204, 161, 255},
    .health_low = {228, 109, 98, 255},
    .all_perfect = {220, 80, 220, 255},
    .full_combo = {40, 180, 130, 255},
    // judge
    .judge_perfect = {200, 210, 255, 255},
    .judge_great = {100, 190, 240, 255},
    .judge_good = {120, 200, 160, 255},
    .judge_bad = {240, 180, 80, 255},
    .judge_miss = {240, 100, 100, 255},
    // rank
    .rank_ss = {220, 80, 220, 255},
    .rank_s = {220, 80, 220, 255},
    .rank_a = {240, 70, 70, 255},
    .rank_b = {80, 130, 240, 255},
    .rank_c = {230, 200, 50, 255},
    .rank_f = {140, 140, 140, 255},
};

// 現在のテーマ。起動時に g_settings.dark_mode に基づいて設定される。
inline const ui_theme* g_theme = &kLightTheme;

// テーマを切り替える。
inline void set_theme(bool dark_mode) {
    g_theme = dark_mode ? &kDarkTheme : &kLightTheme;
}

// 色の線形補間。ホバーアニメーション等に使用する。
inline Color lerp_color(Color from, Color to, float t) {
    const float c = std::clamp(t, 0.0f, 1.0f);
    return {
        static_cast<unsigned char>(from.r + (to.r - from.r) * c),
        static_cast<unsigned char>(from.g + (to.g - from.g) * c),
        static_cast<unsigned char>(from.b + (to.b - from.b) * c),
        static_cast<unsigned char>(from.a + (to.a - from.a) * c),
    };
}

// 色のアルファ値を差し替える。動的な透明度変化に使用する。
inline Color with_alpha(Color color, unsigned char alpha) {
    return {color.r, color.g, color.b, alpha};
}
