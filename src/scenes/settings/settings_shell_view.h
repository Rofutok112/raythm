#pragma once

#include <optional>

#include "localization/localization.h"
#include "settings/settings_layout.h"
#include "theme.h"
#include "ui_draw.h"

namespace settings {

struct shell_draw_result {
    bool back_requested = false;
    std::optional<page_id> selected_page;
};

inline shell_draw_result draw_tab_buttons(page_id current_page) {
    shell_draw_result result;
    const auto& theme = *g_theme;
    Rectangle tabs[kPageCount];
    build_tab_rects(tabs);
    for (int i = 0; i < kPageCount; ++i) {
        const page_descriptor& descriptor = page_descriptor_for(static_cast<page_id>(i));
        if (ui::tab_button(tabs[i], localization::tr(descriptor.navigation_label), {
            .layer = kLayer,
            .font_size = 22,
            .selected = static_cast<int>(current_page) == i,
            .style = ui::tab_button_style::raised,
            .border_width = 2.0f,
            .selected_border_width = 2.0f,
            .bg = theme.row,
            .bg_hover = theme.row_hover,
            .bg_selected = theme.row_selected,
            .bg_selected_hover = theme.row_active,
            .border = theme.border,
            .border_selected = theme.border,
            .text_color = theme.text_secondary,
            .selected_text_color = theme.text,
            .custom_colors = true,
        }).clicked) {
            result.selected_page = static_cast<page_id>(i);
        }
    }
    return result;
}

inline void draw_content_header(page_id current_page) {
    const page_descriptor& descriptor = page_descriptor_for(current_page);
    ui::draw_header_block(kContentHeaderRect, localization::tr(descriptor.title),
                          localization::tr(descriptor.subtitle));
}

inline shell_draw_result draw_shell(page_id current_page) {
    shell_draw_result result;
    ui::panel(kSidebarRect);
    ui::panel(kContentRect);

    ui::draw_header_block(kSidebarHeaderRect, localization::tr(localization::text_key::settings),
                          localization::tr(localization::text_key::saved_on_back));

    const shell_draw_result tab_result = draw_tab_buttons(current_page);
    result.selected_page = tab_result.selected_page;

    if (ui::button(kBackRect, localization::tr(localization::text_key::back), {
            .layer = kLayer,
            .font_size = 22,
        }).clicked) {
        result.back_requested = true;
    }

    draw_content_header(current_page);
    return result;
}

}  // namespace settings
