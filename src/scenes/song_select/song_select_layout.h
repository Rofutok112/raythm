#pragma once

#include <algorithm>

#include "scene_common.h"
#include "ui_draw.h"
#include "ui_layout.h"

namespace song_select::layout {

inline constexpr ui::draw_layer kSceneLayer = ui::draw_layer::base;
inline constexpr float kRowHeight = 90.0f;
inline constexpr float kScrollWheelStep = 120.0f;
inline constexpr float kScrollLerpSpeed = 12.0f;
inline constexpr float kOffsetAdjustButtonSize = 51.0f;
inline constexpr float kAutoApplyButtonWidth = 108.0f;
inline constexpr float kButtonGap = 15.0f;
inline constexpr float kPanelEdgeMargin = 18.0f;
inline constexpr float kContextMenuTopPadding = 18.0f;
inline constexpr float kRankingDropdownGap = 9.0f;
inline constexpr float kRankingDropdownMenuHeight = 114.0f;
inline constexpr float kDetailColumnGap = 30.0f;
inline constexpr float kDetailRightPadding = 24.0f;
inline constexpr float kLocalOffsetLabelOffsetY = 354.0f;
inline constexpr float kLocalOffsetLabelHeight = 33.0f;
inline constexpr float kLocalOffsetControlsOffsetY = 393.0f;
inline constexpr float kLocalOffsetControlsHeight = 51.0f;
inline constexpr float kLocalOffsetButtonsOffsetX = 156.0f;
inline constexpr float kAutoApplyRightInset = 24.0f;
inline constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
inline constexpr Rectangle kSettingsButtonRect = ui::place(kScreenRect, 243.0f, 45.0f,
                                                           ui::anchor::top_right, ui::anchor::top_right,
                                                           {-36.0f, 6.0f});
inline constexpr Rectangle kLoginButtonRect = ui::place(kSettingsButtonRect, 180.0f, 45.0f,
                                                        ui::anchor::top_left, ui::anchor::top_right,
                                                        {-15.0f, 0.0f});
inline constexpr Rectangle kSongListRect = ui::place(kScreenRect, 699.0f, 990.0f,
                                                     ui::anchor::top_right, ui::anchor::top_right,
                                                     {-36.0f, 66.0f});
inline constexpr Rectangle kLeftPanelRect = ui::place(kScreenRect, 1125.0f, 990.0f,
                                                      ui::anchor::top_left, ui::anchor::top_left,
                                                      {36.0f, 66.0f});
inline constexpr Rectangle kJacketRect = ui::place(kLeftPanelRect, 480.0f, 480.0f,
                                                   ui::anchor::top_left, ui::anchor::top_left,
                                                   {30.0f, 36.0f});
inline constexpr Rectangle kRankingPanelRect = ui::place(kLeftPanelRect, kLeftPanelRect.width - 60.0f, 408.0f,
                                                          ui::anchor::bottom_center, ui::anchor::bottom_center,
                                                          {0.0f, -27.0f});
inline constexpr Rectangle kRankingTitleRect = ui::place(kRankingPanelRect, 330.0f, 42.0f,
                                                         ui::anchor::top_left, ui::anchor::top_left,
                                                         {30.0f, 15.0f});
inline constexpr Rectangle kRankingSourceDropdownRect = ui::place(kRankingPanelRect, 174.0f, 42.0f,
                                                                  ui::anchor::top_right, ui::anchor::top_right,
                                                                  {-24.0f, 15.0f});
inline constexpr float kRankingHeaderHeight = 72.0f;
inline constexpr float kRankingBottomPadding = 18.0f;
inline constexpr Rectangle kRankingListRect = ui::scroll_view(kRankingPanelRect, kRankingHeaderHeight, kRankingBottomPadding);
inline constexpr Rectangle kRankingScrollbarTrackRect = ui::place(kRankingListRect, 9.0f, kRankingListRect.height,
                                                                  ui::anchor::top_right, ui::anchor::top_right,
                                                                  {-12.0f, 0.0f});
inline constexpr float kRankingRowHeight = 81.0f;
inline constexpr float kDetailColumnX = kJacketRect.x + kJacketRect.width + kDetailColumnGap;
inline constexpr float kDetailColumnWidth = kLeftPanelRect.x + kLeftPanelRect.width - kDetailColumnX - kDetailRightPadding;
inline constexpr Rectangle kLocalOffsetLabelRect = {
    kDetailColumnX, kJacketRect.y + kLocalOffsetLabelOffsetY, kDetailColumnWidth, kLocalOffsetLabelHeight
};
inline constexpr Rectangle kLocalOffsetControlsRect = {
    kDetailColumnX, kJacketRect.y + kLocalOffsetControlsOffsetY, kDetailColumnWidth, kLocalOffsetControlsHeight
};
inline constexpr Rectangle kSceneTitleRect = ui::place(kScreenRect, 540.0f, 45.0f,
                                                       ui::anchor::top_left, ui::anchor::top_left,
                                                       {45.0f, 18.0f});
inline constexpr Rectangle kSongListTitleRect = ui::place(kSongListRect, 270.0f, 42.0f,
                                                          ui::anchor::top_left, ui::anchor::top_left,
                                                          {30.0f, 15.0f});
inline constexpr Rectangle kSongListNewSongButtonRect = ui::place(kSongListRect, 186.0f, 45.0f,
                                                                  ui::anchor::top_right, ui::anchor::top_right,
                                                                  {-24.0f, 15.0f});
inline constexpr Rectangle kSongListImportSongButtonRect = ui::place(kSongListNewSongButtonRect, 228.0f, 45.0f,
                                                                     ui::anchor::top_left, ui::anchor::top_right,
                                                                     {-15.0f, 0.0f});
inline constexpr float kSongListHeaderHeight = 72.0f;
inline constexpr float kSongListBottomPadding = 18.0f;
inline constexpr float kSongListTopPadding = 15.0f;
inline constexpr Rectangle kSongListViewRect = ui::scroll_view(kSongListRect, kSongListHeaderHeight, kSongListBottomPadding);
inline constexpr Rectangle kSongListScrollbarTrackRect = ui::place(kSongListViewRect, 9.0f, kSongListViewRect.height,
                                                                   ui::anchor::top_right, ui::anchor::top_right,
                                                                   {-12.0f, 0.0f});
inline constexpr ui::draw_layer kContextMenuLayer = ui::draw_layer::overlay;
inline constexpr ui::draw_layer kModalLayer = ui::draw_layer::modal;
inline constexpr float kContextMenuWidth = 330.0f;
inline constexpr float kContextMenuItemHeight = 45.0f;
inline constexpr float kContextMenuItemSpacing = 6.0f;
inline constexpr Rectangle kConfirmDialogRect = ui::place(kScreenRect, 720.0f, 312.0f,
                                                          ui::anchor::center, ui::anchor::center);
inline constexpr Rectangle kLoginDialogRect = ui::place(kScreenRect, 1140.0f, 840.0f,
                                                        ui::anchor::center, ui::anchor::center,
                                                        {0.0f, 18.0f});

inline Rectangle make_context_menu_rect(Vector2 anchor, int item_count) {
    const float height = kContextMenuTopPadding + static_cast<float>(item_count) * kContextMenuItemHeight +
                         static_cast<float>(std::max(0, item_count - 1)) * kContextMenuItemSpacing;
    Rectangle rect = {anchor.x, anchor.y, kContextMenuWidth, height};
    rect.x = std::clamp(rect.x, kPanelEdgeMargin, kScreenRect.width - rect.width - kPanelEdgeMargin);
    rect.y = std::clamp(rect.y, kPanelEdgeMargin, kScreenRect.height - rect.height - kPanelEdgeMargin);
    return rect;
}

inline Rectangle ranking_source_dropdown_menu_rect() {
    return {
        kRankingSourceDropdownRect.x,
        kRankingSourceDropdownRect.y + kRankingSourceDropdownRect.height + kRankingDropdownGap,
        kRankingSourceDropdownRect.width,
        kRankingDropdownMenuHeight
    };
}

inline Rectangle local_offset_double_left_rect() {
    const Rectangle area = {
        kLocalOffsetControlsRect.x + kLocalOffsetButtonsOffsetX,
        kLocalOffsetControlsRect.y,
        kOffsetAdjustButtonSize * 4.0f + kButtonGap * 3.0f,
        kOffsetAdjustButtonSize
    };
    return {area.x, area.y, kOffsetAdjustButtonSize, kOffsetAdjustButtonSize};
}

inline Rectangle local_offset_left_rect() {
    const Rectangle left = local_offset_double_left_rect();
    return {left.x + left.width + kButtonGap, left.y, left.width, left.height};
}

inline Rectangle local_offset_right_rect() {
    const Rectangle left = local_offset_left_rect();
    return {left.x + left.width + kButtonGap, left.y, left.width, left.height};
}

inline Rectangle local_offset_double_right_rect() {
    const Rectangle left = local_offset_right_rect();
    return {left.x + left.width + kButtonGap, left.y, left.width, left.height};
}

inline Rectangle auto_apply_button_rect() {
    return {
        kLocalOffsetControlsRect.x + kLocalOffsetControlsRect.width - kAutoApplyButtonWidth - kAutoApplyRightInset,
        kLocalOffsetControlsRect.y,
        kAutoApplyButtonWidth,
        kOffsetAdjustButtonSize
    };
}

}  // namespace song_select::layout
