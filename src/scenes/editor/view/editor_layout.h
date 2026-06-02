#pragma once

#include "scene_common.h"
#include "ui_layout.h"

namespace editor::layout {

inline constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
inline constexpr Rectangle kHeaderRect = ui::place(kScreenRect, 1830.0f, 72.0f,
                                                   ui::anchor::top_center, ui::anchor::top_center,
                                                   Vector2{0.0f, 18.0f});
inline constexpr Rectangle kLeftPanelRect = ui::place(kScreenRect, 300.0f, 930.0f,
                                                      ui::anchor::top_left, ui::anchor::top_left,
                                                      Vector2{30.0f, 108.0f});
inline constexpr Rectangle kTimelineRect = ui::place(kScreenRect, 1536.0f, 930.0f,
                                                     ui::anchor::top_left, ui::anchor::top_left,
                                                     Vector2{354.0f, 108.0f});
inline constexpr Rectangle kRightPanelRect = ui::place(kScreenRect, 372.0f, 930.0f,
                                                       ui::anchor::top_right, ui::anchor::top_right,
                                                       Vector2{-30.0f, 108.0f});
inline constexpr Rectangle kBackButtonRect = ui::place(kHeaderRect, 51.0f, 51.0f,
                                                       ui::anchor::center_left, ui::anchor::center_left,
                                                       Vector2{24.0f, 0.0f});
inline constexpr Rectangle kSettingsButtonRect = ui::place(kHeaderRect, 51.0f, 51.0f,
                                                           ui::anchor::center_left, ui::anchor::center_left,
                                                           Vector2{84.0f, 0.0f});

inline constexpr Rectangle kEditorTitleRect = ui::place(kHeaderRect, 318.0f, 51.0f,
                                                        ui::anchor::center_left, ui::anchor::center_left,
                                                        Vector2{438.0f, 0.0f});
inline constexpr Rectangle kTransportBarRect = ui::place(kHeaderRect, 486.0f, 51.0f,
                                                         ui::anchor::center, ui::anchor::center,
                                                         Vector2{-48.0f, 0.0f});
inline constexpr Rectangle kLoopStatusRect = ui::place(kHeaderRect, 264.0f, 51.0f,
                                                       ui::anchor::center_right, ui::anchor::center_right,
                                                       Vector2{-777.0f, 0.0f});
inline constexpr Rectangle kHeaderToolsRect = ui::place(kHeaderRect, 252.0f, 51.0f,
                                                        ui::anchor::center_right, ui::anchor::center_right,
                                                        Vector2{-27.0f, 0.0f});
inline constexpr Rectangle kChartOffsetRect = ui::place(kHeaderRect, 282.0f, 51.0f,
                                                        ui::anchor::center_right, ui::anchor::center_right,
                                                        Vector2{-507.0f, 0.0f});
inline constexpr Rectangle kWaveformToggleRect = ui::place(kHeaderRect, 198.0f, 51.0f,
                                                           ui::anchor::center_right, ui::anchor::center_right,
                                                           Vector2{-297.0f, 0.0f});
inline constexpr Rectangle kPlaybackRect = ui::place(kTimelineRect, 348.0f, 51.0f,
                                                     ui::anchor::bottom_left, ui::anchor::bottom_left,
                                                     Vector2{18.0f, -81.0f});

inline constexpr float kHeaderTransportPadding = 10.0f;
inline constexpr float kHeaderTransportButtonSize = 42.0f;
inline constexpr float kHeaderTransportButtonGap = 8.0f;
inline constexpr float kHeaderTransportWidth =
    kHeaderTransportPadding * 2.0f +
    kHeaderTransportButtonSize * 3.0f +
    kHeaderTransportButtonGap * 2.0f;
inline constexpr Rectangle kHeaderTransportRect = {
    kHeaderRect.x + kHeaderRect.width * 0.5f - kHeaderTransportWidth * 0.5f,
    kHeaderRect.y + 11.0f,
    kHeaderTransportWidth,
    50.0f
};
inline constexpr Rectangle kHeaderRestartButtonRect = {
    kHeaderTransportRect.x + kHeaderTransportPadding,
    kHeaderTransportRect.y + 4.0f,
    kHeaderTransportButtonSize,
    kHeaderTransportButtonSize
};
inline constexpr Rectangle kHeaderPlayButtonRect = {
    kHeaderRestartButtonRect.x + kHeaderRestartButtonRect.width + kHeaderTransportButtonGap,
    kHeaderRestartButtonRect.y,
    kHeaderTransportButtonSize,
    kHeaderTransportButtonSize
};
inline constexpr Rectangle kHeaderPlaytestButtonRect = {
    kHeaderPlayButtonRect.x + kHeaderPlayButtonRect.width + kHeaderTransportButtonGap,
    kHeaderPlayButtonRect.y,
    kHeaderTransportButtonSize,
    kHeaderTransportButtonSize
};

inline constexpr float kDropdownItemHeight = 45.0f;
inline constexpr float kDropdownItemSpacing = 6.0f;
inline constexpr Rectangle kSnapDropdownRect = kHeaderToolsRect;

inline constexpr Rectangle kMetadataConfirmRect = ui::place(kScreenRect, 630.0f, 294.0f,
                                                            ui::anchor::center, ui::anchor::center);
inline constexpr Rectangle kUnsavedChangesRect = ui::place(kScreenRect, 684.0f, 315.0f,
                                                           ui::anchor::center, ui::anchor::center);
inline constexpr Rectangle kSaveDialogRect = ui::place(kScreenRect, 780.0f, 336.0f,
                                                       ui::anchor::center, ui::anchor::center);
inline constexpr Rectangle kEditorMetadataModalRect = ui::place(kScreenRect, 720.0f, 430.0f,
                                                                ui::anchor::center, ui::anchor::center);
inline constexpr Rectangle kEditorTimingModalRect = ui::place(kScreenRect, 900.0f, 760.0f,
                                                              ui::anchor::center, ui::anchor::center);

inline Rectangle key_count_confirm_button_rect() {
    return {kMetadataConfirmRect.x + 141.0f, kMetadataConfirmRect.y + 213.0f, 156.0f, 45.0f};
}

inline Rectangle key_count_cancel_button_rect() {
    return {kMetadataConfirmRect.x + 333.0f, kMetadataConfirmRect.y + 213.0f, 156.0f, 45.0f};
}

inline Rectangle unsaved_save_button_rect() {
    return {kUnsavedChangesRect.x + 48.0f, kUnsavedChangesRect.y + 231.0f, 168.0f, 48.0f};
}

inline Rectangle unsaved_discard_button_rect() {
    return {kUnsavedChangesRect.x + 258.0f, kUnsavedChangesRect.y + 231.0f, 168.0f, 48.0f};
}

inline Rectangle unsaved_cancel_button_rect() {
    return {kUnsavedChangesRect.x + 468.0f, kUnsavedChangesRect.y + 231.0f, 168.0f, 48.0f};
}

inline Rectangle save_submit_button_rect() {
    return {kSaveDialogRect.x + 204.0f, kSaveDialogRect.y + 258.0f, 162.0f, 48.0f};
}

inline Rectangle save_cancel_button_rect() {
    return {kSaveDialogRect.x + 414.0f, kSaveDialogRect.y + 258.0f, 162.0f, 48.0f};
}

inline Rectangle snap_dropdown_menu_rect(int item_count) {
    return {
        kSnapDropdownRect.x,
        kSnapDropdownRect.y + kSnapDropdownRect.height + kDropdownItemSpacing,
        kSnapDropdownRect.width,
        18.0f + static_cast<float>(item_count) * kDropdownItemHeight +
            static_cast<float>(item_count - 1) * kDropdownItemSpacing
    };
}

inline Rectangle cursor_hud_rect() {
    return ui::place(kTimelineRect, 510.0f, 51.0f,
                     ui::anchor::bottom_left, ui::anchor::bottom_left,
                     Vector2{18.0f, -18.0f});
}

}  // namespace editor::layout
