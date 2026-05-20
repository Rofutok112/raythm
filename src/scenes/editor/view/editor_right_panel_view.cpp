#include "editor/view/editor_right_panel_view.h"

#include <string>

#include "editor/view/editor_layout.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_layout.h"

namespace {
namespace layout = editor::layout;

const char* timing_event_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Time Sig";
}

}

editor_right_panel_view_result editor_right_panel_view::draw(const editor_right_panel_view_model& model,
                                                             editor_timing_panel_state& timing_state) {
    editor_right_panel_view_result result;
    const Rectangle content = ui::inset(layout::kRightPanelRect, ui::edge_insets::uniform(16.0f));

    // Timing panel
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

    std::vector<editor_timing_panel_item> scroll_items;
    scroll_items.reserve(model.scroll_automation->size());
    for (size_t index = 0; index < model.scroll_automation->size(); ++index) {
        const scroll_automation_point& point = (*model.scroll_automation)[index];
        scroll_items.push_back({
            index,
            std::string("Automation ") + model.meter_map->bar_beat_label(point.tick),
            TextFormat("%.2fx", point.multiplier),
            model.selected_scroll_event_index.has_value() && *model.selected_scroll_event_index == index
        });
    }

    std::optional<timing_event> selected_event;
    if (model.selected_event_index.has_value() && *model.selected_event_index < model.timing_events->size()) {
        selected_event = (*model.timing_events)[*model.selected_event_index];
    }
    std::optional<scroll_automation_point> selected_scroll_event;
    if (model.selected_scroll_event_index.has_value() && *model.selected_scroll_event_index < model.scroll_automation->size()) {
        selected_scroll_event = (*model.scroll_automation)[*model.selected_scroll_event_index];
    }

    result.panel_result = editor_timing_panel::draw(
        {content, model.mouse, std::move(items), std::move(scroll_items),
         selected_event, selected_scroll_event, model.selected_note_count, model.selected_note_summary,
         model.delete_enabled, model.scroll_delete_enabled},
        timing_state);
    const Rectangle editor_box = {content.x, content.y + 660.0f, content.width, content.height - 660.0f};
    result.clicked_outside_editor = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                    !CheckCollisionPointRec(model.mouse, editor_box);

    return result;
}
