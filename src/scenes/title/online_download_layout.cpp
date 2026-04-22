#include "title/online_download_internal.h"

#include <algorithm>

#include "scene_common.h"
#include "title/title_layout.h"
#include "tween.h"

namespace title_online_view {
namespace {

constexpr Rectangle kBackRect = {48.0f, 38.0f, 98.0f, 38.0f};
constexpr Rectangle kOfficialTabRect = {186.0f, 40.0f, 128.0f, 38.0f};
constexpr Rectangle kCommunityTabRect = {322.0f, 40.0f, 146.0f, 38.0f};
constexpr Rectangle kOwnedTabRect = {476.0f, 40.0f, 108.0f, 38.0f};
constexpr Rectangle kContentRect = {60.0f, 98.0f, 1160.0f, 568.0f};
constexpr Rectangle kDetailLeftRect = {72.0f, 108.0f, 336.0f, 548.0f};
constexpr Rectangle kDetailRightRect = {470.0f, 108.0f, 738.0f, 548.0f};
constexpr Rectangle kHeroJacketRect = {92.0f, 174.0f, 270.0f, 270.0f};
constexpr Rectangle kPreviewBarRect = {92.0f, 505.0f, 270.0f, 8.0f};
constexpr Rectangle kPreviewPlayRect = {92.0f, 529.0f, 126.0f, 40.0f};
constexpr Rectangle kPrimaryActionRect = {92.0f, 577.0f, 126.0f, 40.0f};
constexpr Rectangle kChartListRect = {500.0f, 136.0f, 680.0f, 488.0f};

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
    return {
        static_cast<float>(kScreenWidth) * 0.5f + 120.0f,
        376.0f,
        160.0f,
        60.0f,
    };
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
    const Rectangle account_rect = title_layout::account_chip_rect();
    const float left = kOwnedTabRect.x + kOwnedTabRect.width + 24.0f;
    const float right = account_rect.x - 18.0f;
    return {
        left,
        40.0f,
        std::max(220.0f, right - left),
        38.0f,
    };
}

}  // namespace

layout make_layout(float anim_t, Rectangle origin_rect) {
    const float t = tween::ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);
    const Rectangle search_rect = browse_search_rect();

    const Rectangle seed_back = centered_scaled_rect(origin, kBackRect, 0.9f, {-448.0f, -198.0f});
    const Rectangle seed_official_tab = centered_scaled_rect(origin, kOfficialTabRect, 0.84f, {-276.0f, -194.0f});
    const Rectangle seed_community_tab = centered_scaled_rect(origin, kCommunityTabRect, 0.84f, {-132.0f, -194.0f});
    const Rectangle seed_owned_tab = centered_scaled_rect(origin, kOwnedTabRect, 0.84f, {12.0f, -194.0f});
    const Rectangle seed_search = centered_scaled_rect(origin, search_rect, 0.86f, {318.0f, -192.0f});
    const Rectangle seed_content = centered_scaled_rect(origin, kContentRect, 0.84f, {96.0f, 48.0f});
    const Rectangle seed_detail_left = centered_scaled_rect(origin, kDetailLeftRect, 0.78f, {-238.0f, 66.0f});
    const Rectangle seed_detail_right = centered_scaled_rect(origin, kDetailRightRect, 0.8f, {186.0f, 66.0f});
    const Rectangle seed_chart_list = centered_scaled_rect(origin, kChartListRect, 0.84f, {198.0f, 44.0f});
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
