#include "title/online_download_internal.h"

#include <algorithm>
#include <array>

#include "scene_common.h"
#include "title/title_layout.h"
#include "tween.h"
#include "ui_layout.h"

namespace title_online_view {
namespace {

constexpr Rectangle kBackRect = {39.0f, 983.0f, 330.0f, 58.0f};
constexpr Rectangle kOfficialTabRect = {24.0f, 160.0f, 214.0f, 58.0f};
constexpr Rectangle kCommunityTabRect = {24.0f, 220.0f, 214.0f, 58.0f};
constexpr Rectangle kOwnedTabRect = {24.0f, 280.0f, 214.0f, 58.0f};
constexpr Rectangle kContentRect = {390.0f, 109.0f, 1062.0f, 932.0f};
constexpr Rectangle kSidebarRect = {39.0f, 109.0f, 330.0f, 854.0f};
constexpr Rectangle kPreviewPanelRect = {1470.0f, 109.0f, 408.0f, 932.0f};
constexpr Rectangle kDetailLeftRect = {39.0f, 109.0f, 330.0f, 854.0f};
constexpr Rectangle kDetailRightRect = {390.0f, 109.0f, 820.0f, 932.0f};
constexpr Rectangle kDetailPreviewRect = {1228.0f, 109.0f, 650.0f, 932.0f};
constexpr Rectangle kHeroJacketRect = {1258.0f, 141.0f, 212.0f, 212.0f};
constexpr Rectangle kPreviewBarRect = {1198.0f, 500.0f, 660.0f, 12.0f};
constexpr Rectangle kPreviewPlayRect = {1450.0f, 536.0f, 154.0f, 54.0f};
constexpr Rectangle kPrimaryActionRect = {1538.0f, 934.0f, 320.0f, 58.0f};
constexpr float kDetailPanelInset = 24.0f;
constexpr Rectangle kChartListRect = {
    kDetailRightRect.x + kDetailPanelInset,
    kDetailRightRect.y + 86.0f,
    kDetailRightRect.width - kDetailPanelInset * 2.0f,
    690.0f,
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
constexpr Vector2 kSeedDetailPreviewOffset = {520.0f, 99.0f};
constexpr Vector2 kSeedChartListOffset = {297.0f, 66.0f};

constexpr float kPreviewBarBottomInset = (kContentRect.y + kContentRect.height) - kPreviewBarRect.y;
constexpr float kPreviewButtonBottomInset = (kContentRect.y + kContentRect.height) - kPreviewPlayRect.y;
constexpr float kActionButtonBottomInset = (kContentRect.y + kContentRect.height) - kPrimaryActionRect.y;

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

namespace detail {

namespace {

constexpr float kChartLevelWidth = 220.0f;
constexpr float kChartKeyButtonWidth = 44.0f;
constexpr float kChartKeyButtonStep = 50.0f;
constexpr float kFilterInsetX = 20.0f;
constexpr float kFilterTitleY = 22.0f;
constexpr float kFilterSearchY = 56.0f;
constexpr float kFilterSearchHeight = 42.0f;
constexpr float kFilterSourceLabelY = 102.0f;
constexpr float kFilterStatusLabelY = 240.0f;
constexpr float kFilterLevelLabelY = 336.0f;
constexpr float kFilterKeysLabelY = 448.0f;
constexpr float kFilterTitleHeight = 24.0f;
constexpr float kFilterLabelHeight = 18.0f;
constexpr float kChartHeaderInsetX = 24.0f;
constexpr float kChartHeaderTitleY = 22.0f;
constexpr float kChartHeaderSubtitleY = 46.0f;
constexpr float kChartHeaderTitleWidth = 120.0f;
constexpr float kChartHeaderSubtitleWidth = 180.0f;
constexpr float kChartHeaderCountWidth = 132.0f;
constexpr float kChartHeaderCountRightInset = 28.0f;
constexpr float kChartPlaceholderInsetX = 64.0f;
constexpr float kChartPlaceholderHeight = 72.0f;

}  // namespace

chart_filter_layout chart_filter_layout_for(Rectangle filter_panel) {
    const float content_width = filter_panel.width - kFilterInsetX * 2.0f;
    return {
        .title_label = {
            filter_panel.x + kFilterInsetX,
            filter_panel.y + kFilterTitleY,
            content_width,
            kFilterTitleHeight,
        },
        .search_input = {
            filter_panel.x + kFilterInsetX,
            filter_panel.y + kFilterSearchY,
            content_width,
            kFilterSearchHeight,
        },
        .source_label = {
            filter_panel.x + kFilterInsetX,
            filter_panel.y + kFilterSourceLabelY,
            content_width,
            kFilterLabelHeight,
        },
        .status_label = {
            filter_panel.x + kFilterInsetX,
            filter_panel.y + kFilterStatusLabelY,
            content_width,
            kFilterLabelHeight,
        },
        .level_label = {
            filter_panel.x + kFilterInsetX,
            filter_panel.y + kFilterLevelLabelY,
            content_width,
            kFilterLabelHeight,
        },
        .keys_label = {
            filter_panel.x + kFilterInsetX,
            filter_panel.y + kFilterKeysLabelY,
            content_width,
            kFilterLabelHeight,
        },
    };
}

chart_list_header_layout chart_list_header_layout_for(Rectangle detail_panel) {
    return {
        .title = {
            detail_panel.x + kChartHeaderInsetX,
            detail_panel.y + kChartHeaderTitleY,
            kChartHeaderTitleWidth,
            kFilterTitleHeight,
        },
        .subtitle = {
            detail_panel.x + kChartHeaderInsetX,
            detail_panel.y + kChartHeaderSubtitleY,
            kChartHeaderSubtitleWidth,
            kFilterLabelHeight,
        },
        .count = {
            detail_panel.x + detail_panel.width - kChartHeaderCountRightInset - kChartHeaderCountWidth,
            detail_panel.y + kChartHeaderSubtitleY,
            kChartHeaderCountWidth,
            16.0f,
        },
    };
}

Rectangle chart_empty_placeholder_rect(Rectangle chart_list) {
    return {
        chart_list.x + kChartPlaceholderInsetX,
        chart_list.y + chart_list.height * 0.5f - kChartPlaceholderHeight * 0.5f,
        chart_list.width - kChartPlaceholderInsetX * 2.0f,
        kChartPlaceholderHeight,
    };
}

Rectangle chart_source_button_rect(Rectangle chart_list, int index) {
    const float button_width = (chart_list.width - 52.0f) * 0.5f;
    std::array<Rectangle, 4> buttons{};
    ui::grid({chart_list.x + 20.0f, chart_list.y + 124.0f, chart_list.width - 40.0f, 78.0f},
             2,
             button_width,
             36.0f,
             12.0f,
             6.0f,
             buttons);
    return buttons[static_cast<std::size_t>(index)];
}

Rectangle chart_key_button_rect(Rectangle chart_list, int index) {
    const float group_width = kChartKeyButtonWidth + kChartKeyButtonStep * 4.0f;
    std::array<Rectangle, 5> buttons{};
    ui::hstack({chart_list.x + (chart_list.width - group_width) * 0.5f, chart_list.y + 470.0f, group_width, 30.0f},
               kChartKeyButtonWidth,
               kChartKeyButtonStep - kChartKeyButtonWidth,
               buttons);
    return buttons[static_cast<std::size_t>(index)];
}

Rectangle chart_status_button_rect(Rectangle chart_list, int index) {
    std::array<Rectangle, 3> buttons{};
    ui::hstack_fill({chart_list.x + 20.0f, chart_list.y + 262.0f, chart_list.width - 40.0f, 36.0f},
                    6.0f,
                    buttons);
    return buttons[static_cast<std::size_t>(index)];
}

Rectangle chart_clear_button_rect(Rectangle chart_list) {
    return {
        chart_list.x + 20.0f,
        chart_list.y + chart_list.height - 64.0f,
        chart_list.width - 40.0f,
        42.0f,
    };
}

Rectangle chart_level_slider_rect(Rectangle chart_list) {
    return {
        chart_list.x + (chart_list.width - kChartLevelWidth) * 0.5f,
        chart_list.y + 372.0f,
        kChartLevelWidth,
        24.0f,
    };
}

}  // namespace detail

layout make_layout(float anim_t, Rectangle origin_rect) {
    const float t = tween::ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);
    const Rectangle target_search = sidebar_search_rect(kSidebarRect);

    const Rectangle seed_back = ui::scaled_from_center(origin, kBackRect, 0.9f, kSeedBackOffset);
    const Rectangle seed_official_tab = ui::scaled_from_center(origin, kOfficialTabRect, 0.84f, kSeedOfficialTabOffset);
    const Rectangle seed_community_tab = ui::scaled_from_center(origin, kCommunityTabRect, 0.84f, kSeedCommunityTabOffset);
    const Rectangle seed_owned_tab = ui::scaled_from_center(origin, kOwnedTabRect, 0.84f, kSeedOwnedTabOffset);
    const Rectangle seed_search = ui::scaled_from_center(origin, target_search, 0.86f, kSeedSearchOffset);
    const Rectangle seed_content = ui::scaled_from_center(origin, kContentRect, 0.84f, kSeedContentOffset);
    const Rectangle seed_detail_left = ui::scaled_from_center(origin, kDetailLeftRect, 0.78f, kSeedDetailLeftOffset);
    const Rectangle seed_detail_right = ui::scaled_from_center(origin, kDetailRightRect, 0.8f, kSeedDetailRightOffset);
    const Rectangle seed_detail_preview = ui::scaled_from_center(origin, kDetailPreviewRect, 0.8f, kSeedDetailPreviewOffset);
    const Rectangle seed_chart_list = ui::scaled_from_center(origin, kChartListRect, 0.84f, kSeedChartListOffset);
    const Rectangle current_content = tween::lerp(seed_content, kContentRect, t);
    const Rectangle current_sidebar = tween::lerp(seed_content, kSidebarRect, t);
    const Rectangle current_detail_left = tween::lerp(seed_detail_left, kDetailLeftRect, t);
    const Rectangle current_detail_right = tween::lerp(seed_detail_right, kDetailRightRect, t);
    const Rectangle current_detail_preview = tween::lerp(seed_detail_preview, kDetailPreviewRect, t);
    const Rectangle current_jacket = tween::lerp(ui::scaled_from_center(origin, kHeroJacketRect, 0.82f), kHeroJacketRect, t);
    const Rectangle current_preview_bar = {
        current_detail_preview.x + 30.0f,
        current_detail_preview.y + 326.0f,
        current_detail_preview.width - 60.0f,
        kPreviewBarRect.height,
    };
    const Rectangle current_preview_play = {
        current_detail_preview.x + current_detail_preview.width * 0.5f - kPreviewPlayRect.width * 0.5f,
        current_detail_preview.y + 362.0f,
        kPreviewPlayRect.width,
        kPreviewPlayRect.height,
    };
    const Rectangle current_primary = {
        current_detail_preview.x + current_detail_preview.width - 350.0f,
        current_detail_preview.y + current_detail_preview.height - 70.0f,
        320.0f,
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
        current_detail_preview,
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
