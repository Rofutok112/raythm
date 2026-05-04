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

enum class page_id { gameplay, audio, video, network, key_config };

struct page_descriptor {
    const char* navigation_label;
    const char* title;
    const char* subtitle;
};

inline constexpr ui::draw_layer kLayer = ui::draw_layer::base;
inline constexpr int kPageCount = 5;
inline constexpr float kSliderLeftInset = 327.0f;
inline constexpr float kSliderRightInset = 63.0f;
inline constexpr float kSliderTopOffset = 39.0f;
inline constexpr float kArrowButtonSize = 51.0f;
inline constexpr float kSidebarWidth = 384.0f;
inline constexpr float kPanelHeight = 990.0f;
inline constexpr Vector2 kSidebarOffset = {36.0f, 66.0f};
inline constexpr float kContentWidth = 1434.0f;
inline constexpr Vector2 kContentOffset = {450.0f, 66.0f};
inline constexpr float kSidebarItemWidth = 312.0f;
inline constexpr float kSidebarHeaderHeight = 93.0f;
inline constexpr Vector2 kSidebarHeaderOffset = {33.0f, 39.0f};
inline constexpr float kSidebarHintHeight = 36.0f;
inline constexpr Vector2 kSidebarHintOffset = {36.0f, 528.0f};
inline constexpr float kTabRowHeight = 63.0f;
inline constexpr float kTabRowGap = 12.0f;
inline constexpr Vector2 kTabAreaOffset = {0.0f, 228.0f};
inline constexpr float kBackHeight = 63.0f;
inline constexpr Vector2 kBackOffset = {0.0f, -57.0f};
inline constexpr float kContentHeaderWidth = 840.0f;
inline constexpr float kContentHeaderHeight = 90.0f;
inline constexpr Vector2 kContentHeaderOffset = {45.0f, 45.0f};
inline constexpr float kRowWidth = 1335.0f;
inline constexpr float kRowHeight = 72.0f;
inline constexpr float kRowOffsetX = 45.0f;
inline constexpr float kKeyModeHeight = 96.0f;
inline constexpr float kSliderLabelWidth = 300.0f;
inline constexpr float kSliderThickness = 27.0f;
inline constexpr float kRowContentPaddingX = 27.0f;
inline constexpr float kArrowGap = 15.0f;
inline constexpr float kDoubleArrowExtraGap = 45.0f;
inline constexpr float kKeySlotWidth = 840.0f;
inline constexpr float kKeySlotHeight = 72.0f;
inline constexpr float kKeySlotStartY = 321.0f;
inline constexpr float kKeySlotStepY = 93.0f;

inline constexpr std::array<page_descriptor, kPageCount> kPageDescriptors = {{
    {"Gameplay", "Gameplay", "Play feel and lane settings"},
    {"Audio", "Audio", "BGM and sound effect volume"},
    {"Video", "Video", "Display and frame rate settings"},
    {"Network", "Network", "Server environment"},
    {"Key Config", "Key Config", "Per-lane keyboard bindings"},
}};

inline const Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
inline const Rectangle kSidebarRect = ui::place(kScreenRect, kSidebarWidth, kPanelHeight,
                                                ui::anchor::top_left, ui::anchor::top_left, kSidebarOffset);
inline const Rectangle kContentRect = ui::place(kScreenRect, kContentWidth, kPanelHeight,
                                                ui::anchor::top_left, ui::anchor::top_left, kContentOffset);
inline const Rectangle kSidebarHeaderRect = ui::place(kSidebarRect, kSidebarItemWidth, kSidebarHeaderHeight,
                                                      ui::anchor::top_left, ui::anchor::top_left, kSidebarHeaderOffset);
inline const Rectangle kSidebarHintRect = ui::place(kSidebarRect, kSidebarItemWidth, kSidebarHintHeight,
                                                    ui::anchor::top_left, ui::anchor::top_left, kSidebarHintOffset);
inline const Rectangle kTabArea = ui::place(kSidebarRect, kSidebarItemWidth,
                                            static_cast<float>(kPageCount) * kTabRowHeight +
                                                static_cast<float>(kPageCount - 1) * kTabRowGap,
                                            ui::anchor::top_center, ui::anchor::top_center, kTabAreaOffset);
inline const Rectangle kBackRect = ui::place(kSidebarRect, kSidebarItemWidth, kBackHeight,
                                             ui::anchor::bottom_center, ui::anchor::bottom_center, kBackOffset);
inline const Rectangle kContentHeaderRect = ui::place(kContentRect, kContentHeaderWidth, kContentHeaderHeight,
                                                      ui::anchor::top_left, ui::anchor::top_left, kContentHeaderOffset);
inline const std::array<Rectangle, 8> kGeneralRows = {{
    ui::place(kContentRect, kRowWidth, kRowHeight, ui::anchor::top_left, ui::anchor::top_left, Vector2{kRowOffsetX, 174.0f}),
    ui::place(kContentRect, kRowWidth, kRowHeight, ui::anchor::top_left, ui::anchor::top_left, Vector2{kRowOffsetX, 264.0f}),
    ui::place(kContentRect, kRowWidth, kRowHeight, ui::anchor::top_left, ui::anchor::top_left, Vector2{kRowOffsetX, 354.0f}),
    ui::place(kContentRect, kRowWidth, kRowHeight, ui::anchor::top_left, ui::anchor::top_left, Vector2{kRowOffsetX, 504.0f}),
    ui::place(kContentRect, kRowWidth, kRowHeight, ui::anchor::top_left, ui::anchor::top_left, Vector2{kRowOffsetX, 594.0f}),
    ui::place(kContentRect, kRowWidth, kRowHeight, ui::anchor::top_left, ui::anchor::top_left, Vector2{kRowOffsetX, 744.0f}),
    ui::place(kContentRect, kRowWidth, kRowHeight, ui::anchor::top_left, ui::anchor::top_left, Vector2{kRowOffsetX, 834.0f}),
    ui::place(kContentRect, kRowWidth, kRowHeight, ui::anchor::top_left, ui::anchor::top_left, Vector2{kRowOffsetX, 924.0f}),
}};
inline const std::array<Rectangle, 5> kGameplayRows = {{
    kGeneralRows[0],
    kGeneralRows[1],
    kGeneralRows[2],
    ui::place(kContentRect, kRowWidth, kRowHeight, ui::anchor::top_left, ui::anchor::top_left, Vector2{kRowOffsetX, 444.0f}),
    kGeneralRows[4],
}};
inline const Rectangle kKeyModeRect = ui::place(kContentRect, kRowWidth, kKeyModeHeight,
                                                ui::anchor::top_left, ui::anchor::top_left,
                                                Vector2{kRowOffsetX, 189.0f});

inline float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline const page_descriptor& page_descriptor_for(page_id page) {
    return kPageDescriptors[static_cast<std::size_t>(page)];
}

inline Rectangle slider_track_rect(const Rectangle& row_rect) {
    return ui::make_slider_layout(row_rect, kSliderLeftInset, kSliderRightInset, kSliderLabelWidth, kSliderThickness, kSliderTopOffset).track_rect;
}

inline Rectangle arrow_left_rect(const Rectangle& row_rect) {
    const Rectangle content = ui::inset(row_rect, ui::edge_insets::symmetric(0.0f, kRowContentPaddingX));
    const ui::rect_pair columns = ui::split_columns(content, kSliderLabelWidth);
    const Rectangle button_pair_area = ui::place(columns.second, kArrowButtonSize * 2.0f + kArrowGap, kArrowButtonSize,
                                                 ui::anchor::center_right, ui::anchor::center_right);
    return {button_pair_area.x, button_pair_area.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle arrow_right_rect(const Rectangle& row_rect) {
    const Rectangle left = arrow_left_rect(row_rect);
    return {left.x + kArrowButtonSize + kArrowGap, left.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle double_arrow_left_rect(const Rectangle& row_rect) {
    const Rectangle content = ui::inset(row_rect, ui::edge_insets::symmetric(0.0f, kRowContentPaddingX));
    const ui::rect_pair columns = ui::split_columns(content, kSliderLabelWidth);
    const Rectangle button_pair_area = ui::place(columns.second, kArrowButtonSize * 4.0f + kDoubleArrowExtraGap, kArrowButtonSize,
                                                 ui::anchor::center_right, ui::anchor::center_right);
    return {button_pair_area.x, button_pair_area.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle single_arrow_left_rect(const Rectangle& row_rect) {
    const Rectangle left = double_arrow_left_rect(row_rect);
    return {left.x + kArrowButtonSize + kArrowGap, left.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle single_arrow_right_rect(const Rectangle& row_rect) {
    const Rectangle left = single_arrow_left_rect(row_rect);
    return {left.x + kArrowButtonSize + kArrowGap, left.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle double_arrow_right_rect(const Rectangle& row_rect) {
    const Rectangle left = single_arrow_right_rect(row_rect);
    return {left.x + kArrowButtonSize + kArrowGap, left.y, kArrowButtonSize, kArrowButtonSize};
}

inline Rectangle key_slot_rect(int index) {
    return ui::place(kContentRect, kKeySlotWidth, kKeySlotHeight,
                     ui::anchor::top_left, ui::anchor::top_left,
                     Vector2{kRowOffsetX, kKeySlotStartY + static_cast<float>(index) * kKeySlotStepY});
}

inline void build_tab_rects(std::span<Rectangle> out) {
    ui::vstack(kTabArea, kTabRowHeight, kTabRowGap, out);
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
