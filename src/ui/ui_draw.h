#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"
#include "ui_coord.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_hit.h"
#include "ui_layout.h"
#include "ui_text.h"

// 繰り返し使用される UI 描画パターンのユーティリティ。
// theme.h の g_theme を参照するため、テーマ設定後に使用すること。
namespace ui {

struct draw_command {
    int layer = 0;
    std::uint64_t order = 0;
    std::function<void()> draw;
};

inline std::vector<draw_command>& draw_queue() {
    static std::vector<draw_command> queue;
    return queue;
}

inline std::uint64_t& draw_queue_order() {
    static std::uint64_t order = 0;
    return order;
}

// 即時描画と併存する frame-local な UI 描画キュー。
// overlay から段階導入し、将来的には hit test 優先順位も同じ layer に揃える前提。
inline void begin_draw_queue() {
    draw_queue().clear();
    draw_queue_order() = 0;
}

inline void enqueue_draw_command(draw_layer layer, std::function<void()> draw) {
    draw_queue().push_back({static_cast<int>(layer), draw_queue_order()++, std::move(draw)});
}

template <typename Fn>
inline void enqueue_draw_command(draw_layer layer, Fn&& draw) {
    enqueue_draw_command(layer, std::function<void()>(std::forward<Fn>(draw)));
}

inline void flush_draw_queue() {
    auto& queue = draw_queue();
    std::stable_sort(queue.begin(), queue.end(), [](const draw_command& left, const draw_command& right) {
        if (left.layer != right.layer) {
            return left.layer < right.layer;
        }
        return left.order < right.order;
    });

    for (const draw_command& command : queue) {
        command.draw();
    }
    queue.clear();
}

namespace detail {

inline void draw_button_visual(Rectangle rect, bool hovered, bool pressed, const char* label,
                               int font_size, Color bg, Color bg_hover, Color text_color,
                               float border_width) {
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    DrawRectangleRec(visual, lerp_color(bg, bg_hover, hovered ? 1.0f : 0.0f));
    DrawRectangleLinesEx(visual, border_width, g_theme->border);
    draw_text_in_rect(label, font_size, visual, text_color);
}

inline void draw_row_visual(Rectangle rect, bool hovered, bool pressed, Color bg, Color bg_hover,
                            Color border_color, float border_width) {
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;
    DrawRectangleRec(visual, lerp_color(bg, bg_hover, hovered ? 1.0f : 0.0f));
    DrawRectangleLinesEx(visual, border_width, border_color);
}

}  // namespace detail

// ── ボタン ──────────────────────────────────────────────

// ボタンの描画状態。draw_button が返し、呼び出し側でクリック判定に使用する。
struct button_state {
    bool hovered;
    bool pressed;
    bool clicked;
};

struct row_state {
    bool hovered;
    bool pressed;
    bool clicked;
    Rectangle visual;
};

struct selector_state {
    row_state row;
    button_state left;
    button_state right;
};

struct dropdown_state {
    row_state trigger;
    int clicked_index;
};

struct slider_layout {
    Rectangle label_rect;
    Rectangle track_rect;
    Rectangle value_rect;
};

struct scrollbar_interaction {
    float scroll_offset;
    bool changed;
    bool dragging;
};

// 標準ボタンを描画する。hover で色変化、press で 1.5px 押し込み、テキスト中央揃え。
// 戻り値の clicked でアクション判定できる。
inline button_state draw_button(Rectangle rect, const char* label, int font_size,
                                float border_width = 2.0f) {
    const bool hovered = is_hovered(rect);
    const bool pressed = is_pressed(rect);
    const bool clicked = is_clicked(rect);
    detail::draw_button_visual(rect, hovered, pressed, label, font_size,
                               g_theme->row, g_theme->row_hover, g_theme->text, border_width);

    return {hovered, pressed, clicked};
}

// カスタム色のボタン。選択状態やアクティブ状態の表現に使用する。
inline button_state draw_button_colored(Rectangle rect, const char* label, int font_size,
                                        Color bg, Color bg_hover, Color text_color,
                                        float border_width = 2.0f) {
    const bool hovered = is_hovered(rect);
    const bool pressed = is_pressed(rect);
    const bool clicked = is_clicked(rect);
    detail::draw_button_visual(rect, hovered, pressed, label, font_size,
                               bg, bg_hover, text_color, border_width);

    return {hovered, pressed, clicked};
}

// 背景とボーダーのみを持つ汎用行。中身のテキストやアイコンは呼び出し側で描画する。
inline row_state draw_row(Rectangle rect, Color bg, Color bg_hover, Color border_color,
                          float border_width = 2.0f) {
    const bool hovered = is_hovered(rect);
    const bool pressed = is_pressed(rect);
    const bool clicked = is_clicked(rect);
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;

    detail::draw_row_visual(rect, hovered, pressed, bg, bg_hover, border_color, border_width);
    return {hovered, pressed, clicked, visual};
}

inline row_state draw_selectable_row(Rectangle rect, bool selected,
                                     float border_width = 2.0f) {
    return draw_row(rect,
                    selected ? g_theme->row_selected : g_theme->row,
                    selected ? g_theme->row_active : g_theme->row_hover,
                    selected ? g_theme->border_active : g_theme->border,
                    border_width);
}

inline button_state enqueue_button(Rectangle rect, const char* label, int font_size,
                                   draw_layer layer = draw_layer::base,
                                   float border_width = 2.0f) {
    const bool hovered = is_hovered(rect, layer);
    const bool pressed = is_pressed(rect, layer);
    const bool clicked = is_clicked(rect, layer);
    const std::string label_copy = label != nullptr ? label : "";

    enqueue_draw_command(layer, [rect, hovered, pressed, label_copy, font_size, border_width]() {
        detail::draw_button_visual(rect, hovered, pressed, label_copy.c_str(), font_size,
                                   g_theme->row, g_theme->row_hover, g_theme->text, border_width);
    });

    return {hovered, pressed, clicked};
}

inline row_state enqueue_row(Rectangle rect, Color bg, Color bg_hover, Color border_color,
                             draw_layer layer = draw_layer::base,
                             float border_width = 2.0f) {
    const bool hovered = is_hovered(rect, layer);
    const bool pressed = is_pressed(rect, layer);
    const bool clicked = is_clicked(rect, layer);
    const Rectangle visual = pressed ? inset(rect, 1.5f) : rect;

    enqueue_draw_command(layer, [rect, hovered, pressed, bg, bg_hover, border_color, border_width]() {
        detail::draw_row_visual(rect, hovered, pressed, bg, bg_hover, border_color, border_width);
    });

    return {hovered, pressed, clicked, visual};
}

// ── パネル ──────────────────────────────────────────────

// メインパネル（panel 背景 + border ボーダー、2px）。
inline void draw_panel(Rectangle rect) {
    DrawRectangleRec(rect, g_theme->panel);
    DrawRectangleLinesEx(rect, 2.0f, g_theme->border);
}

inline void enqueue_panel(Rectangle rect, draw_layer layer = draw_layer::base) {
    enqueue_draw_command(layer, [rect]() {
        draw_panel(rect);
    });
}

// セクションパネル（section 背景 + border_light ボーダー、1.5px）。
inline void draw_section(Rectangle rect) {
    DrawRectangleRec(rect, g_theme->section);
    DrawRectangleLinesEx(rect, 1.5f, g_theme->border_light);
}

inline void enqueue_section(Rectangle rect, draw_layer layer = draw_layer::base) {
    enqueue_draw_command(layer, [rect]() {
        draw_section(rect);
    });
}

inline void enqueue_text_in_rect(const char* text, int font_size, Rectangle rect, Color color,
                                 text_align align = text_align::center,
                                 draw_layer layer = draw_layer::base) {
    const std::string text_copy = text != nullptr ? text : "";
    enqueue_draw_command(layer, [text_copy, font_size, rect, color, align]() {
        draw_text_in_rect(text_copy.c_str(), font_size, rect, color, align);
    });
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

inline void draw_label_value_marquee(Rectangle rect, const char* label, const char* value,
                                     int font_size, Color label_color, Color value_color,
                                     double time, float label_width = 200.0f) {
    const Rectangle label_rect = {rect.x, rect.y, label_width, rect.height};
    const Rectangle value_rect = {rect.x + label_width, rect.y,
                                  rect.width - label_width, rect.height};
    draw_text_in_rect(label, font_size, label_rect, label_color, text_align::left);
    draw_marquee_text(value, value_rect.x, value_rect.y + 4.0f, font_size,
                      value_color, value_rect.width, time);
}

inline selector_state draw_value_selector(Rectangle rect, const char* label, const char* value,
                                          int font_size = 24, float button_size = 34.0f,
                                          float label_width = 200.0f, float content_padding = 18.0f) {
    const row_state row = draw_row(rect, g_theme->row, g_theme->row_hover, g_theme->border);

    const Rectangle content = inset(row.visual, edge_insets::symmetric(0.0f, content_padding));
    const rect_pair columns = split_columns(content, label_width);
    const Rectangle button_pair_area = place(columns.second, button_size * 2.0f + 10.0f, button_size,
                                             anchor::center_right, anchor::center_right);
    Rectangle buttons[2];
    hstack(button_pair_area, button_size, 10.0f, buttons);

    const Rectangle value_rect = {
        columns.second.x,
        columns.second.y,
        button_pair_area.x - columns.second.x - 16.0f,
        columns.second.height
    };

    draw_text_in_rect(label, font_size, columns.first, g_theme->text, text_align::left);
    draw_text_in_rect(value, font_size, value_rect, g_theme->text_dim, text_align::right);
    const button_state left = draw_button(buttons[0], "<", font_size);
    const button_state right = draw_button(buttons[1], ">", font_size);
    return {row, left, right};
}

inline selector_state draw_value_selector(Rectangle rect, const char* label, const char* value,
                                          draw_layer layer,
                                          int font_size = 24, float button_size = 34.0f,
                                          float label_width = 200.0f, float content_padding = 18.0f) {
    const bool row_hovered = is_hovered(rect, layer);
    const bool row_pressed = is_pressed(rect, layer);
    const bool row_clicked = is_clicked(rect, layer);
    const Rectangle visual = row_pressed ? inset(rect, 1.5f) : rect;
    detail::draw_row_visual(rect, row_hovered, row_pressed, g_theme->row, g_theme->row_hover, g_theme->border, 2.0f);
    const row_state row = {row_hovered, row_pressed, row_clicked, visual};

    const Rectangle content = inset(row.visual, edge_insets::symmetric(0.0f, content_padding));
    const rect_pair columns = split_columns(content, label_width);
    const Rectangle button_pair_area = place(columns.second, button_size * 2.0f + 10.0f, button_size,
                                             anchor::center_right, anchor::center_right);
    Rectangle buttons[2];
    hstack(button_pair_area, button_size, 10.0f, buttons);

    const Rectangle value_rect = {
        columns.second.x,
        columns.second.y,
        button_pair_area.x - columns.second.x - 16.0f,
        columns.second.height
    };

    draw_text_in_rect(label, font_size, columns.first, g_theme->text, text_align::left);
    draw_text_in_rect(value, font_size, value_rect, g_theme->text_dim, text_align::right);

    const bool left_hovered = is_hovered(buttons[0], layer);
    const bool left_pressed = is_pressed(buttons[0], layer);
    const bool left_clicked = is_clicked(buttons[0], layer);
    detail::draw_button_visual(buttons[0], left_hovered, left_pressed, "<", font_size,
                               g_theme->row, g_theme->row_hover, g_theme->text, 2.0f);

    const bool right_hovered = is_hovered(buttons[1], layer);
    const bool right_pressed = is_pressed(buttons[1], layer);
    const bool right_clicked = is_clicked(buttons[1], layer);
    detail::draw_button_visual(buttons[1], right_hovered, right_pressed, ">", font_size,
                               g_theme->row, g_theme->row_hover, g_theme->text, 2.0f);

    const button_state left = {left_hovered, left_pressed, left_clicked};
    const button_state right = {right_hovered, right_pressed, right_clicked};
    return {row, left, right};
}

inline dropdown_state draw_dropdown(Rectangle trigger_rect, Rectangle menu_rect,
                                    const char* label, const char* value,
                                    std::span<const char* const> options,
                                    int selected_index, bool open,
                                    int font_size = 18, float label_width = 72.0f,
                                    float content_padding = 12.0f,
                                    float item_height = 30.0f, float item_spacing = 4.0f) {
    const row_state trigger = draw_row(trigger_rect,
                                       open ? g_theme->row_selected : g_theme->row,
                                       open ? g_theme->row_selected_hover : g_theme->row_hover,
                                       open ? g_theme->border_active : g_theme->border);
    const Rectangle content = inset(trigger.visual, edge_insets::symmetric(0.0f, content_padding));
    const rect_pair columns = split_columns(content, label_width);
    const Rectangle arrow_rect = place(columns.second, 18.0f, columns.second.height,
                                       anchor::center_right, anchor::center_right);
    const Rectangle value_rect = {
        columns.second.x,
        columns.second.y,
        std::max(0.0f, arrow_rect.x - columns.second.x - 8.0f),
        columns.second.height
    };

    draw_text_in_rect(label, font_size, columns.first, g_theme->text, text_align::left);
    draw_text_in_rect(value, font_size, value_rect, g_theme->text_dim, text_align::right);
    draw_text_in_rect(open ? "^" : "v", font_size, arrow_rect, g_theme->text_dim);

    int clicked_index = -1;
    if (open) {
        draw_section(menu_rect);
        Rectangle item_rect = {menu_rect.x + 6.0f, menu_rect.y + 6.0f, menu_rect.width - 12.0f, item_height};
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            const row_state option_row = draw_selectable_row(item_rect, i == selected_index, 1.5f);
            draw_text_in_rect(options[i], font_size, inset(option_row.visual, edge_insets::symmetric(0.0f, 12.0f)),
                              i == selected_index ? g_theme->text : g_theme->text_dim, text_align::left);
            if (option_row.clicked) {
                clicked_index = i;
            }
            item_rect.y += item_height + item_spacing;
        }
    }

    return {trigger, clicked_index};
}

inline dropdown_state enqueue_dropdown(Rectangle trigger_rect, Rectangle menu_rect,
                                       const char* label, const char* value,
                                       std::span<const char* const> options,
                                       int selected_index, bool open,
                                       draw_layer trigger_layer = draw_layer::base,
                                       draw_layer menu_layer = draw_layer::overlay,
                                       int font_size = 18, float label_width = 72.0f,
                                       float content_padding = 12.0f,
                                       float item_height = 30.0f, float item_spacing = 4.0f) {
    if (open) {
        register_hit_region(menu_rect, menu_layer);
    }

    const bool trigger_pressed = is_pressed(trigger_rect, trigger_layer);
    const row_state trigger = {
        is_hovered(trigger_rect, trigger_layer),
        trigger_pressed,
        is_clicked(trigger_rect, trigger_layer),
        trigger_pressed ? inset(trigger_rect, 1.5f) : trigger_rect
    };

    std::vector<std::string> option_copies;
    option_copies.reserve(options.size());
    for (const char* option : options) {
        option_copies.emplace_back(option != nullptr ? option : "");
    }
    const std::string label_copy = label != nullptr ? label : "";
    const std::string value_copy = value != nullptr ? value : "";

    enqueue_draw_command(trigger_layer, [trigger_rect, trigger, label_copy, value_copy, font_size,
                                         label_width, content_padding, open]() {
        const Rectangle trigger_content = inset(trigger.visual, edge_insets::symmetric(0.0f, content_padding));
        const rect_pair trigger_columns = split_columns(trigger_content, label_width);
        const Rectangle trigger_arrow_rect = place(trigger_columns.second, 18.0f, trigger_columns.second.height,
                                                   anchor::center_right, anchor::center_right);
        const Rectangle trigger_value_rect = {
            trigger_columns.second.x,
            trigger_columns.second.y,
            std::max(0.0f, trigger_arrow_rect.x - trigger_columns.second.x - 8.0f),
            trigger_columns.second.height
        };

        detail::draw_row_visual(trigger_rect, trigger.hovered, trigger.pressed,
                                open ? g_theme->row_selected : g_theme->row,
                                open ? g_theme->row_selected_hover : g_theme->row_hover,
                                open ? g_theme->border_active : g_theme->border,
                                2.0f);
        draw_text_in_rect(label_copy.c_str(), font_size, trigger_columns.first, g_theme->text, text_align::left);
        draw_text_in_rect(value_copy.c_str(), font_size, trigger_value_rect, g_theme->text_dim, text_align::right);
        draw_text_in_rect(open ? "^" : "v", font_size, trigger_arrow_rect, g_theme->text_dim);
    });

    int clicked_index = -1;
    if (open) {
        Rectangle item_rect = {menu_rect.x + 6.0f, menu_rect.y + 6.0f, menu_rect.width - 12.0f, item_height};
        std::vector<row_state> item_states;
        item_states.reserve(option_copies.size());

        for (int i = 0; i < static_cast<int>(option_copies.size()); ++i) {
            const bool item_pressed = is_pressed(item_rect, menu_layer);
            const row_state option_row = {
                is_hovered(item_rect, menu_layer),
                item_pressed,
                is_clicked(item_rect, menu_layer),
                item_pressed ? inset(item_rect, 1.5f) : item_rect
            };
            item_states.push_back(option_row);
            if (option_row.clicked) {
                clicked_index = i;
            }
            item_rect.y += item_height + item_spacing;
        }

        enqueue_draw_command(menu_layer, [menu_rect, item_height, item_spacing, font_size, selected_index,
                                          option_copies = std::move(option_copies),
                                          item_states = std::move(item_states)]() {
            draw_section(menu_rect);
            Rectangle draw_item_rect = {menu_rect.x + 6.0f, menu_rect.y + 6.0f, menu_rect.width - 12.0f, item_height};
            for (int i = 0; i < static_cast<int>(option_copies.size()); ++i) {
                const row_state& option_row = item_states[static_cast<size_t>(i)];
                detail::draw_row_visual(draw_item_rect, option_row.hovered, option_row.pressed,
                                        i == selected_index ? g_theme->row_selected : g_theme->row,
                                        i == selected_index ? g_theme->row_active : g_theme->row_hover,
                                        i == selected_index ? g_theme->border_active : g_theme->border,
                                        1.5f);
                draw_text_in_rect(option_copies[static_cast<size_t>(i)].c_str(), font_size,
                                  inset(option_row.visual, edge_insets::symmetric(0.0f, 12.0f)),
                                  i == selected_index ? g_theme->text : g_theme->text_dim, text_align::left);
                draw_item_rect.y += item_height + item_spacing;
            }
        });
    }

    return {trigger, clicked_index};
}

inline void draw_header_block(Rectangle rect, const char* title, const char* subtitle,
                              int title_size = 34, int subtitle_size = 20,
                              float spacing = 8.0f) {
    const rect_pair rows = split_rows(rect, static_cast<float>(title_size), spacing);
    draw_text_in_rect(title, title_size, rows.first, g_theme->text, text_align::left);
    draw_text_in_rect(subtitle, subtitle_size, rows.second, g_theme->text_muted, text_align::left);
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
        draw_rect_f(fill_area.x, fill_area.y, fill_w, fill_area.height, fill);
    }
}

// ── スライダー ──────────────────────────────────────────

inline slider_layout make_slider_layout(Rectangle row_rect, float track_left_inset, float track_right_inset,
                                        float label_width = 200.0f, float content_padding = 18.0f,
                                        float track_top_offset = 26.0f) {
    const Rectangle content = inset(row_rect, edge_insets::symmetric(0.0f, content_padding));
    const Rectangle label_rect = {content.x, content.y, label_width, content.height};
    const float track_left = row_rect.x + track_left_inset;
    const float track_width = row_rect.width - track_left_inset - track_right_inset;
    return {
        label_rect,
        {track_left, row_rect.y + track_top_offset, track_width, 6.0f},
        {track_left, row_rect.y, track_width, track_top_offset}
    };
}

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
    draw_rect_f(track.x, track.y, track.width * clamped, track.height, g_theme->slider_fill);

    // つまみ
    const float knob_x = track.x + track.width * clamped;
    draw_rect_f(knob_x - 6.0f, track.y - 8.0f, 12.0f, 22.0f, g_theme->slider_knob);

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

inline float draw_slider(Rectangle row_rect, const char* label, const char* value_text,
                         float ratio, float track_left, float track_width, draw_layer layer,
                         int font_size = 22, float track_top_offset = 26.0f) {
    DrawRectangleRec(row_rect, g_theme->row);
    DrawRectangleLinesEx(row_rect, 2.0f, g_theme->border);

    const Rectangle label_rect = {row_rect.x + 18.0f, row_rect.y, 200.0f, row_rect.height};
    draw_text_in_rect(label, font_size, label_rect, g_theme->text, text_align::left);

    const Rectangle track = {track_left, row_rect.y + track_top_offset, track_width, 6.0f};
    const float clamped = std::clamp(ratio, 0.0f, 1.0f);
    DrawRectangleRec(track, g_theme->slider_track);
    draw_rect_f(track.x, track.y, track.width * clamped, track.height, g_theme->slider_fill);

    const float knob_x = track.x + track.width * clamped;
    draw_rect_f(knob_x - 6.0f, track.y - 8.0f, 12.0f, 22.0f, g_theme->slider_knob);

    const Rectangle value_rect = {track.x, row_rect.y, track.width, track_top_offset};
    draw_text_in_rect(value_text, font_size, value_rect, g_theme->text_dim, text_align::right);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && is_hovered(row_rect, layer)) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        return std::clamp((mouse.x - track.x) / track.width, 0.0f, 1.0f);
    }
    return -1.0f;
}

// スライダー行を描画する。track_left_inset / track_right_inset は row_rect 基準の相対指定。
inline float draw_slider_relative(Rectangle row_rect, const char* label, const char* value_text,
                                  float ratio, float track_left_inset, float track_right_inset,
                                  int font_size = 22, float track_top_offset = 26.0f,
                                  float label_width = 200.0f, float content_padding = 18.0f) {
    const row_state row = draw_row(row_rect, g_theme->row, g_theme->row_hover, g_theme->border);
    const slider_layout layout = make_slider_layout(row.visual, track_left_inset, track_right_inset,
                                                    label_width, content_padding, track_top_offset);
    const float clamped = std::clamp(ratio, 0.0f, 1.0f);

    draw_text_in_rect(label, font_size, layout.label_rect, g_theme->text, text_align::left);
    DrawRectangleRec(layout.track_rect, g_theme->slider_track);
    draw_rect_f(layout.track_rect.x, layout.track_rect.y,
                layout.track_rect.width * clamped, layout.track_rect.height, g_theme->slider_fill);

    const float knob_x = layout.track_rect.x + layout.track_rect.width * clamped;
    draw_rect_f(knob_x - 6.0f, layout.track_rect.y - 8.0f, 12.0f, 22.0f, g_theme->slider_knob);
    draw_text_in_rect(value_text, font_size, layout.value_rect, g_theme->text_dim, text_align::right);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), row_rect)) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        return std::clamp((mouse.x - layout.track_rect.x) / layout.track_rect.width, 0.0f, 1.0f);
    }
    return -1.0f;
}

inline float draw_slider_relative(Rectangle row_rect, const char* label, const char* value_text,
                                  float ratio, float track_left_inset, float track_right_inset,
                                  draw_layer layer,
                                  int font_size = 22, float track_top_offset = 26.0f,
                                  float label_width = 200.0f, float content_padding = 18.0f) {
    const bool hovered = is_hovered(row_rect, layer);
    const bool pressed = is_pressed(row_rect, layer);
    const Rectangle visual = pressed ? inset(row_rect, 1.5f) : row_rect;
    detail::draw_row_visual(row_rect, hovered, pressed, g_theme->row, g_theme->row_hover, g_theme->border, 2.0f);
    const row_state row = {hovered, pressed, is_clicked(row_rect, layer), visual};
    const slider_layout layout = make_slider_layout(row.visual, track_left_inset, track_right_inset,
                                                    label_width, content_padding, track_top_offset);
    const float clamped = std::clamp(ratio, 0.0f, 1.0f);

    draw_text_in_rect(label, font_size, layout.label_rect, g_theme->text, text_align::left);
    DrawRectangleRec(layout.track_rect, g_theme->slider_track);
    draw_rect_f(layout.track_rect.x, layout.track_rect.y,
                layout.track_rect.width * clamped, layout.track_rect.height, g_theme->slider_fill);

    const float knob_x = layout.track_rect.x + layout.track_rect.width * clamped;
    draw_rect_f(knob_x - 6.0f, layout.track_rect.y - 8.0f, 12.0f, 22.0f, g_theme->slider_knob);
    draw_text_in_rect(value_text, font_size, layout.value_rect, g_theme->text_dim, text_align::right);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && is_hovered(row_rect, layer)) {
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        return std::clamp((mouse.x - layout.track_rect.x) / layout.track_rect.width, 0.0f, 1.0f);
    }
    return -1.0f;
}

inline void draw_scrollbar(Rectangle track_rect, float content_height, float scroll_offset,
                           Color track_color, Color thumb_color, float min_thumb_height = 36.0f) {
    const scroll_metrics metrics = vertical_scroll_metrics(track_rect, content_height, scroll_offset, min_thumb_height);
    if (content_height <= track_rect.height) {
        return;
    }

    DrawRectangleRec(track_rect, track_color);
    DrawRectangleRec(metrics.thumb_rect, thumb_color);
}

inline scrollbar_interaction update_vertical_scrollbar(Rectangle track_rect, float content_height, float scroll_offset,
                                                       bool& dragging, float& drag_offset,
                                                       float min_thumb_height = 36.0f) {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        dragging = false;
    }

    const scroll_metrics metrics = vertical_scroll_metrics(track_rect, content_height, scroll_offset, min_thumb_height);
    if (metrics.max_scroll <= 0.0f) {
        return {0.0f, false, false};
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    float next_offset = std::clamp(scroll_offset, 0.0f, metrics.max_scroll);
    bool changed = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, track_rect)) {
        if (CheckCollisionPointRec(mouse, metrics.thumb_rect)) {
            dragging = true;
            drag_offset = mouse.y - metrics.thumb_rect.y;
        } else {
            const float thumb_half = metrics.thumb_rect.height * 0.5f;
            const float available = std::max(1.0f, track_rect.height - metrics.thumb_rect.height);
            const float thumb_top = std::clamp(mouse.y - thumb_half - track_rect.y, 0.0f, available);
            next_offset = metrics.max_scroll * (thumb_top / available);
            changed = true;
        }
    }

    if (dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const float available = std::max(1.0f, track_rect.height - metrics.thumb_rect.height);
        const float thumb_top = std::clamp(mouse.y - drag_offset - track_rect.y, 0.0f, available);
        next_offset = metrics.max_scroll * (thumb_top / available);
        changed = true;
    }

    next_offset = std::clamp(next_offset, 0.0f, metrics.max_scroll);
    return {next_offset, changed, dragging};
}

inline scrollbar_interaction update_vertical_scrollbar(Rectangle track_rect, float content_height, float scroll_offset,
                                                       bool& dragging, float& drag_offset, draw_layer layer,
                                                       float min_thumb_height = 36.0f) {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        dragging = false;
    }

    const scroll_metrics metrics = vertical_scroll_metrics(track_rect, content_height, scroll_offset, min_thumb_height);
    if (metrics.max_scroll <= 0.0f) {
        return {0.0f, false, false};
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    float next_offset = std::clamp(scroll_offset, 0.0f, metrics.max_scroll);
    bool changed = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && is_hovered(track_rect, layer)) {
        if (CheckCollisionPointRec(mouse, metrics.thumb_rect)) {
            dragging = true;
            drag_offset = mouse.y - metrics.thumb_rect.y;
        } else {
            const float thumb_half = metrics.thumb_rect.height * 0.5f;
            const float available = std::max(1.0f, track_rect.height - metrics.thumb_rect.height);
            const float thumb_top = std::clamp(mouse.y - thumb_half - track_rect.y, 0.0f, available);
            next_offset = metrics.max_scroll * (thumb_top / available);
            changed = true;
        }
    }

    if (dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const float available = std::max(1.0f, track_rect.height - metrics.thumb_rect.height);
        const float thumb_top = std::clamp(mouse.y - drag_offset - track_rect.y, 0.0f, available);
        next_offset = metrics.max_scroll * (thumb_top / available);
        changed = true;
    }

    next_offset = std::clamp(next_offset, 0.0f, metrics.max_scroll);
    return {next_offset, changed, dragging};
}

// ── オーバーレイ ────────────────────────────────────────

// 画面全体を覆う半透明オーバーレイ。ポーズ画面やフェードイン/アウトに使用する。
inline void draw_fullscreen_overlay(Color color) {
    DrawRectangle(0, 0, 1280, 720, color);
}

inline void enqueue_fullscreen_overlay(Color color, draw_layer layer = draw_layer::overlay) {
    enqueue_draw_command(layer, [color]() {
        draw_fullscreen_overlay(color);
    });
}

}  // namespace ui
