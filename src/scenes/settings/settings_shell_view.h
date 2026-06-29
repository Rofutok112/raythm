#pragma once

#include <array>
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

struct shell_tab_button {
    page_id page = page_id::gameplay;
    localization::text_key label = localization::text_key::gameplay;
    Rectangle rect{};
    bool selected = false;
};

struct shell_action_button {
    Rectangle rect{};
    localization::text_key label = localization::text_key::back;
};

inline std::array<shell_tab_button, kPageCount> shell_tab_buttons_for(page_id current_page) {
    std::array<Rectangle, kPageCount> rects{};
    build_tab_rects(rects);

    std::array<shell_tab_button, kPageCount> buttons{};
    for (int i = 0; i < kPageCount; ++i) {
        const page_id page = static_cast<page_id>(i);
        const page_descriptor& descriptor = page_descriptor_for(page);
        buttons[static_cast<std::size_t>(i)] = {
            .page = page,
            .label = descriptor.navigation_label,
            .rect = rects[static_cast<std::size_t>(i)],
            .selected = page == current_page,
        };
    }
    return buttons;
}

inline ui::button_state draw_shell_tab_button(const shell_tab_button& button) {
    const auto& theme = *g_theme;
    return ui::tab_button(button.rect, localization::tr(button.label), {
        .layer = kLayer,
        .font_size = 22,
        .selected = button.selected,
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
    });
}

inline ui::button_state draw_shell_action_button(const shell_action_button& button) {
    return ui::button(button.rect, localization::tr(button.label), {
        .layer = kLayer,
        .font_size = 22,
    });
}

inline shell_draw_result draw_tab_buttons(page_id current_page) {
    shell_draw_result result;
    for (const shell_tab_button& button : shell_tab_buttons_for(current_page)) {
        if (draw_shell_tab_button(button).clicked) {
            result.selected_page = button.page;
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

    const shell_action_button back_button = {
        .rect = kBackRect,
        .label = localization::text_key::back,
    };
    if (draw_shell_action_button(back_button).clicked) {
        result.back_requested = true;
    }

    draw_content_header(current_page);
    return result;
}

}  // namespace settings
