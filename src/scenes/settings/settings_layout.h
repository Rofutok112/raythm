#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "raylib.h"
#include "scene_common.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace settings {

enum class page_id { gameplay, audio, video, key_config };

struct page_descriptor {
    const char* navigation_label;
    const char* title;
    const char* subtitle;
};

inline constexpr ui::draw_layer kLayer = ui::draw_layer::base;
inline constexpr int kPageCount = 4;
inline constexpr float kSliderLeftInset = 218.0f;
inline constexpr float kSliderRightInset = 42.0f;
inline constexpr float kSliderTopOffset = 26.0f;
inline constexpr float kArrowButtonSize = 34.0f;

inline constexpr std::array<page_descriptor, kPageCount> kPageDescriptors = {{
    {"Gameplay", "Gameplay", "Play feel and lane settings"},
    {"Audio", "Audio", "BGM and sound effect volume"},
    {"Video", "Video", "Display and frame rate settings"},
    {"Key Config", "Key Config", "Per-lane keyboard bindings"},
}};

inline const Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
inline const Rectangle kSidebarRect = ui::place(kScreenRect, 256.0f, 660.0f,
                                                ui::anchor::top_left, ui::anchor::top_left,
                                                {24.0f, 44.0f});
inline const Rectangle kContentRect = ui::place(kScreenRect, 956.0f, 660.0f,
                                                ui::anchor::top_left, ui::anchor::top_left,
                                                {300.0f, 44.0f});
inline const Rectangle kSidebarHeaderRect = ui::place(kSidebarRect, 208.0f, 62.0f,
                                                      ui::anchor::top_left, ui::anchor::top_left,
                                                      {22.0f, 26.0f});
inline const Rectangle kSidebarHintRect = ui::place(kSidebarRect, 208.0f, 24.0f,
                                                    ui::anchor::top_left, ui::anchor::top_left,
                                                    {24.0f, 352.0f});
inline const Rectangle kTabArea = ui::place(kSidebarRect, 208.0f, 4.0f * 42.0f + 3.0f * 8.0f,
                                            ui::anchor::top_center, ui::anchor::top_center,
                                            {0.0f, 152.0f});
inline const Rectangle kBackRect = ui::place(kSidebarRect, 208.0f, 42.0f,
                                             ui::anchor::bottom_center, ui::anchor::bottom_center,
                                             {0.0f, -38.0f});
inline const Rectangle kContentHeaderRect = ui::place(kContentRect, 560.0f, 60.0f,
                                                      ui::anchor::top_left, ui::anchor::top_left,
                                                      {30.0f, 30.0f});
inline const std::array<Rectangle, 8> kGeneralRows = {{
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 116.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 176.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 236.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 336.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 396.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 496.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 556.0f}),
    ui::place(kContentRect, 890.0f, 48.0f, ui::anchor::top_left, ui::anchor::top_left, {30.0f, 616.0f}),
}};
inline const Rectangle kKeyModeRect = ui::place(kContentRect, 890.0f, 64.0f,
                                                ui::anchor::top_left, ui::anchor::top_left,
                                                {30.0f, 126.0f});

inline float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline const page_descriptor& page_descriptor_for(page_id page) {
    return kPageDescriptors[static_cast<std::size_t>(page)];
}

inline Rectangle slider_track_rect(const Rectangle& row_rect) {
    return ui::make_slider_layout(row_rect, kSliderLeftInset, kSliderRightInset, 200.0f, 18.0f, kSliderTopOffset).track_rect;
}

inline Rectangle arrow_left_rect(const Rectangle& row_rect) {
    const Rectangle content = ui::inset(row_rect, ui::edge_insets::symmetric(0.0f, 18.0f));
    const ui::rect_pair columns = ui::split_columns(content, 200.0f);
    const Rectangle button_pair_area = ui::place(columns.second, kArrowButtonSize * 2.0f + 10.0f, kArrowButtonSize,
                                                 ui::anchor::center_right, ui::anchor::center_right);
    return {button_pair_area.x, button_pair_area.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle arrow_right_rect(const Rectangle& row_rect) {
    const Rectangle left = arrow_left_rect(row_rect);
    return {left.x + kArrowButtonSize + 10.0f, left.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle double_arrow_left_rect(const Rectangle& row_rect) {
    const Rectangle content = ui::inset(row_rect, ui::edge_insets::symmetric(0.0f, 18.0f));
    const ui::rect_pair columns = ui::split_columns(content, 200.0f);
    const Rectangle button_pair_area = ui::place(columns.second, kArrowButtonSize * 4.0f + 30.0f, kArrowButtonSize,
                                                 ui::anchor::center_right, ui::anchor::center_right);
    return {button_pair_area.x, button_pair_area.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle single_arrow_left_rect(const Rectangle& row_rect) {
    const Rectangle left = double_arrow_left_rect(row_rect);
    return {left.x + kArrowButtonSize + 10.0f, left.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle single_arrow_right_rect(const Rectangle& row_rect) {
    const Rectangle left = single_arrow_left_rect(row_rect);
    return {left.x + kArrowButtonSize + 10.0f, left.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle double_arrow_right_rect(const Rectangle& row_rect) {
    const Rectangle left = single_arrow_right_rect(row_rect);
    return {left.x + kArrowButtonSize + 10.0f, left.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle key_slot_rect(int index) {
    return ui::place(kContentRect, 560.0f, 48.0f,
                     ui::anchor::top_left, ui::anchor::top_left,
                     {30.0f, 214.0f + static_cast<float>(index) * 62.0f});
}

inline void build_tab_rects(std::span<Rectangle> out) {
    ui::vstack(kTabArea, 42.0f, 8.0f, out);
}

inline float slider_ratio_from_mouse(const Rectangle& row_rect) {
    const Rectangle track = slider_track_rect(row_rect);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    return clamp01((mouse.x - track.x) / track.width);
}

inline int fps_option_index(int target_fps) {
    constexpr std::array<int, 4> kFrameRateOptions = {120, 144, 240, 0};
    for (int i = 0; i < static_cast<int>(kFrameRateOptions.size()); ++i) {
        if (kFrameRateOptions[static_cast<std::size_t>(i)] == target_fps) {
            return i;
        }
    }
    return 1;
}

}  // namespace settings
