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
constexpr Rectangle kDetailLeftRect = {32.0f, 174.0f, 716.0f, 830.0f};
constexpr Rectangle kDetailRightRect = {780.0f, 174.0f, 1108.0f, 830.0f};
constexpr Rectangle kHeroJacketRect = {72.0f, 250.0f, 296.0f, 296.0f};
constexpr Rectangle kPreviewBarRect = {220.0f, 670.0f, 320.0f, 12.0f};
constexpr Rectangle kPreviewPlayRect = {72.0f, 646.0f, 106.0f, 54.0f};
constexpr Rectangle kPrimaryActionRect = {72.0f, 910.0f, 410.0f, 58.0f};
constexpr float kDetailPanelInset = 24.0f;
constexpr Rectangle kChartListRect = {
    kDetailRightRect.x + kDetailPanelInset,
    kDetailRightRect.y + 320.0f,
    kDetailRightRect.width - kDetailPanelInset * 2.0f,
    416.0f,
};
constexpr Rectangle kFallbackOriginRect = {1140.0f, 564.0f, 240.0f, 90.0f};
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
    return {
        detail_left_rect.x + 40.0f,
        detail_left_rect.y + 76.0f,
        width,
        height,
    };
}

Rectangle left_pane_preview_bar_rect(Rectangle detail_left_rect) {
    return {
        detail_left_rect.x + 188.0f,
        detail_left_rect.y + 496.0f,
        320.0f,
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

Rectangle sidebar_search_rect(Rectangle sidebar) {
    constexpr float kSearchInsetX = 14.0f;
    constexpr float kSearchTop = 22.0f;
    constexpr float kSearchHeight = 54.0f;
    return {
        sidebar.x + kSearchInsetX,
        sidebar.y + kSearchTop,
        sidebar.width - kSearchInsetX * 2.0f,
        kSearchHeight,
    };
}

}  // namespace

layout make_layout(float anim_t, Rectangle origin_rect) {
    const float t = tween::ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);
    const Rectangle target_search = sidebar_search_rect(kSidebarRect);

    const Rectangle seed_back = centered_scaled_rect(origin, kBackRect, 0.9f, kSeedBackOffset);
    const Rectangle seed_official_tab = centered_scaled_rect(origin, kOfficialTabRect, 0.84f, kSeedOfficialTabOffset);
    const Rectangle seed_community_tab = centered_scaled_rect(origin, kCommunityTabRect, 0.84f, kSeedCommunityTabOffset);
    const Rectangle seed_owned_tab = centered_scaled_rect(origin, kOwnedTabRect, 0.84f, kSeedOwnedTabOffset);
    const Rectangle seed_search = centered_scaled_rect(origin, target_search, 0.86f, kSeedSearchOffset);
    const Rectangle seed_content = centered_scaled_rect(origin, kContentRect, 0.84f, kSeedContentOffset);
    const Rectangle seed_detail_left = centered_scaled_rect(origin, kDetailLeftRect, 0.78f, kSeedDetailLeftOffset);
    const Rectangle seed_detail_right = centered_scaled_rect(origin, kDetailRightRect, 0.8f, kSeedDetailRightOffset);
    const Rectangle seed_chart_list = centered_scaled_rect(origin, kChartListRect, 0.84f, kSeedChartListOffset);
    const Rectangle current_content = tween::lerp(seed_content, kContentRect, t);
    const Rectangle current_sidebar = tween::lerp(seed_content, kSidebarRect, t);
    const Rectangle current_detail_left = tween::lerp(seed_detail_left, kDetailLeftRect, t);
    const Rectangle current_detail_right = tween::lerp(seed_detail_right, kDetailRightRect, t);
    const Rectangle current_jacket =
        centered_left_pane_jacket_rect(current_detail_left, tween::lerp(0.82f, 1.0f, t));
    const Rectangle current_preview_bar = left_pane_preview_bar_rect(current_detail_left);
    const Rectangle current_preview_play = {
        current_detail_left.x + 40.0f,
        current_detail_left.y + 472.0f,
        kPreviewPlayRect.width,
        kPreviewPlayRect.height,
    };
    const Rectangle current_primary = {
        current_detail_left.x + 40.0f,
        current_detail_left.y + current_detail_left.height - 94.0f,
        410.0f,
        kPrimaryActionRect.height,
    };

    return {
        tween::lerp(seed_back, kBackRect, t),
        tween::lerp(seed_official_tab, kOfficialTabRect, t),
        tween::lerp(seed_community_tab, kCommunityTabRect, t),
        tween::lerp(seed_owned_tab, kOwnedTabRect, t),
        tween::lerp(seed_search, sidebar_search_rect(current_sidebar), t),
        current_content,
        current_sidebar,
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
