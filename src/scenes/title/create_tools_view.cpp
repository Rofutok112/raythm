#include "title/create_tools_view.h"

#include <algorithm>
#include <vector>

#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_layout.h"
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

    const int column = index % kCreateToolColumnCount;
    const int row = index / kCreateToolColumnCount;
    const float width =
        (section_rect.width - kCreateToolColumnGap * static_cast<float>(kCreateToolColumnCount - 1)) /
        static_cast<float>(kCreateToolColumnCount);
    return {
        section_rect.x + static_cast<float>(column) * (width + kCreateToolColumnGap),
        section_rect.y + static_cast<float>(row) * (kCreateToolButtonHeight + kCreateToolButtonGap),
        width,
        kCreateToolButtonHeight,
    };
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
    return rect.y + rect.height >= viewport.y - kCreateToolsClipSlack &&
           rect.y <= viewport.y + viewport.height + kCreateToolsClipSlack;
}

bool create_tool_rect_contains(Rectangle viewport, Rectangle rect, Vector2 point) {
    return ui::contains_point(viewport, point) && ui::contains_point(rect, point);
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
    rows += (secondary_count + kCreateToolColumnCount - 1) / kCreateToolColumnCount;
    return static_cast<float>(rows) * kCreateToolButtonHeight +
           static_cast<float>(std::max(0, rows - 1)) * kCreateToolButtonGap;
}

template <typename SectionCallback, typename EntryCallback>
bool visit_create_tools(const title_create_tools_model::view_model& model,
                        Rectangle content_rect,
                        SectionCallback on_section,
                        EntryCallback on_entry) {
    float section_y = content_rect.y;
    for (const title_create_tools_model::section& section : model.sections) {
        const Rectangle section_label_rect{
            content_rect.x,
            section_y,
            content_rect.width,
            kCreatePanelSectionLabelHeight,
        };
        if (!on_section(section, section_label_rect)) {
            return false;
        }

        const float tools_y = section_y + kCreatePanelSectionLabelHeight;
        const Rectangle tools_rect{
            content_rect.x,
            tools_y,
            content_rect.width,
            content_rect.height - (tools_y - content_rect.y),
        };
        int primary_index = 0;
        int secondary_index = 0;
        for (const title_create_tools_model::entry& entry : section.entries) {
            const int index = entry.primary ? primary_index++ : secondary_index++;
            const Rectangle rect = create_tool_entry_rect(tools_rect, index, entry.primary, primary_index);
            if (!on_entry(section, entry, rect)) {
                return false;
            }
        }
        section_y = tools_y + create_tools_height(section.entries) + kCreatePanelSectionGap;
    }

    return true;
}

void apply_action(title_play_view::update_result& result, title_create_tools_model::action action) {
    switch (action) {
    case title_create_tools_model::action::create_song: result.create_song_requested = true; break;
    case title_create_tools_model::action::edit_song: result.edit_song_requested = true; break;
    case title_create_tools_model::action::import_song: result.import_song_requested = true; break;
    case title_create_tools_model::action::export_song: result.export_song_requested = true; break;
    case title_create_tools_model::action::upload_song: result.upload_song_requested = true; break;
    case title_create_tools_model::action::create_chart: result.create_chart_requested = true; break;
    case title_create_tools_model::action::edit_chart: result.edit_chart_requested = true; break;
    case title_create_tools_model::action::import_chart: result.import_chart_requested = true; break;
    case title_create_tools_model::action::export_chart: result.export_chart_requested = true; break;
    case title_create_tools_model::action::upload_chart: result.upload_chart_requested = true; break;
    case title_create_tools_model::action::edit_mv: result.edit_mv_requested = true; break;
    }
}

void draw_create_tool_card(Rectangle rect,
                           const title_create_tools_model::entry& entry,
                           const title_create_tools_view::draw_config& config,
                           bool hovered) {
    const auto& t = *g_theme;
    const unsigned char row_alpha = !entry.enabled
        ? static_cast<unsigned char>(config.normal_row_alpha / 2)
        : hovered ? config.hover_row_alpha : config.normal_row_alpha;
    const Color action_color = entry.primary && entry.enabled
        ? (entry.title.find("UPDATE") != std::string::npos ? t.accent : t.success)
        : config.button_base;
    const Color fill = entry.primary && entry.enabled
        ? with_alpha(lerp_color(config.button_base, action_color, hovered ? 0.30f : 0.20f), row_alpha)
        : with_alpha(config.button_base, row_alpha);
    const Color border = entry.primary && entry.enabled
        ? with_alpha(action_color, static_cast<unsigned char>(std::min(255, static_cast<int>(row_alpha) + 38)))
        : with_alpha(t.border, row_alpha);
    ui::surface(rect, fill, border, entry.primary ? 1.6f : 1.2f, {
        .fill = fill,
        .border_color = border,
        .custom_colors = true,
    });
    const create_tool_card_layout layout = make_create_tool_card_layout(rect);
    ui::draw_text_in_rect(entry.title.c_str(), entry.primary ? 16 : 13,
                          layout.title,
                          with_alpha(entry.enabled ? t.text : t.text_muted, config.alpha),
                          ui::text_align::left);
    ui::draw_text_in_rect(entry.detail.c_str(), 10,
                          layout.detail,
                          with_alpha(t.text_muted, config.alpha), ui::text_align::left);
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
    visit_create_tools(
        model,
        content_rect,
        [](const title_create_tools_model::section&, Rectangle) {
            return true;
        },
        [&](const title_create_tools_model::section&,
            const title_create_tools_model::entry& entry,
            Rectangle rect) {
            if (!entry.enabled ||
                !create_tool_rect_visible(content_rect, rect) ||
                !create_tool_rect_contains(content_rect, rect, mouse)) {
                return true;
            }
            apply_action(result, entry.command);
            return false;
        });

    return result;
}

void draw(const title_create_tools_model::view_model& model, const draw_config& config) {
    const auto& t = *g_theme;

    const Rectangle header_rect = create_tools_header_rect(config.current);
    const Rectangle content_rect = create_tools_content_rect(config.current);
    ui::draw_text_in_rect("CREATE", 22, header_rect,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    ui::scoped_clip_rect clip(content_rect);

    visit_create_tools(
        model,
        content_rect,
        [&](const title_create_tools_model::section& section, Rectangle section_label_rect) {
            if (create_tool_rect_visible(content_rect, section_label_rect)) {
                ui::draw_text_in_rect(section.title.c_str(), 14, section_label_rect,
                                      with_alpha(t.text_secondary, config.alpha), ui::text_align::left);
            }
            return true;
        },
        [&](const title_create_tools_model::section&,
            const title_create_tools_model::entry& entry,
            Rectangle rect) {
            if (!create_tool_rect_visible(content_rect, rect)) {
                return true;
            }
            const bool hovered = entry.enabled && create_tool_rect_contains(content_rect, rect, mouse);
            draw_create_tool_card(rect, entry, config, hovered);
            return true;
        });
}

}  // namespace title_create_tools_view
