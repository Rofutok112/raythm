#include <cstdlib>
#include <iostream>

#include "editor/editor_panel_controller.h"

int main() {
    {
        metadata_panel_state metadata_panel;
        editor_timing_panel_state timing_panel;
        timing_panel.active_input_field = editor_timing_input_field::bpm_value;
        timing_panel.bar_pick_mode = true;
        timing_panel.input_error = "error";

        const editor_metadata_panel_result result = editor_panel_controller::update_metadata_panel(
            metadata_panel, timing_panel, {true, false, false});
        if (result.request_apply_metadata || timing_panel.active_input_field != editor_timing_input_field::none ||
            timing_panel.bar_pick_mode || !timing_panel.input_error.empty()) {
            std::cerr << "metadata activation should clear timing panel focus\n";
            return EXIT_FAILURE;
        }
    }

    {
        metadata_panel_state metadata_panel;
        editor_timing_panel_state timing_panel;
        const editor_metadata_panel_result result = editor_panel_controller::update_metadata_panel(
            metadata_panel, timing_panel, {false, false, true});
        if (!result.request_apply_metadata || metadata_panel.key_count != 6) {
            std::cerr << "key count toggle should request metadata apply\n";
            return EXIT_FAILURE;
        }
    }

    {
        metadata_panel_state metadata_panel;
        editor_timing_panel_state timing_panel;
        metadata_panel.difficulty_input.active = true;
        metadata_panel.chart_author_input.active = true;
        const editor_metadata_panel_result result = editor_panel_controller::update_metadata_panel(
            metadata_panel, timing_panel, {false, true, false});
        if (!result.request_apply_metadata || metadata_panel.difficulty_input.active || metadata_panel.chart_author_input.active) {
            std::cerr << "metadata submit should deactivate inputs and request apply\n";
            return EXIT_FAILURE;
        }
    }

    {
        metadata_panel_state metadata_panel;
        metadata_panel.difficulty_input.active = true;
        metadata_panel.chart_author_input.active = true;
        editor_timing_panel_state timing_panel;
        timing_panel.active_input_field = editor_timing_input_field::bpm_value;

        editor_timing_panel_result panel_result;
        panel_result.selected_event_index = 2;
        panel_result.apply_selected = true;
        const editor_timing_panel_update_result result = editor_panel_controller::update_timing_panel(
            metadata_panel, timing_panel, {panel_result, false});
        if (!result.select_timing_event_index.has_value() || *result.select_timing_event_index != 2 ||
            !result.request_apply_selected || metadata_panel.difficulty_input.active ||
            metadata_panel.chart_author_input.active ||
            timing_panel.active_input_field != editor_timing_input_field::none) {
            std::cerr << "timing panel selection/apply should update panel focus state\n";
            return EXIT_FAILURE;
        }
    }

    {
        metadata_panel_state metadata_panel;
        editor_timing_panel_state timing_panel;
        timing_panel.active_input_field = editor_timing_input_field::meter_numerator;
        timing_panel.bar_pick_mode = true;
        const editor_timing_panel_update_result result = editor_panel_controller::update_timing_panel(
            metadata_panel, timing_panel, {{}, true});
        if (result.request_apply_selected || timing_panel.active_input_field != editor_timing_input_field::none ||
            timing_panel.bar_pick_mode) {
            std::cerr << "outside click should clear timing editor focus\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "editor_panel_controller smoke test passed\n";
    return EXIT_SUCCESS;
}
