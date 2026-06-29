#include "title/create_tools_view.h"

#include <algorithm>
#include <array>
#include <vector>

#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_layout.h"
#include "ui_scroll.h"
#include "virtual_screen.h"

namespace {

constexpr float kCreateToolButtonHeight = 50.0f;
constexpr float kCreateToolButtonGap = 8.0f;
constexpr float kCreateToolColumnGap = 12.0f;
constexpr float kCreatePanelSectionGap = 22.0f;
constexpr float kCreatePanelSectionLabelHeight = 24.0f;
constexpr int kCreateToolColumnCount = 2;
constexpr float kCreatePanelPaddingX = 28.0f;
constexpr float kCreatePanelHeaderOffsetY = 24.0f;
constexpr float kCreatePanelHeaderHeight = 30.0f;
constexpr float kCreatePanelContentOffsetY = 72.0f;
constexpr float kCreatePanelContentBottomPadding = 24.0f;
constexpr float kCreateToolTextPaddingX = 12.0f;
constexpr float kCreateToolTitleOffsetY = 5.0f;
constexpr float kCreateToolTitleHeight = 20.0f;
constexpr float kCreateToolDetailOffsetY = 27.0f;
constexpr float kCreateToolDetailHeight = 15.0f;
constexpr float kCreateToolsClipSlack = 2.0f;

struct create_tools_panel_layout {
    Rectangle panel;
    Rectangle header;
    Rectangle content;
};

struct create_tool_card_layout {
    Rectangle title;
    Rectangle detail;
};

struct create_tool_entry_card {
    const title_create_tools_model::entry* entry = nullptr;
    Rectangle rect{};
};

struct create_tool_card_style {
    Color fill{};
    Color border{};
    float border_width = 1.2f;
};

struct action_result_binding {
    title_create_tools_model::action action = title_create_tools_model::action::create_song;
    bool title_play_view::update_result::*flag = nullptr;
};

constexpr std::array<action_result_binding, 11> kActionResultBindings{{
    {title_create_tools_model::action::create_song, &title_play_view::update_result::create_song_requested},
    {title_create_tools_model::action::edit_song, &title_play_view::update_result::edit_song_requested},
    {title_create_tools_model::action::import_song, &title_play_view::update_result::import_song_requested},
    {title_create_tools_model::action::export_song, &title_play_view::update_result::export_song_requested},
    {title_create_tools_model::action::upload_song, &title_play_view::update_result::upload_song_requested},
    {title_create_tools_model::action::create_chart, &title_play_view::update_result::create_chart_requested},
    {title_create_tools_model::action::edit_chart, &title_play_view::update_result::edit_chart_requested},
    {title_create_tools_model::action::import_chart, &title_play_view::update_result::import_chart_requested},
    {title_create_tools_model::action::export_chart, &title_play_view::update_result::export_chart_requested},
    {title_create_tools_model::action::upload_chart, &title_play_view::update_result::upload_chart_requested},
    {title_create_tools_model::action::edit_mv, &title_play_view::update_result::edit_mv_requested},
}};

create_tools_panel_layout make_create_tools_panel_layout(const title_play_view::layout& current) {
    const Rectangle panel = title_play_view::right_bottom_pane_rect(current.ranking_column);
    return {
        panel,
        {
            panel.x + kCreatePanelPaddingX,
            panel.y + kCreatePanelHeaderOffsetY,
            panel.width - kCreatePanelPaddingX * 2.0f,
            kCreatePanelHeaderHeight,
        },
        ui::inset(panel, {
            kCreatePanelContentOffsetY,
            kCreatePanelPaddingX,
            kCreatePanelContentBottomPadding,
            kCreatePanelPaddingX,
        }),
    };
}

create_tool_card_layout make_create_tool_card_layout(Rectangle rect) {
    return {
        {
            rect.x + kCreateToolTextPaddingX,
            rect.y + kCreateToolTitleOffsetY,
            rect.width - kCreateToolTextPaddingX * 2.0f,
            kCreateToolTitleHeight,
        },
        {
            rect.x + kCreateToolTextPaddingX,
            rect.y + kCreateToolDetailOffsetY,
            rect.width - kCreateToolTextPaddingX * 2.0f,
            kCreateToolDetailHeight,
        },
    };
}

Rectangle create_tools_header_rect(const title_play_view::layout& current) {
    return make_create_tools_panel_layout(current).header;
}

Rectangle create_tools_content_rect(const title_play_view::layout& current) {
    return make_create_tools_panel_layout(current).content;
}

Rectangle create_tool_rect(Rectangle section_rect, int index, bool primary) {
    if (primary) {
        return {
            section_rect.x,
            section_rect.y,
            section_rect.width,
            kCreateToolButtonHeight,
        };
    }

    const float width =
        (section_rect.width - kCreateToolColumnGap * static_cast<float>(kCreateToolColumnCount - 1)) /
        static_cast<float>(kCreateToolColumnCount);
    return ui::vertical_grid_item_rect(
        section_rect,
        index,
        kCreateToolColumnCount,
        width,
        kCreateToolButtonHeight,
        kCreateToolColumnGap,
        kCreateToolButtonGap,
        0.0f);
}

Rectangle create_tool_entry_rect(Rectangle tools_rect, int index, bool primary, int primary_count) {
    Rectangle rect = create_tool_rect(tools_rect, index, primary);
    if (!primary) {
        const float secondary_top =
            tools_rect.y + static_cast<float>(primary_count) * (kCreateToolButtonHeight + kCreateToolButtonGap);
        rect.y = secondary_top +
            static_cast<float>(index / kCreateToolColumnCount) *
                (kCreateToolButtonHeight + kCreateToolButtonGap);
    }
    return rect;
}

bool create_tool_rect_visible(Rectangle viewport, Rectangle rect) {
    return ui::rect_visible_in_viewport(rect, viewport, kCreateToolsClipSlack);
}

bool create_tool_rect_contains(Rectangle viewport, Rectangle rect, Vector2 point) {
    return ui::contains_point(viewport, point) && ui::contains_point(rect, point);
}

create_tool_entry_card create_tool_entry_card_for(const title_create_tools_model::entry& entry, Rectangle rect) {
    return {
        .entry = &entry,
        .rect = rect,
    };
}

bool create_tool_entry_visible(Rectangle viewport, const create_tool_entry_card& card) {
    return create_tool_rect_visible(viewport, card.rect);
}

bool create_tool_entry_contains(Rectangle viewport, const create_tool_entry_card& card, Vector2 point) {
    return create_tool_rect_contains(viewport, card.rect, point);
}

bool create_tool_entry_clicked(Rectangle viewport, const create_tool_entry_card& card, Vector2 point) {
    return card.entry != nullptr &&
           card.entry->enabled &&
           create_tool_entry_visible(viewport, card) &&
           create_tool_entry_contains(viewport, card, point);
}

float create_tools_height(const std::vector<title_create_tools_model::entry>& entries) {
    if (entries.empty()) {
        return 0.0f;
    }

    int rows = 0;
    int secondary_count = 0;
    for (const title_create_tools_model::entry& entry : entries) {
        if (entry.primary) {
            rows += 1;
        } else {
            ++secondary_count;
        }
    }
    rows += ui::grid_row_count(secondary_count, kCreateToolColumnCount);
    return ui::vertical_list_content_height(rows, kCreateToolButtonHeight, kCreateToolButtonGap);
}

template <typename SectionCallback, typename EntryCallback>
bool visit_create_tools(const title_create_tools_model::view_model& model,
                        Rectangle content_rect,
                        SectionCallback on_section,
                        EntryCallback on_entry) {
    ui::vertical_layout_cursor sections =
        ui::vertical_cursor(content_rect.x, content_rect.y, content_rect.width, 0.0f);
    for (const title_create_tools_model::section& section : model.sections) {
        const Rectangle section_label_rect = sections.next(kCreatePanelSectionLabelHeight);
        if (!on_section(section, section_label_rect)) {
            return false;
        }

        const Rectangle tools_rect = sections.next(create_tools_height(section.entries));
        int primary_index = 0;
        int secondary_index = 0;
        for (const title_create_tools_model::entry& entry : section.entries) {
            const int index = entry.primary ? primary_index++ : secondary_index++;
            const Rectangle rect = create_tool_entry_rect(tools_rect, index, entry.primary, primary_index);
            if (!on_entry(section, entry, rect)) {
                return false;
            }
        }
        sections.skip(kCreatePanelSectionGap);
    }

    return true;
}

void apply_action(title_play_view::update_result& result, title_create_tools_model::action action) {
    for (const action_result_binding& binding : kActionResultBindings) {
        if (binding.action == action && binding.flag != nullptr) {
            result.*(binding.flag) = true;
            return;
        }
    }
}

create_tool_card_style create_tool_card_style_for(const create_tool_entry_card& card,
                                                  const title_create_tools_view::draw_config& config,
                                                  bool hovered) {
    if (card.entry == nullptr) {
        return {};
    }
    const title_create_tools_model::entry& entry = *card.entry;
    const auto& t = *g_theme;
    const unsigned char row_alpha = !entry.enabled
        ? static_cast<unsigned char>(config.normal_row_alpha / 2)
        : hovered ? config.hover_row_alpha : config.normal_row_alpha;
    const Color action_color = entry.primary && entry.enabled
        ? (entry.title.find("UPDATE") != std::string::npos ? t.accent : t.success)
        : config.button_base;
    return {
        .fill = entry.primary && entry.enabled
            ? with_alpha(lerp_color(config.button_base, action_color, hovered ? 0.30f : 0.20f), row_alpha)
            : with_alpha(config.button_base, row_alpha),
        .border = entry.primary && entry.enabled
            ? with_alpha(action_color, static_cast<unsigned char>(std::min(255, static_cast<int>(row_alpha) + 38)))
            : with_alpha(t.border, row_alpha),
        .border_width = entry.primary ? 1.6f : 1.2f,
    };
}

void draw_create_tool_card(const create_tool_entry_card& card,
                           const title_create_tools_view::draw_config& config,
                           const create_tool_card_style& style) {
    if (card.entry == nullptr) {
        return;
    }
    const title_create_tools_model::entry& entry = *card.entry;
    const auto& t = *g_theme;
    ui::surface(card.rect, style.fill, style.border, style.border_width, {
        .fill = style.fill,
        .border_color = style.border,
        .custom_colors = true,
    });
    const create_tool_card_layout layout = make_create_tool_card_layout(card.rect);
    ui::draw_text_in_rect(entry.title.c_str(), entry.primary ? 16 : 13,
                          layout.title,
                          with_alpha(entry.enabled ? t.text : t.text_muted, config.alpha),
                          ui::text_align::left);
    ui::draw_text_in_rect(entry.detail.c_str(), 10,
                          layout.detail,
                          with_alpha(t.text_muted, config.alpha), ui::text_align::left);
}

title_play_view::update_result update_create_tool_cards(
    const title_create_tools_model::view_model& model,
    Rectangle content_rect,
    Vector2 mouse) {
    title_play_view::update_result result;
    visit_create_tools(
        model,
        content_rect,
        [](const title_create_tools_model::section&, Rectangle) {
            return true;
        },
        [&](const title_create_tools_model::section&,
            const title_create_tools_model::entry& entry,
            Rectangle rect) {
            const create_tool_entry_card card = create_tool_entry_card_for(entry, rect);
            if (!create_tool_entry_clicked(content_rect, card, mouse)) {
                return true;
            }
            apply_action(result, entry.command);
            return false;
        });
    return result;
}

void draw_create_tools_header(Rectangle header_rect, unsigned char alpha) {
    ui::draw_text_in_rect("CREATE", 22, header_rect,
                          with_alpha(g_theme->text, alpha), ui::text_align::left);
}

void draw_create_tools_section_label(const title_create_tools_model::section& section,
                                     Rectangle section_label_rect,
                                     unsigned char alpha) {
    ui::draw_text_in_rect(section.title.c_str(), 14, section_label_rect,
                          with_alpha(g_theme->text_secondary, alpha), ui::text_align::left);
}

void draw_create_tool_cards(const title_create_tools_model::view_model& model,
                            Rectangle content_rect,
                            const title_create_tools_view::draw_config& config,
                            Vector2 mouse) {
    visit_create_tools(
        model,
        content_rect,
        [&](const title_create_tools_model::section& section, Rectangle section_label_rect) {
            if (create_tool_rect_visible(content_rect, section_label_rect)) {
                draw_create_tools_section_label(section, section_label_rect, config.alpha);
            }
            return true;
        },
        [&](const title_create_tools_model::section&,
            const title_create_tools_model::entry& entry,
            Rectangle rect) {
            const create_tool_entry_card card = create_tool_entry_card_for(entry, rect);
            if (!create_tool_entry_visible(content_rect, card)) {
                return true;
            }
            const bool hovered = card.entry->enabled && create_tool_entry_contains(content_rect, card, mouse);
            draw_create_tool_card(card, config, create_tool_card_style_for(card, config, hovered));
            return true;
        });
}

}  // namespace

namespace title_create_tools_view {

title_play_view::update_result update(const title_create_tools_model::view_model& model,
                                      const title_play_view::layout& current,
                                      bool left_pressed,
                                      Vector2 mouse) {
    title_play_view::update_result result;
    if (!left_pressed) {
        return result;
    }

    const Rectangle content_rect = create_tools_content_rect(current);
    return update_create_tool_cards(model, content_rect, mouse);
}

void draw(const title_create_tools_model::view_model& model, const draw_config& config) {
    const Rectangle header_rect = create_tools_header_rect(config.current);
    const Rectangle content_rect = create_tools_content_rect(config.current);
    draw_create_tools_header(header_rect, config.alpha);

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    ui::scoped_clip_rect clip(content_rect);
    draw_create_tool_cards(model, content_rect, config, mouse);
}

}  // namespace title_create_tools_view
