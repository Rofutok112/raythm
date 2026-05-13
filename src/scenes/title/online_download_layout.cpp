#include "title/online_download_internal.h"

#include <algorithm>

#include "scene_common.h"
#include "title/title_layout.h"
#include "tween.h"
#include "ui_layout.h"

namespace title_online_view {
namespace {

constexpr Rectangle kBackRect = {72.0f, 57.0f, 147.0f, 57.0f};
constexpr Rectangle kOfficialTabRect = {24.0f, 160.0f, 214.0f, 58.0f};
constexpr Rectangle kCommunityTabRect = {24.0f, 220.0f, 214.0f, 58.0f};
constexpr Rectangle kOwnedTabRect = {24.0f, 280.0f, 214.0f, 58.0f};
constexpr Rectangle kContentRect = {310.0f, 150.0f, 1140.0f, 880.0f};
constexpr Rectangle kSidebarRect = {50.0f, 150.0f, 240.0f, 880.0f};
constexpr Rectangle kPreviewPanelRect = {1462.0f, 150.0f, 408.0f, 880.0f};
constexpr Rectangle kDetailLeftRect = {332.0f, 174.0f, 408.0f, 830.0f};
constexpr Rectangle kDetailRightRect = {766.0f, 174.0f, 594.0f, 830.0f};
constexpr Rectangle kHeroJacketRect = {358.0f, 220.0f, 356.0f, 356.0f};
constexpr Rectangle kPreviewBarRect = {358.0f, 670.0f, 356.0f, 12.0f};
constexpr Rectangle kPreviewPlayRect = {478.0f, 710.0f, 116.0f, 54.0f};
constexpr Rectangle kPrimaryActionRect = {358.0f, 910.0f, 356.0f, 58.0f};
constexpr Rectangle kChartListRect = {786.0f, 286.0f, 554.0f, 612.0f};
constexpr Rectangle kFallbackOriginRect = {1140.0f, 564.0f, 240.0f, 90.0f};
constexpr float kSearchLeftGap = 36.0f;
constexpr float kSearchRightGap = 27.0f;
constexpr float kSearchY = 82.0f;
constexpr float kSearchHeight = 57.0f;
constexpr Vector2 kSeedBackOffset = {-672.0f, -297.0f};
constexpr Vector2 kSeedOfficialTabOffset = {-414.0f, -291.0f};
constexpr Vector2 kSeedCommunityTabOffset = {-198.0f, -291.0f};
constexpr Vector2 kSeedOwnedTabOffset = {18.0f, -291.0f};
constexpr Vector2 kSeedSearchOffset = {477.0f, -288.0f};
constexpr Vector2 kSeedContentOffset = {144.0f, 72.0f};
constexpr Vector2 kSeedDetailLeftOffset = {-357.0f, 99.0f};
constexpr Vector2 kSeedDetailRightOffset = {279.0f, 99.0f};
constexpr Vector2 kSeedChartListOffset = {297.0f, 66.0f};

constexpr float kPreviewBarBottomInset = (kContentRect.y + kContentRect.height) - kPreviewBarRect.y;
constexpr float kPreviewButtonBottomInset = (kContentRect.y + kContentRect.height) - kPreviewPlayRect.y;
constexpr float kActionButtonBottomInset = (kContentRect.y + kContentRect.height) - kPrimaryActionRect.y;

Rectangle centered_scaled_rect(Rectangle anchor, Rectangle target, float scale, Vector2 offset = {0.0f, 0.0f}) {
    const Vector2 center = {
        anchor.x + anchor.width * 0.5f + offset.x,
        anchor.y + anchor.height * 0.5f + offset.y,
    };
    const float width = target.width * scale;
    const float height = target.height * scale;
    return {
        center.x - width * 0.5f,
        center.y - height * 0.5f,
        width,
        height,
    };
}

Rectangle fallback_origin_rect() {
    return kFallbackOriginRect;
}

Rectangle resolve_origin_rect(Rectangle origin_rect) {
    return origin_rect.width > 0.0f && origin_rect.height > 0.0f ? origin_rect : fallback_origin_rect();
}

Rectangle centered_left_pane_jacket_rect(Rectangle detail_left_rect, float scale) {
    const float width = kHeroJacketRect.width * scale;
    const float height = kHeroJacketRect.height * scale;
    const float inset = std::max(0.0f, (detail_left_rect.width - width) * 0.5f);
    return {
        detail_left_rect.x + inset,
        detail_left_rect.y + inset,
        width,
        height,
    };
}

Rectangle left_pane_preview_bar_rect(Rectangle content_rect, Rectangle hero_jacket_rect) {
    return {
        hero_jacket_rect.x,
        content_rect.y + content_rect.height - kPreviewBarBottomInset,
        hero_jacket_rect.width,
        kPreviewBarRect.height,
    };
}

Rectangle left_pane_full_width_button_rect(Rectangle content_rect,
                                           Rectangle hero_jacket_rect,
                                           float bottom_inset,
                                           float height) {
    return {
        hero_jacket_rect.x,
        content_rect.y + content_rect.height - bottom_inset,
        hero_jacket_rect.width,
        height,
    };
}

Rectangle browse_search_rect() {
    const Rectangle refresh_rect = title_layout::refresh_chip_rect();
    const float left = 360.0f;
    const float right = refresh_rect.x - kSearchRightGap - 30.0f;
    return {
        left,
        kSearchY,
        std::max(0.0f, right - left),
        kSearchHeight,
    };
}

}  // namespace

layout make_layout(float anim_t, Rectangle origin_rect) {
    const float t = tween::ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);
    const Rectangle search_rect = browse_search_rect();

    const Rectangle seed_back = centered_scaled_rect(origin, kBackRect, 0.9f, kSeedBackOffset);
    const Rectangle seed_official_tab = centered_scaled_rect(origin, kOfficialTabRect, 0.84f, kSeedOfficialTabOffset);
    const Rectangle seed_community_tab = centered_scaled_rect(origin, kCommunityTabRect, 0.84f, kSeedCommunityTabOffset);
    const Rectangle seed_owned_tab = centered_scaled_rect(origin, kOwnedTabRect, 0.84f, kSeedOwnedTabOffset);
    const Rectangle seed_search = centered_scaled_rect(origin, search_rect, 0.86f, kSeedSearchOffset);
    const Rectangle seed_content = centered_scaled_rect(origin, kContentRect, 0.84f, kSeedContentOffset);
    const Rectangle seed_detail_left = centered_scaled_rect(origin, kDetailLeftRect, 0.78f, kSeedDetailLeftOffset);
    const Rectangle seed_detail_right = centered_scaled_rect(origin, kDetailRightRect, 0.8f, kSeedDetailRightOffset);
    const Rectangle seed_chart_list = centered_scaled_rect(origin, kChartListRect, 0.84f, kSeedChartListOffset);
    const Rectangle current_content = tween::lerp(seed_content, kContentRect, t);
    const Rectangle current_detail_left = tween::lerp(seed_detail_left, kDetailLeftRect, t);
    const Rectangle current_detail_right = tween::lerp(seed_detail_right, kDetailRightRect, t);
    const Rectangle current_jacket =
        centered_left_pane_jacket_rect(current_detail_left, tween::lerp(0.82f, 1.0f, t));
    const Rectangle current_preview_bar = left_pane_preview_bar_rect(current_content, current_jacket);
    const Rectangle current_preview_play =
        left_pane_full_width_button_rect(current_content, current_jacket,
                                         kPreviewButtonBottomInset, kPreviewPlayRect.height);
    const Rectangle current_primary =
        left_pane_full_width_button_rect(current_content, current_jacket,
                                         kActionButtonBottomInset, kPrimaryActionRect.height);

    return {
        tween::lerp(seed_back, kBackRect, t),
        tween::lerp(seed_official_tab, kOfficialTabRect, t),
        tween::lerp(seed_community_tab, kCommunityTabRect, t),
        tween::lerp(seed_owned_tab, kOwnedTabRect, t),
        tween::lerp(seed_search, search_rect, t),
        current_content,
        tween::lerp(seed_content, kSidebarRect, t),
        tween::lerp(seed_detail_right, kPreviewPanelRect, t),
        detail::song_list_viewport(current_content),
        current_detail_left,
        current_detail_right,
        current_jacket,
        current_preview_bar,
        current_preview_play,
        {},
        tween::lerp(seed_chart_list, kChartListRect, t),
        current_primary,
        {},
    };
}

}  // namespace title_online_view
