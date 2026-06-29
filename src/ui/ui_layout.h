#pragma once

#include <algorithm>
#include <optional>
#include <span>

#include "raylib.h"

// UI レイアウトシステム。
// Rectangle の位置・サイズを計算する純粋関数群。描画はシーン側の責任。
namespace ui {

struct rect_pair {
    Rectangle first;
    Rectangle second;
};

struct scroll_metrics {
    float max_scroll;
    Rectangle thumb_rect;
};

struct vertical_layout_cursor {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float gap = 0.0f;

    constexpr Rectangle next(float height) {
        const Rectangle rect{x, y, width, height};
        y += height + gap;
        return rect;
    }

    constexpr Rectangle peek(float height) const {
        return {x, y, width, height};
    }

    constexpr void skip(float amount) {
        y += amount;
    }
};

struct wrapped_layout_cursor {
    Rectangle bounds = {};
    float x = 0.0f;
    float y = 0.0f;
    float gap = 0.0f;
    float row_step = 0.0f;

    constexpr std::optional<Rectangle> next(float width, float height) {
        if (x + width > bounds.x + bounds.width && x > bounds.x) {
            x = bounds.x;
            y += row_step;
        }
        if (y + height > bounds.y + bounds.height) {
            return std::nullopt;
        }
        const Rectangle rect{x, y, width, height};
        x += width + gap;
        return rect;
    }
};

// ── 型定義 ──────────────────────────────────────────────

// 9点アンカー。親 Rectangle 上の基準点を指定する。
//
//   top_left ──── top_center ──── top_right
//       │              │              │
//   center_left ── center ──── center_right
//       │              │              │
//   bottom_left ─ bottom_center ─ bottom_right
//
enum class anchor : int {
    top_left = 0,
    top_center,
    top_right,
    center_left,
    center,
    center_right,
    bottom_left,
    bottom_center,
    bottom_right,
};

// 四辺のインセット。padding / margin に使用する。
struct edge_insets {
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;

    // 全辺同一値。
    static constexpr edge_insets uniform(float v) { return {v, v, v, v}; }

    // 上下・左右で対称。
    static constexpr edge_insets symmetric(float vertical, float horizontal) {
        return {vertical, horizontal, vertical, horizontal};
    }
};

// テキストの水平揃え。
enum class text_align { left, center, right };

// ── アンカー ────────────────────────────────────────────

// rect 上のアンカー座標を返す。
constexpr Vector2 anchor_point(Rectangle rect, anchor a) {
    const float col = static_cast<float>(static_cast<int>(a) % 3);
    const float row = static_cast<float>(static_cast<int>(a) / 3);
    return {
        rect.x + rect.width * (col * 0.5f),
        rect.y + rect.height * (row * 0.5f),
    };
}

// parent 上の parent_anchor に、child_size の child_anchor を合わせて配置する。
// offset で微調整可能。
constexpr Rectangle place(Rectangle parent,
                          float child_width, float child_height,
                          anchor parent_anchor,
                          anchor child_anchor = anchor::top_left,
                          Vector2 offset = {0, 0}) {
    const Vector2 p = anchor_point(parent, parent_anchor);
    const Vector2 c = anchor_point({0, 0, child_width, child_height}, child_anchor);
    return {
        p.x - c.x + offset.x,
        p.y - c.y + offset.y,
        child_width,
        child_height,
    };
}

// ── インセット ──────────────────────────────────────────

// rect を edge_insets 分だけ内側に縮小する。
constexpr Rectangle inset(Rectangle rect, edge_insets edges) {
    return {
        rect.x + edges.left,
        rect.y + edges.top,
        rect.width - edges.left - edges.right,
        rect.height - edges.top - edges.bottom,
    };
}

// 全辺均一にインセットする。
constexpr Rectangle inset(Rectangle rect, float amount) {
    return inset(rect, edge_insets::uniform(amount));
}

// ── 配置 ────────────────────────────────────────────────

// parent の中央に w x h の Rectangle を配置する。
constexpr Rectangle center(Rectangle parent, float w, float h) {
    return place(parent, w, h, anchor::center, anchor::center);
}

constexpr Rectangle centered_square(Rectangle parent, float size) {
    return center(parent, size, size);
}

constexpr Rectangle inscribed_square(Rectangle parent, float inset_amount = 0.0f) {
    const float size = std::max(1.0f, std::min(parent.width, parent.height) - inset_amount * 2.0f);
    return centered_square(parent, size);
}

constexpr Rectangle aspect_fit_rect(Rectangle bounds, float content_width, float content_height) {
    if (content_width <= 0.0f || content_height <= 0.0f || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return bounds;
    }

    const float scale = std::min(bounds.width / content_width, bounds.height / content_height);
    const float width = content_width * scale;
    const float height = content_height * scale;
    return center(bounds, width, height);
}

constexpr Rectangle scaled_from_center(Rectangle anchor,
                                       Rectangle target,
                                       float scale,
                                       Vector2 center_offset = {0.0f, 0.0f}) {
    const Vector2 center_point = {
        anchor.x + anchor.width * 0.5f + center_offset.x,
        anchor.y + anchor.height * 0.5f + center_offset.y,
    };
    const float width = target.width * scale;
    const float height = target.height * scale;
    return {
        center_point.x - width * 0.5f,
        center_point.y - height * 0.5f,
        width,
        height,
    };
}

constexpr Rectangle map_rect_between(Rectangle rect, Rectangle from_space, Rectangle to_space) {
    if (from_space.width == 0.0f || from_space.height == 0.0f) {
        return {to_space.x, to_space.y, 0.0f, 0.0f};
    }
    const float scale_x = to_space.width / from_space.width;
    const float scale_y = to_space.height / from_space.height;
    return {
        to_space.x + (rect.x - from_space.x) * scale_x,
        to_space.y + (rect.y - from_space.y) * scale_y,
        rect.width * scale_x,
        rect.height * scale_y,
    };
}

constexpr Rectangle translated(Rectangle rect, float dx, float dy) {
    return {
        rect.x + dx,
        rect.y + dy,
        rect.width,
        rect.height,
    };
}

constexpr Rectangle translated(Rectangle rect, Vector2 delta) {
    return translated(rect, delta.x, delta.y);
}

constexpr Vector2 map_point_between(Vector2 point, Rectangle from_space, Rectangle to_space) {
    if (from_space.width == 0.0f || from_space.height == 0.0f) {
        return {to_space.x, to_space.y};
    }
    return {
        to_space.x + (point.x - from_space.x) * to_space.width / from_space.width,
        to_space.y + (point.y - from_space.y) * to_space.height / from_space.height,
    };
}

// parent を左右2カラムに分割する。first_width が左カラム幅、spacing が列間。
constexpr rect_pair split_columns(Rectangle parent, float first_width, float spacing = 0.0f) {
    return {
        {parent.x, parent.y, first_width, parent.height},
        {parent.x + first_width + spacing, parent.y,
         parent.width - first_width - spacing, parent.height},
    };
}

// parent を main + 右端固定領域に分割する。second_width が右端領域幅、spacing が列間。
constexpr rect_pair split_trailing(Rectangle parent, float second_width, float spacing = 0.0f) {
    return {
        {parent.x, parent.y, parent.width - second_width - spacing, parent.height},
        {parent.x + parent.width - second_width, parent.y, second_width, parent.height},
    };
}

// parent を上下2行に分割する。first_height が上段高さ、spacing が行間。
constexpr rect_pair split_rows(Rectangle parent, float first_height, float spacing = 0.0f) {
    return {
        {parent.x, parent.y, parent.width, first_height},
        {parent.x, parent.y + first_height + spacing,
         parent.width, parent.height - first_height - spacing},
    };
}

// スクロールコンテナ内の実際の表示領域を返す。header_height は上部固定領域、bottom_padding は下余白。
constexpr Rectangle scroll_view(Rectangle container, float header_height = 0.0f, float bottom_padding = 0.0f) {
    return {
        container.x,
        container.y + header_height,
        container.width,
        container.height - header_height - bottom_padding,
    };
}

// 垂直スクロールバーのサム位置と最大スクロール量を計算する。
inline scroll_metrics vertical_scroll_metrics(Rectangle track_rect, float content_height, float scroll_offset,
                                              float min_thumb_height = 36.0f) {
    const float view_height = track_rect.height;
    const float max_scroll = std::max(0.0f, content_height - view_height);
    if (content_height <= view_height || view_height <= 0.0f) {
        return {max_scroll, track_rect};
    }

    const float thumb_height = std::max(min_thumb_height, view_height * (view_height / content_height));
    const float scroll_t = max_scroll > 0.0f ? std::clamp(scroll_offset / max_scroll, 0.0f, 1.0f) : 0.0f;
    return {
        max_scroll,
        {
            track_rect.x,
            track_rect.y + (track_rect.height - thumb_height) * scroll_t,
            track_rect.width,
            thumb_height,
        }
    };
}

// ── スタック ────────────────────────────────────────────

constexpr vertical_layout_cursor vertical_cursor(Rectangle first_row, float spacing) {
    return {
        first_row.x,
        first_row.y,
        first_row.width,
        spacing,
    };
}

constexpr vertical_layout_cursor vertical_cursor(float x, float y, float width, float spacing) {
    return {x, y, width, spacing};
}

constexpr wrapped_layout_cursor wrapped_cursor(Rectangle bounds, float gap, float row_step) {
    return {
        bounds,
        bounds.x,
        bounds.y,
        gap,
        row_step,
    };
}

// 垂直スタック。parent 内に item_height 高の要素を spacing 間隔で上から並べる。
// 幅は parent に合わせる。
inline int vstack(Rectangle parent, float item_height, float spacing,
                  std::span<Rectangle> out) {
    const int count = static_cast<int>(out.size());
    float y = parent.y;
    for (int i = 0; i < count; ++i) {
        out[i] = {parent.x, y, parent.width, item_height};
        y += item_height + spacing;
    }
    return count;
}

// 水平スタック。parent 内に item_width 幅の要素を spacing 間隔で左から並べる。
// 高さは parent に合わせる。
inline int hstack(Rectangle parent, float item_width, float spacing,
                  std::span<Rectangle> out) {
    const int count = static_cast<int>(out.size());
    float x = parent.x;
    for (int i = 0; i < count; ++i) {
        out[i] = {x, parent.y, item_width, parent.height};
        x += item_width + spacing;
    }
    return count;
}

inline float stack_content_width(int item_count, float item_width, float spacing) {
    if (item_count <= 0) {
        return 0.0f;
    }
    return static_cast<float>(item_count) * item_width +
           static_cast<float>(item_count - 1) * spacing;
}

inline int centered_hstack(Rectangle parent,
                           float item_width,
                           float item_height,
                           float spacing,
                           std::span<Rectangle> out) {
    const int count = static_cast<int>(out.size());
    const float total_width = stack_content_width(count, item_width, spacing);
    return hstack({
                      parent.x + (parent.width - total_width) * 0.5f,
                      parent.y + (parent.height - item_height) * 0.5f,
                      total_width,
                      item_height,
                  },
                  item_width,
                  spacing,
                  out);
}

// 水平スタック（可変幅）。item_widths の各幅を spacing 間隔で左から並べる。
// 高さは parent に合わせる。
inline int hstack_widths(Rectangle parent, std::span<const float> item_widths, float spacing,
                         std::span<Rectangle> out) {
    const int count = std::min(static_cast<int>(item_widths.size()), static_cast<int>(out.size()));
    float x = parent.x;
    for (int i = 0; i < count; ++i) {
        out[i] = {x, parent.y, item_widths[i], parent.height};
        x += item_widths[i] + spacing;
    }
    return count;
}

// 垂直スタック（均等分割）。parent の高さを要素数で等分する。
inline int vstack_fill(Rectangle parent, float spacing,
                       std::span<Rectangle> out) {
    const int count = static_cast<int>(out.size());
    if (count <= 0) return 0;
    const float total_spacing = spacing * static_cast<float>(count - 1);
    const float item_height = (parent.height - total_spacing) / static_cast<float>(count);
    return vstack(parent, item_height, spacing, out);
}

// 水平スタック（均等分割）。parent の幅を要素数で等分する。
inline int hstack_fill(Rectangle parent, float spacing,
                       std::span<Rectangle> out) {
    const int count = static_cast<int>(out.size());
    if (count <= 0) return 0;
    const float total_spacing = spacing * static_cast<float>(count - 1);
    const float item_width = (parent.width - total_spacing) / static_cast<float>(count);
    return hstack(parent, item_width, spacing, out);
}

// ── グリッド ────────────────────────────────────────────

// グリッド配置。cols 列で cell_width x cell_height のセルを配置する。
inline int grid(Rectangle parent, int cols,
                float cell_width, float cell_height,
                float h_spacing, float v_spacing,
                std::span<Rectangle> out) {
    const int count = static_cast<int>(out.size());
    for (int i = 0; i < count; ++i) {
        const int col = i % cols;
        const int row = i / cols;
        out[i] = {
            parent.x + static_cast<float>(col) * (cell_width + h_spacing),
            parent.y + static_cast<float>(row) * (cell_height + v_spacing),
            cell_width,
            cell_height,
        };
    }
    return count;
}

}  // namespace ui
