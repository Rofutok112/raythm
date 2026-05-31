#include "title/create_tools_view.h"

#include <algorithm>
#include <vector>

#include "scene_common.h"
#include "theme.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr float kCreateToolButtonHeight = 50.0f;
constexpr float kCreateToolButtonGap = 8.0f;
constexpr float kCreateToolColumnGap = 12.0f;
constexpr float kCreatePanelSectionGap = 22.0f;
constexpr float kCreatePanelSectionLabelHeight = 24.0f;
constexpr int kCreateToolColumnCount = 2;

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
    case title_create_tools_model::action::manage_library: result.manage_library_requested = true; break;
    }
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

    float section_y = current.ranking_list_rect.y;
    for (const title_create_tools_model::section& section : model.sections) {
        const float tools_y = section_y + kCreatePanelSectionLabelHeight;
        const Rectangle tools_rect{
            current.ranking_list_rect.x,
            tools_y,
            current.ranking_list_rect.width,
            current.ranking_list_rect.height - (tools_y - current.ranking_list_rect.y),
        };
        int primary_index = 0;
        int secondary_index = 0;
        for (const title_create_tools_model::entry& entry : section.entries) {
            const int index = entry.primary ? primary_index++ : secondary_index++;
            Rectangle rect = create_tool_rect(tools_rect, index, entry.primary);
            if (!entry.primary) {
                const float secondary_top =
                    tools_y + static_cast<float>(primary_index) * (kCreateToolButtonHeight + kCreateToolButtonGap);
                rect.y = secondary_top +
                    static_cast<float>(index / kCreateToolColumnCount) *
                        (kCreateToolButtonHeight + kCreateToolButtonGap);
            }
            if (!entry.enabled || !CheckCollisionPointRec(mouse, rect)) {
                continue;
            }
            apply_action(result, entry.command);
            return result;
        }
        section_y = tools_y + create_tools_height(section.entries) + kCreatePanelSectionGap;
    }

    return result;
}

void draw(const title_create_tools_model::view_model& model, const draw_config& config) {
    const auto& t = *g_theme;

    ui::draw_text_in_rect("CREATE", 22, config.current.ranking_header_rect,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    float section_y = config.current.ranking_list_rect.y;
    for (const title_create_tools_model::section& section : model.sections) {
        ui::draw_text_in_rect(section.title.c_str(), 14,
                              {config.current.ranking_list_rect.x, section_y,
                               config.current.ranking_list_rect.width, kCreatePanelSectionLabelHeight},
                              with_alpha(t.text_secondary, config.alpha), ui::text_align::left);
        const float tools_y = section_y + kCreatePanelSectionLabelHeight;
        const Rectangle tools_rect{
            config.current.ranking_list_rect.x,
            tools_y,
            config.current.ranking_list_rect.width,
            config.current.ranking_list_rect.height - (tools_y - config.current.ranking_list_rect.y),
        };
        int primary_index = 0;
        int secondary_index = 0;
        for (const title_create_tools_model::entry& entry : section.entries) {
            const int index = entry.primary ? primary_index++ : secondary_index++;
            Rectangle rect = create_tool_rect(tools_rect, index, entry.primary);
            if (!entry.primary) {
                const float secondary_top =
                    tools_y + static_cast<float>(primary_index) * (kCreateToolButtonHeight + kCreateToolButtonGap);
                rect.y = secondary_top +
                    static_cast<float>(index / kCreateToolColumnCount) *
                        (kCreateToolButtonHeight + kCreateToolButtonGap);
            }

            const bool hovered = entry.enabled && CheckCollisionPointRec(mouse, rect);
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
            ui::draw_rect_f(rect, fill);
            ui::draw_rect_lines(rect, entry.primary ? 1.6f : 1.2f, border);
            ui::draw_text_in_rect(entry.title.c_str(), entry.primary ? 16 : 13,
                                  {rect.x + 12.0f, rect.y + 5.0f, rect.width - 24.0f, 20.0f},
                                  with_alpha(entry.enabled ? t.text : t.text_muted, config.alpha),
                                  ui::text_align::left);
            ui::draw_text_in_rect(entry.detail.c_str(), 10,
                                  {rect.x + 12.0f, rect.y + 27.0f, rect.width - 24.0f, 15.0f},
                                  with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        }
        section_y = tools_y + create_tools_height(section.entries) + kCreatePanelSectionGap;
    }
}

}  // namespace title_create_tools_view
