#include "title/title_layout.h"

#include "scene_common.h"
#include "ui_draw.h"

namespace title_layout {

Rectangle screen_rect() {
    return {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
}

Rectangle closed_header_rect() {
    return ui::place(screen_rect(), 860.0f, 182.0f,
                     ui::anchor::center, ui::anchor::center,
                     {0.0f, -54.0f});
}

Rectangle open_header_rect() {
    return ui::place(screen_rect(), 760.0f, 170.0f,
                     ui::anchor::top_left, ui::anchor::top_left,
                     {72.0f, 84.0f});
}

Rectangle play_header_rect() {
    const Rectangle open = open_header_rect();
    return {
        open.x,
        52.0f,
        open.width,
        open.height
    };
}

Rectangle spectrum_rect() {
    return ui::place(screen_rect(), static_cast<float>(kScreenWidth), 332.0f,
                     ui::anchor::bottom_center, ui::anchor::bottom_center,
                     {0.0f, 0.0f});
}

Rectangle account_chip_rect() {
    return ui::place(screen_rect(), 264.0f, 58.0f,
                     ui::anchor::top_right, ui::anchor::top_right,
                     {-28.0f, 20.0f});
}

}  // namespace title_layout
