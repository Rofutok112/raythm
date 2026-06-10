#pragma once

#include <optional>

#include "localization/localization.h"
#include "settings/settings_layout.h"
#include "theme.h"
#include "ui_draw.h"

namespace settings {

inline std::optional<page_id> clicked_tab_page() {
    Rectangle tabs[kPageCount];
    build_tab_rects(tabs);
    for (int i = 0; i < kPageCount; ++i) {
        if (ui::is_clicked(tabs[i], kLayer)) {
            return static_cast<page_id>(i);
        }
    }
    return std::nullopt;
}

inline void draw_tab_buttons(page_id current_page) {
    const auto& theme = *g_theme;
    Rectangle tabs[kPageCount];
    build_tab_rects(tabs);
    for (int i = 0; i < kPageCount; ++i) {
        const page_descriptor& descriptor = page_descriptor_for(static_cast<page_id>(i));
        if (static_cast<int>(current_page) == i) {
            ui::draw_button_colored(tabs[i], localization::tr(descriptor.navigation_label), 22,
                                    theme.row_selected, theme.row_active, theme.text);
        } else {
            ui::draw_button_colored(tabs[i], localization::tr(descriptor.navigation_label), 22,
                                    theme.row, theme.row_hover, theme.text_secondary);
        }
    }
}

inline void draw_content_header(page_id current_page) {
    const page_descriptor& descriptor = page_descriptor_for(current_page);
    ui::draw_header_block(kContentHeaderRect, localization::tr(descriptor.title),
                          localization::tr(descriptor.subtitle));
}

}  // namespace settings
