#pragma once

#include <algorithm>

#include "scene_common.h"
#include "ui_draw.h"
#include "ui_layout.h"

namespace song_select::layout {

inline constexpr ui::draw_layer kSceneLayer = ui::draw_layer::base;
inline constexpr float kRowHeight = 60.0f;
inline constexpr float kScrollWheelStep = 80.0f;
inline constexpr float kScrollLerpSpeed = 12.0f;
inline constexpr float kOffsetAdjustButtonSize = 34.0f;
inline constexpr float kAutoApplyButtonWidth = 72.0f;
inline constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
inline constexpr Rectangle kSettingsButtonRect = ui::place(kScreenRect, 162.0f, 30.0f,
                                                           ui::anchor::top_right, ui::anchor::top_right,
                                                           {-24.0f, 4.0f});
inline constexpr Rectangle kSongListRect = ui::place(kScreenRect, 466.0f, 660.0f,
                                                     ui::anchor::top_right, ui::anchor::top_right,
                                                     {-24.0f, 44.0f});
inline constexpr Rectangle kLeftPanelRect = ui::place(kScreenRect, 750.0f, 660.0f,
                                                      ui::anchor::top_left, ui::anchor::top_left,
                                                      {24.0f, 44.0f});
inline constexpr Rectangle kJacketRect = ui::place(kLeftPanelRect, 320.0f, 320.0f,
                                                   ui::anchor::top_left, ui::anchor::top_left,
                                                   {20.0f, 24.0f});
inline constexpr float kDetailColumnX = kJacketRect.x + kJacketRect.width + 20.0f;
inline constexpr float kDetailColumnWidth = kLeftPanelRect.x + kLeftPanelRect.width - kDetailColumnX - 16.0f;
inline constexpr Rectangle kLocalOffsetLabelRect = {kDetailColumnX, kJacketRect.y + 236.0f, kDetailColumnWidth, 22.0f};
inline constexpr Rectangle kLocalOffsetControlsRect = {kDetailColumnX, kJacketRect.y + 262.0f, kDetailColumnWidth, 34.0f};
inline constexpr Rectangle kSceneTitleRect = ui::place(kScreenRect, 360.0f, 30.0f,
                                                       ui::anchor::top_left, ui::anchor::top_left,
                                                       {30.0f, 12.0f});
inline constexpr Rectangle kSongListTitleRect = ui::place(kSongListRect, 180.0f, 28.0f,
                                                          ui::anchor::top_left, ui::anchor::top_left,
                                                          {20.0f, 10.0f});
inline constexpr Rectangle kSongListNewSongButtonRect = ui::place(kSongListRect, 124.0f, 30.0f,
                                                                  ui::anchor::top_right, ui::anchor::top_right,
                                                                  {-16.0f, 10.0f});
inline constexpr Rectangle kSongListImportSongButtonRect = ui::place(kSongListNewSongButtonRect, 152.0f, 30.0f,
                                                                     ui::anchor::top_left, ui::anchor::top_right,
                                                                     {-10.0f, 0.0f});
inline constexpr float kSongListHeaderHeight = 48.0f;
inline constexpr float kSongListBottomPadding = 12.0f;
inline constexpr Rectangle kSongListViewRect = ui::scroll_view(kSongListRect, kSongListHeaderHeight, kSongListBottomPadding);
inline constexpr Rectangle kSongListScrollbarTrackRect = ui::place(kSongListViewRect, 6.0f, kSongListViewRect.height,
                                                                   ui::anchor::top_right, ui::anchor::top_right,
                                                                   {-8.0f, 0.0f});
inline constexpr ui::draw_layer kContextMenuLayer = ui::draw_layer::overlay;
inline constexpr ui::draw_layer kModalLayer = ui::draw_layer::modal;
inline constexpr float kContextMenuWidth = 220.0f;
inline constexpr float kContextMenuItemHeight = 30.0f;
inline constexpr float kContextMenuItemSpacing = 4.0f;
inline constexpr Rectangle kConfirmDialogRect = ui::place(kScreenRect, 480.0f, 208.0f,
                                                          ui::anchor::center, ui::anchor::center);

inline Rectangle make_context_menu_rect(Vector2 anchor, int item_count) {
    const float height = 12.0f + static_cast<float>(item_count) * kContextMenuItemHeight +
                         static_cast<float>(std::max(0, item_count - 1)) * kContextMenuItemSpacing;
    Rectangle rect = {anchor.x, anchor.y, kContextMenuWidth, height};
    rect.x = std::clamp(rect.x, 12.0f, kScreenRect.width - rect.width - 12.0f);
    rect.y = std::clamp(rect.y, 12.0f, kScreenRect.height - rect.height - 12.0f);
    return rect;
}

inline Rectangle local_offset_double_left_rect() {
    const Rectangle area = {
        kLocalOffsetControlsRect.x + 104.0f,
        kLocalOffsetControlsRect.y,
        kOffsetAdjustButtonSize * 4.0f + 10.0f * 3.0f,
        kOffsetAdjustButtonSize
    };
    return {area.x, area.y, kOffsetAdjustButtonSize, kOffsetAdjustButtonSize};
}

inline Rectangle local_offset_left_rect() {
    const Rectangle left = local_offset_double_left_rect();
    return {left.x + left.width + 10.0f, left.y, left.width, left.height};
}

inline Rectangle local_offset_right_rect() {
    const Rectangle left = local_offset_left_rect();
    return {left.x + left.width + 10.0f, left.y, left.width, left.height};
}

inline Rectangle local_offset_double_right_rect() {
    const Rectangle left = local_offset_right_rect();
    return {left.x + left.width + 10.0f, left.y, left.width, left.height};
}

inline Rectangle auto_apply_button_rect() {
    return {
        kLocalOffsetControlsRect.x + kLocalOffsetControlsRect.width - kAutoApplyButtonWidth - 16.0f,
        kLocalOffsetControlsRect.y,
        kAutoApplyButtonWidth,
        kOffsetAdjustButtonSize
    };
}

}  // namespace song_select::layout
