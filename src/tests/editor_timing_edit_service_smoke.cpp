#include <cstdlib>
#include <iostream>

#include "editor/service/editor_timing_edit_service.h"

namespace {

chart_data make_chart() {
    chart_data data;
    data.meta.chart_id = "editor-timing-edit-service-smoke";
    data.meta.song_id = "song";
    data.meta.key_count = 4;
    data.meta.difficulty = "Normal";
    data.meta.chart_author = "Codex";
    data.meta.format_version = 1;
    data.meta.resolution = 480;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    return data;
}

editor_timing_edit_context context(editor_state& state,
                                   editor_meter_map& meter_map,
                                   editor_timing_panel_state& timing_panel,
                                   int default_tick = 480) {
    return {state, meter_map, timing_panel, default_tick};
}

}  // namespace

int main() {
    editor_state state(make_chart(), "");
    editor_meter_map meter_map;
    meter_map.rebuild(state.data());
    editor_timing_panel_state panel;

    const editor_timing_edit_result added =
        editor_timing_edit_service::add_scroll_event(context(state, meter_map, panel, 480));
    if (!added.success || !added.selected_scroll_event_index.has_value() ||
        state.data().scroll_automation.size() != 1 ||
        state.data().scroll_automation.front().tick != 480) {
        std::cerr << "add_scroll_event should create an undoable scroll point\n";
        return EXIT_FAILURE;
    }

    const editor_timing_edit_result duplicate =
        editor_timing_edit_service::add_scroll_event(
            context(state, meter_map, panel, 960),
            scroll_automation_point{480, 1.0f, scroll_automation_curve::ease_in});
    if (duplicate.success || state.data().scroll_automation.size() != 1) {
        std::cerr << "add_scroll_event should reject overlapping scroll points\n";
        return EXIT_FAILURE;
    }

    const editor_timing_edit_result same_tick =
        editor_timing_edit_service::add_scroll_event(
            context(state, meter_map, panel, 960),
            scroll_automation_point{480, 2.0f, scroll_automation_curve::ease_in});
    if (!same_tick.success || state.data().scroll_automation.size() != 2) {
        std::cerr << "add_scroll_event should allow same-tick scroll rate changes\n";
        return EXIT_FAILURE;
    }

    panel.selected_scroll_event_index = 0;
    const editor_timing_edit_result cycled =
        editor_timing_edit_service::cycle_selected_scroll_curve(context(state, meter_map, panel));
    if (!cycled.success ||
        state.data().scroll_automation.front().curve_to_next != scroll_automation_curve::ease_in) {
        std::cerr << "cycle_selected_scroll_curve should mutate via timing edit service\n";
        return EXIT_FAILURE;
    }

    panel.inputs.scroll_start_bar.value = "2:1";
    panel.inputs.scroll_multiplier.value = "1.75";
    const editor_timing_edit_result applied =
        editor_timing_edit_service::apply_selected_scroll(context(state, meter_map, panel));
    if (!applied.success || state.data().scroll_automation.front().tick != 1920 ||
        state.data().scroll_automation.front().multiplier != 1.75f) {
        std::cerr << "apply_selected_scroll should parse panel inputs and update the point\n";
        return EXIT_FAILURE;
    }

    scroll_automation_guides guides;
    guides.values[0] = 0.25f;
    guides.values[1] = 0.75f;
    guides.values[2] = 2.0f;
    guides.values[3] = 4.0f;
    const editor_timing_edit_result guide_result =
        editor_timing_edit_service::modify_scroll_guides(context(state, meter_map, panel), guides);
    if (!guide_result.success || state.data().scroll_guides.values[3] != 4.0f) {
        std::cerr << "modify_scroll_guides should route guide edits through the service\n";
        return EXIT_FAILURE;
    }

    const editor_timing_edit_result deleted =
        editor_timing_edit_service::delete_selected_scroll(context(state, meter_map, panel));
    if (!deleted.success || state.data().scroll_automation.size() != 1) {
        std::cerr << "delete_selected_scroll should remove the selected point\n";
        return EXIT_FAILURE;
    }
    if (!state.undo() || state.data().scroll_automation.size() != 2) {
        std::cerr << "scroll delete should remain undoable through editor_state\n";
        return EXIT_FAILURE;
    }

    panel.selected_event_index = 0;
    panel.inputs.bpm_bar.value = "1:1";
    panel.inputs.bpm_value.value = "150";
    const editor_timing_edit_result bpm_result =
        editor_timing_edit_service::apply_selected(context(state, meter_map, panel));
    if (!bpm_result.success || state.data().timing_events.front().bpm != 150.0f) {
        std::cerr << "apply_selected should update timing events through the service\n";
        return EXIT_FAILURE;
    }

    std::cout << "editor_timing_edit_service smoke test passed\n";
    return EXIT_SUCCESS;
}
