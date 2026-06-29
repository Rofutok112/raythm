#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "game_settings.h"
#include "raylib.h"
#include "localization/localization.h"
#include "scene_common.h"
#include "ui_draw.h"
#include "ui_scroll.h"
#include "virtual_screen.h"

namespace settings {

enum class page_id { gameplay, audio, video, system, key_config };

struct page_descriptor {
    localization::text_key navigation_label;
    localization::text_key title;
    localization::text_key subtitle;
};

struct offset_stepper_layout {
    Rectangle label_rect;
    Rectangle value_rect;
    Rectangle double_left_rect;
    Rectangle single_left_rect;
    Rectangle single_right_rect;
    Rectangle double_right_rect;
};

inline constexpr std::array<int, 4> kOffsetStepperDeltas = {-5, -1, 1, 5};
inline constexpr std::array<const char*, 4> kOffsetStepperLabels = {"<<", "<", ">", ">>"};

struct key_slot_layout {
    Rectangle label_rect;
    Rectangle value_rect;
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
inline constexpr float kRowContentPaddingX = 27.0f;
inline constexpr float kArrowGap = 15.0f;
inline constexpr float kKeySlotWidth = 840.0f;
inline constexpr float kKeySlotHeight = 72.0f;
inline constexpr float kKeySlotStartY = 321.0f;
inline constexpr float kKeySlotStepY = 93.0f;
inline constexpr float kGameplayPreviewWidth = 400.0f;
inline constexpr float kGameplayPreviewHeight = 225.0f;

inline constexpr std::array<page_descriptor, kPageCount> kPageDescriptors = {{
    {localization::text_key::gameplay, localization::text_key::gameplay, localization::text_key::gameplay_subtitle},
    {localization::text_key::audio, localization::text_key::audio, localization::text_key::audio_subtitle},
    {localization::text_key::video, localization::text_key::video, localization::text_key::video_subtitle},
    {localization::text_key::system, localization::text_key::system, localization::text_key::system_subtitle},
    {localization::text_key::key_config, localization::text_key::key_config, localization::text_key::key_config_subtitle},
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

inline Rectangle setting_row_rect(float y_offset) {
    return ui::place(kContentRect, kRowWidth, kRowHeight,
                     ui::anchor::top_left, ui::anchor::top_left,
                     Vector2{kRowOffsetX, y_offset});
}

template <std::size_t Count>
inline std::array<Rectangle, Count> build_setting_row_stack(float start_y, float step_y) {
    const Rectangle stack = {
        kContentRect.x + kRowOffsetX,
        kContentRect.y + start_y,
        kRowWidth,
        kContentRect.height - start_y,
    };
    const float row_gap = step_y - kRowHeight;
    std::array<Rectangle, Count> rows{};
    for (std::size_t i = 0; i < Count; ++i) {
        rows[i] = ui::vertical_list_row_rect(stack, static_cast<int>(i), kRowHeight, row_gap, 0.0f);
    }
    return rows;
}

inline const std::array<Rectangle, 8> kGeneralRows = {{
    setting_row_rect(174.0f),
    setting_row_rect(264.0f),
    setting_row_rect(354.0f),
    setting_row_rect(504.0f),
    setting_row_rect(594.0f),
    setting_row_rect(744.0f),
    setting_row_rect(834.0f),
    setting_row_rect(924.0f),
}};
inline const std::array<Rectangle, 6> kGameplayRows = build_setting_row_stack<6>(174.0f, 90.0f);
inline const Rectangle kGameplayPreviewRect = ui::place(kContentRect, kGameplayPreviewWidth, kGameplayPreviewHeight,
                                                        ui::anchor::top_left, ui::anchor::top_left,
                                                        Vector2{kRowOffsetX, 744.0f});
inline const Rectangle kKeyModeRect = ui::place(kContentRect, kRowWidth, kKeyModeHeight,
                                                ui::anchor::top_left, ui::anchor::top_left,
                                                Vector2{kRowOffsetX, 189.0f});

inline float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline const page_descriptor& page_descriptor_for(page_id page) {
    return kPageDescriptors[static_cast<std::size_t>(page)];
}

inline ui::value_selector_options value_selector_options() {
    return {
        .layer = kLayer,
    };
}

inline Rectangle arrow_left_rect(const Rectangle& row_rect) {
    return ui::make_value_selector_layout(row_rect, value_selector_options()).left_button_rect;
}

inline Rectangle arrow_right_rect(const Rectangle& row_rect) {
    return ui::make_value_selector_layout(row_rect, value_selector_options()).right_button_rect;
}

inline offset_stepper_layout global_offset_stepper_layout(const Rectangle& row_rect) {
    constexpr float kOffsetLabelWidth = 200.0f;
    constexpr float kOffsetContentPadding = 18.0f;
    const Rectangle content = ui::inset(row_rect, ui::edge_insets::symmetric(0.0f, kOffsetContentPadding));
    const ui::rect_pair columns = ui::split_columns(content, kOffsetLabelWidth);
    const float button_group_right = row_rect.x + row_rect.width - kRowContentPaddingX;
    const Rectangle button_group_area = {
        columns.second.x,
        columns.second.y,
        std::max(0.0f, button_group_right - columns.second.x),
        columns.second.height
    };
    const Rectangle button_group = ui::place(button_group_area,
                                             kArrowButtonSize * 4.0f + kArrowGap * 3.0f,
                                             kArrowButtonSize,
                                             ui::anchor::center_right,
                                             ui::anchor::center_right);
    Rectangle buttons[4];
    ui::hstack(button_group, kArrowButtonSize, kArrowGap, buttons);
    return {
        columns.first,
        {
            columns.second.x,
            columns.second.y,
            buttons[0].x - columns.second.x - 16.0f,
            columns.second.height
        },
        buttons[0],
        buttons[1],
        buttons[2],
        buttons[3],
    };
}

inline std::array<Rectangle, 4> offset_stepper_button_rects(const offset_stepper_layout& layout) {
    return {layout.double_left_rect,
            layout.single_left_rect,
            layout.single_right_rect,
            layout.double_right_rect};
}

inline Rectangle double_arrow_left_rect(const Rectangle& row_rect) {
    return global_offset_stepper_layout(row_rect).double_left_rect;
}

inline Rectangle single_arrow_left_rect(const Rectangle& row_rect) {
    return global_offset_stepper_layout(row_rect).single_left_rect;
}

inline Rectangle single_arrow_right_rect(const Rectangle& row_rect) {
    return global_offset_stepper_layout(row_rect).single_right_rect;
}

inline Rectangle double_arrow_right_rect(const Rectangle& row_rect) {
    return global_offset_stepper_layout(row_rect).double_right_rect;
}

inline Rectangle key_slot_rect(int index) {
    constexpr float row_gap = kKeySlotStepY - kKeySlotHeight;
    const Rectangle stack = {
        kContentRect.x + kRowOffsetX,
        kContentRect.y + kKeySlotStartY,
        kKeySlotWidth,
        kContentRect.height - kKeySlotStartY,
    };
    return ui::vertical_list_row_rect(stack, index, kKeySlotHeight, row_gap, 0.0f);
}

inline key_slot_layout key_slot_layout_for(const Rectangle& row_rect) {
    constexpr float kKeySlotPadding = 18.0f;
    constexpr float kKeySlotLabelWidth = 160.0f;
    const ui::rect_pair columns = ui::split_columns(ui::inset(row_rect, kKeySlotPadding),
                                                    kKeySlotLabelWidth);
    return {columns.first, columns.second};
}

inline Rectangle key_config_error_rect(int visible_key_count) {
    return ui::place(kContentRect, 560.0f, 28.0f,
                     ui::anchor::top_left, ui::anchor::top_left,
                     {30.0f, 214.0f + static_cast<float>(visible_key_count) * 62.0f + 8.0f});
}

inline void build_tab_rects(std::span<Rectangle> out) {
    ui::vstack(kTabArea, kTabRowHeight, kTabRowGap, out);
}

inline float slider_ratio_from_mouse(const Rectangle& row_rect) {
    return ui::slider_ratio_from_mouse(row_rect, kSliderLeftInset, kSliderRightInset, {
        .layer = kLayer,
        .track_top_offset = kSliderTopOffset,
        .label_width = kSliderLabelWidth,
        .content_padding = kRowContentPaddingX,
    });
}

inline int fps_option_index(int target_fps) {
    constexpr std::array<int, 4> kFrameRateOptions = {120, 144, 240, 360};
    target_fps = sanitize_target_fps(target_fps);
    for (int i = 0; i < static_cast<int>(kFrameRateOptions.size()); ++i) {
        if (kFrameRateOptions[static_cast<std::size_t>(i)] == target_fps) {
            return i;
        }
    }
    return 1;
}

}  // namespace settings
