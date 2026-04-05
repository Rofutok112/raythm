#pragma once

#include "scene_common.h"
#include "ui_layout.h"

namespace editor::layout {

inline constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
inline constexpr Rectangle kHeaderRect = ui::place(kScreenRect, 1220.0f, 48.0f,
                                                   ui::anchor::top_center, ui::anchor::top_center,
                                                   {0.0f, 12.0f});
inline constexpr Rectangle kLeftPanelRect = ui::place(kScreenRect, 248.0f, 620.0f,
                                                      ui::anchor::top_left, ui::anchor::top_left,
                                                      {20.0f, 72.0f});
inline constexpr Rectangle kTimelineRect = ui::place(kScreenRect, 724.0f, 620.0f,
                                                     ui::anchor::top_center, ui::anchor::top_center,
                                                     {0.0f, 72.0f});
inline constexpr Rectangle kRightPanelRect = ui::place(kScreenRect, 248.0f, 620.0f,
                                                       ui::anchor::top_right, ui::anchor::top_right,
                                                       {-20.0f, 72.0f});
inline constexpr Rectangle kBackButtonRect = ui::place(kHeaderRect, 120.0f, 34.0f,
                                                       ui::anchor::center_left, ui::anchor::center_left,
                                                       {16.0f, 0.0f});

inline constexpr Rectangle kHeaderToolsRect = ui::place(kHeaderRect, 168.0f, 34.0f,
                                                        ui::anchor::center_right, ui::anchor::center_right,
                                                        {-18.0f, 0.0f});
inline constexpr Rectangle kChartOffsetRect = ui::place(kHeaderRect, 188.0f, 34.0f,
                                                        ui::anchor::center_right, ui::anchor::center_right,
                                                        {-338.0f, 0.0f});
inline constexpr Rectangle kWaveformToggleRect = ui::place(kHeaderRect, 132.0f, 34.0f,
                                                           ui::anchor::center_right, ui::anchor::center_right,
                                                           {-198.0f, 0.0f});
inline constexpr Rectangle kPlaybackRect = ui::place(kTimelineRect, 232.0f, 34.0f,
                                                     ui::anchor::bottom_left, ui::anchor::bottom_left,
                                                     {12.0f, -54.0f});

inline constexpr float kDropdownItemHeight = 30.0f;
inline constexpr float kDropdownItemSpacing = 4.0f;
inline constexpr Rectangle kSnapDropdownRect = kHeaderToolsRect;

inline constexpr Rectangle kMetadataConfirmRect = ui::place(kScreenRect, 420.0f, 196.0f,
                                                            ui::anchor::center, ui::anchor::center);
inline constexpr Rectangle kUnsavedChangesRect = ui::place(kScreenRect, 456.0f, 210.0f,
                                                           ui::anchor::center, ui::anchor::center);
inline constexpr Rectangle kSaveDialogRect = ui::place(kScreenRect, 520.0f, 224.0f,
                                                       ui::anchor::center, ui::anchor::center);

inline Rectangle key_count_confirm_button_rect() {
    return {kMetadataConfirmRect.x + 94.0f, kMetadataConfirmRect.y + 142.0f, 104.0f, 30.0f};
}

inline Rectangle key_count_cancel_button_rect() {
    return {kMetadataConfirmRect.x + 222.0f, kMetadataConfirmRect.y + 142.0f, 104.0f, 30.0f};
}

inline Rectangle unsaved_save_button_rect() {
    return {kUnsavedChangesRect.x + 32.0f, kUnsavedChangesRect.y + 154.0f, 112.0f, 32.0f};
}

inline Rectangle unsaved_discard_button_rect() {
    return {kUnsavedChangesRect.x + 172.0f, kUnsavedChangesRect.y + 154.0f, 112.0f, 32.0f};
}

inline Rectangle unsaved_cancel_button_rect() {
    return {kUnsavedChangesRect.x + 312.0f, kUnsavedChangesRect.y + 154.0f, 112.0f, 32.0f};
}

inline Rectangle save_submit_button_rect() {
    return {kSaveDialogRect.x + 136.0f, kSaveDialogRect.y + 172.0f, 108.0f, 32.0f};
}

inline Rectangle save_cancel_button_rect() {
    return {kSaveDialogRect.x + 276.0f, kSaveDialogRect.y + 172.0f, 108.0f, 32.0f};
}

inline Rectangle snap_dropdown_menu_rect(int item_count) {
    return {
        kSnapDropdownRect.x,
        kSnapDropdownRect.y + kSnapDropdownRect.height + 4.0f,
        kSnapDropdownRect.width,
        12.0f + static_cast<float>(item_count) * kDropdownItemHeight +
            static_cast<float>(item_count - 1) * kDropdownItemSpacing
    };
}

inline Rectangle cursor_hud_rect() {
    return ui::place(kTimelineRect, 340.0f, 34.0f,
                     ui::anchor::bottom_left, ui::anchor::bottom_left,
                     {12.0f, -12.0f});
}

}  // namespace editor::layout
