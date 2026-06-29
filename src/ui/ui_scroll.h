#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <span>

#include "raylib.h"
#include "ui_hit.h"

namespace ui {

struct index_range {
    int begin = 0;
    int end = 0;

    constexpr bool empty() const {
        return begin >= end;
    }
};

struct scroll_offset_state {
    float content_height = 0.0f;
    float max_scroll = 0.0f;
    float offset = 0.0f;
};

struct vertical_stack_item {
    float y = 0.0f;
    float height = 0.0f;
};

inline float clamp_scroll_offset(float scroll_offset, float max_scroll) {
    return std::clamp(scroll_offset, 0.0f, max_scroll);
}

inline float max_scroll_offset(float content_height, float viewport_height, float trailing_padding = 0.0f) {
    return std::max(0.0f, content_height - viewport_height + trailing_padding);
}

inline float max_scroll_offset(float content_height, Rectangle viewport, float trailing_padding = 0.0f) {
    return max_scroll_offset(content_height, viewport.height, trailing_padding);
}

inline scroll_offset_state scroll_offset_state_for(Rectangle viewport,
                                                   float content_height,
                                                   float scroll_offset,
                                                   float trailing_padding = 0.0f) {
    const float max_scroll = max_scroll_offset(content_height, viewport, trailing_padding);
    return {
        content_height,
        max_scroll,
        clamp_scroll_offset(scroll_offset, max_scroll),
    };
}

inline float vertical_list_content_height(int item_count,
                                          float row_height,
                                          float row_gap = 0.0f,
                                          float min_height = 0.0f) {
    if (item_count <= 0) {
        return min_height;
    }
    const float rows = static_cast<float>(item_count) * row_height;
    const float gaps = static_cast<float>(item_count - 1) * row_gap;
    return std::max(min_height, rows + gaps);
}

inline float vertical_list_content_height(std::size_t item_count,
                                          float row_height,
                                          float row_gap = 0.0f,
                                          float min_height = 0.0f) {
    if (item_count == 0) {
        return min_height;
    }
    const float rows = static_cast<float>(item_count) * row_height;
    const float gaps = static_cast<float>(item_count - 1) * row_gap;
    return std::max(min_height, rows + gaps);
}

inline float vertical_stack_content_height(std::span<const vertical_stack_item> items,
                                           float trailing_padding = 0.0f,
                                           float min_height = 0.0f) {
    float height = 0.0f;
    for (const vertical_stack_item item : items) {
        if (item.height <= 0.0f) {
            continue;
        }
        height = std::max(height, item.y + item.height);
    }
    return std::max(min_height, height + (height > 0.0f ? trailing_padding : 0.0f));
}

template <typename ItemMetrics>
inline float vertical_stack_content_height(int item_count,
                                           ItemMetrics item_metrics,
                                           float trailing_padding = 0.0f,
                                           float min_height = 0.0f) {
    if (item_count <= 0) {
        return min_height;
    }
    float height = 0.0f;
    for (int i = 0; i < item_count; ++i) {
        const vertical_stack_item item = item_metrics(i);
        if (item.height <= 0.0f) {
            continue;
        }
        height = std::max(height, item.y + item.height);
    }
    return std::max(min_height, height + (height > 0.0f ? trailing_padding : 0.0f));
}

inline float vertical_list_content_height_with_trailing_padding(int item_count,
                                                                float row_height,
                                                                float row_gap,
                                                                float trailing_padding,
                                                                float min_height = 0.0f) {
    const float height = vertical_list_content_height(item_count, row_height, row_gap, min_height);
    return item_count > 0 ? height + trailing_padding : height;
}

inline float vertical_list_content_height_with_trailing_padding(std::size_t item_count,
                                                                float row_height,
                                                                float row_gap,
                                                                float trailing_padding,
                                                                float min_height = 0.0f) {
    const float height = vertical_list_content_height(item_count, row_height, row_gap, min_height);
    return item_count > 0 ? height + trailing_padding : height;
}

constexpr int grid_row_count(int item_count, int column_count) {
    return item_count <= 0 || column_count <= 0 ? 0 : (item_count + column_count - 1) / column_count;
}

inline int grid_row_count(std::size_t item_count, int column_count) {
    return column_count <= 0 ? 0 : static_cast<int>((item_count + static_cast<std::size_t>(column_count) - 1) /
                                                    static_cast<std::size_t>(column_count));
}

inline float vertical_grid_content_height(int item_count,
                                          int column_count,
                                          float item_height,
                                          float row_gap = 0.0f,
                                          float min_height = 0.0f) {
    return vertical_list_content_height(grid_row_count(item_count, column_count), item_height, row_gap, min_height);
}

inline float vertical_grid_content_height(std::size_t item_count,
                                          int column_count,
                                          float item_height,
                                          float row_gap = 0.0f,
                                          float min_height = 0.0f) {
    return vertical_list_content_height(grid_row_count(item_count, column_count), item_height, row_gap, min_height);
}

constexpr Rectangle vertical_list_row_rect(Rectangle viewport,
                                           int index,
                                           float row_height,
                                           float row_gap,
                                           float scroll_offset) {
    return {
        viewport.x,
        viewport.y + static_cast<float>(index) * (row_height + row_gap) - scroll_offset,
        viewport.width,
        row_height,
    };
}

constexpr Rectangle vertical_span_rect(Rectangle viewport, float y, float height) {
    return {viewport.x, y, viewport.width, height};
}

constexpr Rectangle vertical_stack_item_rect(Rectangle viewport,
                                             vertical_stack_item item,
                                             float scroll_offset) {
    return {
        viewport.x,
        viewport.y + item.y - scroll_offset,
        viewport.width,
        item.height,
    };
}

constexpr Rectangle vertical_viewport_after_top_inset(Rectangle area, float top_inset) {
    return {
        area.x,
        area.y + top_inset,
        area.width,
        std::max(0.0f, area.height - top_inset),
    };
}

constexpr Rectangle vertical_grid_item_rect(Rectangle viewport,
                                            int index,
                                            int column_count,
                                            float item_width,
                                            float item_height,
                                            float column_gap,
                                            float row_gap,
                                            float scroll_offset) {
    if (column_count <= 0) {
        return {viewport.x, viewport.y - scroll_offset, item_width, item_height};
    }
    const int row = index / column_count;
    const int column = index % column_count;
    return {
        viewport.x + static_cast<float>(column) * (item_width + column_gap),
        viewport.y + static_cast<float>(row) * (item_height + row_gap) - scroll_offset,
        item_width,
        item_height,
    };
}

inline int vertical_list_index_at_y(float y,
                                    Rectangle viewport,
                                    float row_height,
                                    float row_gap,
                                    float scroll_offset) {
    const float pitch = row_height + row_gap;
    if (row_height <= 0.0f || pitch <= 0.0f) {
        return -1;
    }

    const float local_y = y - viewport.y + scroll_offset;
    if (local_y < 0.0f) {
        return -1;
    }

    const int index = static_cast<int>(local_y / pitch);
    const float row_local_y = local_y - static_cast<float>(index) * pitch;
    return row_local_y <= row_height ? index : -1;
}

constexpr bool rect_visible_in_viewport(Rectangle rect, Rectangle viewport, float slack = 0.0f) {
    return rect.y + rect.height >= viewport.y - slack &&
           rect.y <= viewport.y + viewport.height + slack;
}

constexpr bool vertical_list_row_visible(Rectangle viewport,
                                         int index,
                                         float row_height,
                                         float row_gap,
                                         float scroll_offset,
                                         float slack = 0.0f) {
    return rect_visible_in_viewport(
        vertical_list_row_rect(viewport, index, row_height, row_gap, scroll_offset), viewport, slack);
}

inline index_range vertical_list_visible_range(int item_count,
                                               Rectangle row_viewport,
                                               Rectangle clip_viewport,
                                               float row_height,
                                               float row_gap,
                                               float scroll_offset,
                                               float slack = 0.0f) {
    const float pitch = row_height + row_gap;
    if (item_count <= 0 || row_height <= 0.0f || pitch <= 0.0f) {
        return {};
    }

    const float first_visible =
        std::ceil((clip_viewport.y - slack - row_viewport.y + scroll_offset - row_height) / pitch);
    const float last_visible =
        std::floor((clip_viewport.y + clip_viewport.height + slack - row_viewport.y + scroll_offset) / pitch);
    const int begin = std::clamp(static_cast<int>(first_visible), 0, item_count);
    const int end = std::clamp(static_cast<int>(last_visible) + 1, begin, item_count);
    return {begin, end};
}

inline index_range vertical_list_visible_range(int item_count,
                                               Rectangle viewport,
                                               float row_height,
                                               float row_gap,
                                               float scroll_offset,
                                               float slack = 0.0f) {
    return vertical_list_visible_range(item_count, viewport, viewport, row_height, row_gap, scroll_offset, slack);
}

inline index_range vertical_list_visible_range(std::size_t item_count,
                                               Rectangle row_viewport,
                                               Rectangle clip_viewport,
                                               float row_height,
                                               float row_gap,
                                               float scroll_offset,
                                               float slack = 0.0f) {
    const int capped_count = static_cast<int>(std::min<std::size_t>(
        item_count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    return vertical_list_visible_range(capped_count, row_viewport, clip_viewport, row_height, row_gap, scroll_offset, slack);
}

inline index_range vertical_list_visible_range(std::size_t item_count,
                                               Rectangle viewport,
                                               float row_height,
                                               float row_gap,
                                               float scroll_offset,
                                               float slack = 0.0f) {
    const int capped_count = static_cast<int>(std::min<std::size_t>(
        item_count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    return vertical_list_visible_range(capped_count, viewport, row_height, row_gap, scroll_offset, slack);
}

template <typename ItemMetrics>
inline index_range vertical_stack_visible_range(int item_count,
                                                ItemMetrics item_metrics,
                                                Rectangle viewport,
                                                float scroll_offset,
                                                float slack = 0.0f) {
    if (item_count <= 0) {
        return {};
    }
    int begin = item_count;
    int end = begin;
    const float visible_top = scroll_offset - slack;
    const float visible_bottom = scroll_offset + viewport.height + slack;
    for (int i = 0; i < item_count; ++i) {
        const vertical_stack_item item = item_metrics(i);
        if (item.height <= 0.0f) {
            continue;
        }
        const float item_top = item.y;
        const float item_bottom = item.y + item.height;
        if (item_bottom < visible_top) {
            continue;
        }
        if (item_top > visible_bottom) {
            break;
        }
        if (begin == item_count) {
            begin = i;
        }
        end = i + 1;
    }
    if (begin == item_count) {
        return {};
    }
    return {begin, end};
}

inline index_range vertical_stack_visible_range(std::span<const vertical_stack_item> items,
                                                Rectangle viewport,
                                                float scroll_offset,
                                                float slack = 0.0f) {
    const int capped_count = static_cast<int>(std::min<std::size_t>(
        items.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
    return vertical_stack_visible_range(
        capped_count,
        [&](int index) {
            return items[static_cast<std::size_t>(index)];
        },
        viewport,
        scroll_offset,
        slack);
}

inline index_range vertical_grid_visible_index_range(int item_count,
                                                     Rectangle viewport,
                                                     int column_count,
                                                     float item_height,
                                                     float row_gap,
                                                     float scroll_offset,
                                                     float slack = 0.0f) {
    if (item_count <= 0 || column_count <= 0) {
        return {};
    }

    const index_range rows = vertical_list_visible_range(
        grid_row_count(item_count, column_count), viewport, item_height, row_gap, scroll_offset, slack);
    return {
        std::clamp(rows.begin * column_count, 0, item_count),
        std::clamp(rows.end * column_count, 0, item_count),
    };
}

inline index_range vertical_grid_visible_index_range(std::size_t item_count,
                                                     Rectangle viewport,
                                                     int column_count,
                                                     float item_height,
                                                     float row_gap,
                                                     float scroll_offset,
                                                     float slack = 0.0f) {
    const int capped_count = static_cast<int>(std::min<std::size_t>(
        item_count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    return vertical_grid_visible_index_range(capped_count, viewport, column_count, item_height, row_gap, scroll_offset, slack);
}

inline index_range vertical_list_fitting_range(int item_count,
                                               Rectangle row_viewport,
                                               Rectangle fit_viewport,
                                               float row_height,
                                               float row_gap,
                                               float scroll_offset) {
    const float pitch = row_height + row_gap;
    if (item_count <= 0 || row_height <= 0.0f || pitch <= 0.0f) {
        return {};
    }

    const float first_fit = std::ceil((fit_viewport.y - row_viewport.y + scroll_offset) / pitch);
    const float last_fit =
        std::floor((fit_viewport.y + fit_viewport.height - row_height - row_viewport.y + scroll_offset) / pitch);
    const int begin = std::clamp(static_cast<int>(first_fit), 0, item_count);
    const int end = std::clamp(static_cast<int>(last_fit) + 1, begin, item_count);
    return {begin, end};
}

inline index_range vertical_list_fitting_range(int item_count,
                                               Rectangle viewport,
                                               float row_height,
                                               float row_gap,
                                               float scroll_offset) {
    return vertical_list_fitting_range(item_count, viewport, viewport, row_height, row_gap, scroll_offset);
}

inline index_range vertical_list_fitting_range(std::size_t item_count,
                                               Rectangle row_viewport,
                                               Rectangle fit_viewport,
                                               float row_height,
                                               float row_gap,
                                               float scroll_offset) {
    const int capped_count = static_cast<int>(std::min<std::size_t>(
        item_count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    return vertical_list_fitting_range(capped_count, row_viewport, fit_viewport, row_height, row_gap, scroll_offset);
}

inline index_range vertical_list_fitting_range(std::size_t item_count,
                                               Rectangle viewport,
                                               float row_height,
                                               float row_gap,
                                               float scroll_offset) {
    const int capped_count = static_cast<int>(std::min<std::size_t>(
        item_count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    return vertical_list_fitting_range(capped_count, viewport, row_height, row_gap, scroll_offset);
}

constexpr bool y_visible_in_viewport(float y, Rectangle viewport, float slack = 0.0f) {
    return y >= viewport.y - slack && y <= viewport.y + viewport.height + slack;
}

constexpr bool vertical_span_fits_in_viewport(float y, float height, Rectangle viewport) {
    return y >= viewport.y && y + height <= viewport.y + viewport.height;
}

constexpr bool vertical_span_rect_fits_in_viewport(Rectangle viewport, float y, float height) {
    const Rectangle rect = vertical_span_rect(viewport, y, height);
    return vertical_span_fits_in_viewport(rect.y, rect.height, viewport);
}

constexpr bool rect_fits_vertically_in_viewport(Rectangle rect, Rectangle viewport) {
    return vertical_span_fits_in_viewport(rect.y, rect.height, viewport);
}

constexpr bool vertical_list_row_fits(Rectangle viewport,
                                      int index,
                                      float row_height,
                                      float row_gap,
                                      float scroll_offset) {
    return rect_fits_vertically_in_viewport(
        vertical_list_row_rect(viewport, index, row_height, row_gap, scroll_offset), viewport);
}

inline float wheel_scrolled_offset(Rectangle viewport,
                                   Vector2 point,
                                   float wheel,
                                   float scroll_offset,
                                   float max_scroll,
                                   float step) {
    const float clamped = clamp_scroll_offset(scroll_offset, max_scroll);
    if (wheel == 0.0f || !contains_point(viewport, point)) {
        return clamped;
    }
    return clamp_scroll_offset(clamped - wheel * step, max_scroll);
}

inline scroll_offset_state wheel_scrolled_offset_state(Rectangle viewport,
                                                       Vector2 point,
                                                       float wheel,
                                                       float content_height,
                                                       float scroll_offset,
                                                       float step,
                                                       float trailing_padding = 0.0f) {
    scroll_offset_state state = scroll_offset_state_for(viewport, content_height, scroll_offset, trailing_padding);
    state.offset = wheel_scrolled_offset(viewport, point, wheel, state.offset, state.max_scroll, step);
    return state;
}

inline float wheel_scrolled_target(float scroll_target, float wheel, float step) {
    return wheel == 0.0f ? scroll_target : scroll_target - wheel * step;
}

inline float wheel_scrolled_target(Rectangle viewport,
                                   Vector2 point,
                                   float wheel,
                                   float scroll_target,
                                   float step) {
    if (wheel == 0.0f || !contains_point(viewport, point)) {
        return scroll_target;
    }
    return wheel_scrolled_target(scroll_target, wheel, step);
}

inline float scroll_offset_with_item_visible(float scroll_offset,
                                             float item_top,
                                             float item_bottom,
                                             float viewport_height,
                                             float max_scroll,
                                             float padding = 0.0f) {
    float next = clamp_scroll_offset(scroll_offset, max_scroll);
    const float visible_top = item_top - padding;
    const float visible_bottom = item_bottom + padding;
    if (visible_top < next) {
        next = visible_top;
    } else if (visible_bottom > next + viewport_height) {
        next = visible_bottom - viewport_height;
    }
    return clamp_scroll_offset(next, max_scroll);
}

inline float vertical_list_scroll_offset_with_index_visible(float scroll_offset,
                                                           int index,
                                                           int item_count,
                                                           float viewport_height,
                                                           float row_height,
                                                           float row_gap,
                                                           float padding = 0.0f) {
    const float pitch = row_height + row_gap;
    if (index < 0 || index >= item_count || row_height <= 0.0f || pitch <= 0.0f || viewport_height <= 0.0f) {
        return scroll_offset;
    }
    const float content_height = vertical_list_content_height(item_count, row_height, row_gap);
    const float max_scroll = max_scroll_offset(content_height, viewport_height);
    const float item_top = static_cast<float>(index) * pitch;
    return scroll_offset_with_item_visible(
        scroll_offset, item_top, item_top + row_height, viewport_height, max_scroll, padding);
}

inline float vertical_list_scroll_offset_with_index_visible(float scroll_offset,
                                                           int index,
                                                           std::size_t item_count,
                                                           float viewport_height,
                                                           float row_height,
                                                           float row_gap,
                                                           float padding = 0.0f) {
    const int capped_count = static_cast<int>(std::min<std::size_t>(
        item_count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    return vertical_list_scroll_offset_with_index_visible(
        scroll_offset, index, capped_count, viewport_height, row_height, row_gap, padding);
}

}  // namespace ui
