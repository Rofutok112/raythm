#include "title/title_layout.h"

#include "scene_common.h"
#include "ui_draw.h"

namespace title_layout {

namespace {

constexpr float kClosedHeaderWidth = 1290.0f;
constexpr float kClosedHeaderHeight = 273.0f;
constexpr Vector2 kClosedHeaderOffset = {0.0f, -81.0f};
constexpr float kOpenHeaderWidth = 1140.0f;
constexpr float kOpenHeaderHeight = 255.0f;
constexpr Vector2 kOpenHeaderOffset = {108.0f, 126.0f};
constexpr float kPlayHeaderY = 78.0f;
constexpr float kSpectrumHeight = 498.0f;
constexpr float kAccountChipWidth = 396.0f;
constexpr float kAccountChipHeight = 87.0f;
constexpr Vector2 kAccountChipOffset = {-42.0f, 30.0f};
constexpr float kSettingsChipSize = 87.0f;
constexpr float kSettingsChipGap = 18.0f;

}  // namespace

Rectangle screen_rect() {
    return {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
}

Rectangle closed_header_rect() {
    return ui::place(screen_rect(), kClosedHeaderWidth, kClosedHeaderHeight,
                     ui::anchor::center, ui::anchor::center,
                     kClosedHeaderOffset);
}

Rectangle open_header_rect() {
    return ui::place(screen_rect(), kOpenHeaderWidth, kOpenHeaderHeight,
                     ui::anchor::top_left, ui::anchor::top_left,
                     kOpenHeaderOffset);
}

Rectangle play_header_rect() {
    const Rectangle open = open_header_rect();
    return {
        open.x,
        kPlayHeaderY,
        open.width,
        open.height
    };
}

Rectangle spectrum_rect() {
    return ui::place(screen_rect(), static_cast<float>(kScreenWidth), kSpectrumHeight,
                     ui::anchor::bottom_center, ui::anchor::bottom_center,
                     {0.0f, 0.0f});
}

Rectangle settings_chip_rect() {
    const Rectangle account = account_chip_rect();
    return {
        account.x - kSettingsChipGap - kSettingsChipSize,
        account.y,
        kSettingsChipSize,
        kSettingsChipSize
    };
}

Rectangle account_chip_rect() {
    return ui::place(screen_rect(), kAccountChipWidth, kAccountChipHeight,
                     ui::anchor::top_right, ui::anchor::top_right,
                     kAccountChipOffset);
}

}  // namespace title_layout
