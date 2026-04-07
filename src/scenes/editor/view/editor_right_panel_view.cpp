#include "editor/view/editor_right_panel_view.h"

#include <string>

#include "editor/view/editor_layout.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_layout.h"

namespace {
namespace layout = editor::layout;

constexpr float kTabBarHeight = 28.0f;
constexpr float kTabGap = 4.0f;
constexpr float kTabContentGap = 8.0f;

const char* timing_event_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Meter";
}
}

editor_right_panel_view_result editor_right_panel_view::draw(const editor_right_panel_view_model& model,
                                                             editor_timing_panel_state& timing_state,
                                                             editor_mv_script_panel_state& mv_script_state) {
    editor_right_panel_view_result result;
    const Rectangle content = ui::inset(layout::kRightPanelRect, ui::edge_insets::uniform(16.0f));

    // Tab bar
    Rectangle tab_bar = {content.x, content.y, content.width, kTabBarHeight};
    float tab_w = (tab_bar.width - kTabGap) * 0.5f;
    Rectangle timing_tab = {tab_bar.x, tab_bar.y, tab_w, kTabBarHeight};
    Rectangle mv_tab = {tab_bar.x + tab_w + kTabGap, tab_bar.y, tab_w, kTabBarHeight};

    bool timing_active = (model.active_tab == editor_right_panel_tab::timing);
    auto timing_btn = ui::draw_button_colored(timing_tab, "Timing", 14,
        timing_active ? g_theme->row_selected : g_theme->row,
        timing_active ? g_theme->row_selected_hover : g_theme->row_hover,
        g_theme->text);
    auto mv_btn = ui::draw_button_colored(mv_tab, "MV Script", 14,
        !timing_active ? g_theme->row_selected : g_theme->row,
        !timing_active ? g_theme->row_selected_hover : g_theme->row_hover,
        g_theme->text);

    if (timing_btn.clicked) result.timing_tab_clicked = true;
    if (mv_btn.clicked) result.mv_script_tab_clicked = true;

    // Content area below tabs
    Rectangle tab_content = {
        content.x, content.y + kTabBarHeight + kTabContentGap,
        content.width, content.height - kTabBarHeight - kTabContentGap
    };

    if (model.active_tab == editor_right_panel_tab::timing) {
        // Timing panel (existing logic)
        std::vector<editor_timing_panel_item> items;
        items.reserve(model.timing_events->size());
        for (size_t index = 0; index < model.timing_events->size(); ++index) {
            const timing_event& event = (*model.timing_events)[index];
            items.push_back({
                index,
                std::string(timing_event_type_label(event.type)) + " " + model.meter_map->bar_beat_label(event.tick),
                event.type == timing_event_type::bpm
                    ? TextFormat("%.1f", event.bpm)
                    : TextFormat("%d/%d", event.numerator, event.denominator),
                model.selected_event_index.has_value() && *model.selected_event_index == index
            });
        }

        std::optional<timing_event> selected_event;
        if (model.selected_event_index.has_value() && *model.selected_event_index < model.timing_events->size()) {
            selected_event = (*model.timing_events)[*model.selected_event_index];
        }

        result.panel_result = editor_timing_panel::draw(
            {tab_content, model.mouse, std::move(items), selected_event, model.delete_enabled},
            timing_state);
        const Rectangle editor_box = {tab_content.x, tab_content.y + 372.0f, tab_content.width, 164.0f};
        result.clicked_outside_editor = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                        !CheckCollisionPointRec(model.mouse, editor_box);
    } else {
        // MV Script panel
        result.mv_script_result = editor_mv_script_panel::draw(
            {tab_content, model.mouse}, mv_script_state);
    }

    return result;
}
