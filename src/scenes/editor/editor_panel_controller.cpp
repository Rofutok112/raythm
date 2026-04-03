#include "editor_panel_controller.h"

editor_metadata_panel_result editor_panel_controller::update_metadata_panel(
    metadata_panel_state& metadata_panel,
    editor_timing_panel_state& timing_panel,
    const editor_metadata_panel_actions& actions) {
    editor_metadata_panel_result result;

    if (actions.metadata_input_activated) {
        timing_panel.active_input_field = editor_timing_input_field::none;
        timing_panel.bar_pick_mode = false;
        timing_panel.input_error.clear();
    }

    if (actions.key_count_toggle_requested && !metadata_panel.key_count_confirm_open) {
        metadata_panel.key_count = metadata_panel.key_count == 4 ? 6 : 4;
        metadata_panel.error.clear();
        result.request_apply_metadata = true;
    }

    if (actions.metadata_submit_requested) {
        result.request_apply_metadata = true;
        metadata_panel.difficulty_input.active = false;
        metadata_panel.chart_author_input.active = false;
        metadata_panel.chart_name_input.active = false;
        metadata_panel.description_input.active = false;
    }

    return result;
}

editor_timing_panel_update_result editor_panel_controller::update_timing_panel(
    metadata_panel_state& metadata_panel,
    editor_timing_panel_state& timing_panel,
    const editor_timing_panel_actions& actions) {
    editor_timing_panel_update_result result;
    result.select_timing_event_index = actions.panel_result.selected_event_index;
    result.request_add_bpm = actions.panel_result.add_bpm;
    result.request_add_meter = actions.panel_result.add_meter;
    result.request_delete_selected = actions.panel_result.delete_selected;
    result.request_apply_selected = actions.panel_result.apply_selected;

    if (actions.panel_result.selected_event_index.has_value() || actions.panel_result.clicked_input_row) {
        metadata_panel.difficulty_input.active = false;
        metadata_panel.chart_author_input.active = false;
    }

    if (actions.panel_result.apply_selected) {
        timing_panel.active_input_field = editor_timing_input_field::none;
        timing_panel.bar_pick_mode = false;
    }

    if (actions.clicked_outside_editor &&
        timing_panel.active_input_field != editor_timing_input_field::none &&
        !actions.panel_result.clicked_input_row) {
        timing_panel.active_input_field = editor_timing_input_field::none;
        timing_panel.bar_pick_mode = false;
    }

    return result;
}
