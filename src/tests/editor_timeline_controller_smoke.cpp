#include <cstdlib>
#include <iostream>
#include <memory>

#include "editor/editor_timeline_controller.h"

namespace {

chart_data make_chart() {
    chart_data data;
    data.meta.chart_id = "timeline-smoke";
    data.meta.song_id = "song";
    data.meta.key_count = 4;
    data.meta.difficulty = "Normal";
    data.meta.chart_author = "Codex";
    data.meta.format_version = 1;
    data.meta.resolution = 480;
    data.meta.offset = 0;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    data.notes = {
        {note_type::tap, 480, 1, 480},
    };
    return data;
}

editor_timeline_metrics make_metrics() {
    editor_timeline_metrics metrics;
    metrics.panel_rect = {0.0f, 0.0f, 724.0f, 620.0f};
    metrics.bottom_tick = 0.0f;
    metrics.ticks_per_pixel = 2.0f;
    metrics.key_count = 4;
    return metrics;
}

}  // namespace

int main() {
    const auto state = std::make_shared<editor_state>(make_chart(), "");
    editor_meter_map meter_map;
    meter_map.rebuild(state->data());

    {
        editor_timing_panel_state timing_panel;
        const editor_timeline_metrics metrics = make_metrics();
        const Rectangle lane_rect = metrics.lane_rect(1);
        const float y = metrics.tick_to_y(480);
        const editor_timeline_result result = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane_rect.x + lane_rect.width * 0.5f, y}, true,
             false, false, false, true, false, false, 4, std::nullopt, {}});
        if (!result.note_to_delete_index.has_value() || *result.note_to_delete_index != 0) {
            std::cerr << "right click should request deleting the note under the cursor\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_timing_panel_state timing_panel;
        const editor_timeline_metrics metrics = make_metrics();
        const Rectangle lane_rect = metrics.lane_rect(0);
        const float y = metrics.tick_to_y(720);
        const editor_timeline_result result = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane_rect.x + lane_rect.width * 0.5f, y}, true,
             true, false, false, false, false, true, 8, std::nullopt, {}});
        if (!result.request_seek || result.seek_tick != 720 || !result.scroll_seek_if_paused) {
            std::cerr << "alt+left click should request seek to snapped tick\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_timing_panel_state timing_panel;
        timing_panel.bar_pick_mode = true;
        timing_panel.selected_event_index = 0;
        const editor_timeline_metrics metrics = make_metrics();
        const Rectangle lane_rect = metrics.lane_rect(0);
        const float y = metrics.tick_to_y(960);
        const editor_timeline_result result = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane_rect.x + lane_rect.width * 0.5f, y}, true,
             true, false, false, false, false, false, 4, std::nullopt, {}});
        if (!result.request_apply_selected_timing || timing_panel.inputs.bpm_bar.value.empty() ||
            timing_panel.bar_pick_mode) {
            std::cerr << "bar pick mode should write bar input and request apply\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_timing_panel_state timing_panel;
        const editor_timeline_metrics metrics = make_metrics();
        const Rectangle lane_rect = metrics.lane_rect(2);
        const float start_y = metrics.tick_to_y(240);
        editor_timeline_note_drag_state drag_state;
        editor_timeline_result start = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane_rect.x + lane_rect.width * 0.5f, start_y}, true,
             true, false, false, false, false, false, 8, std::nullopt, drag_state});
        editor_timeline_result finish = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane_rect.x + lane_rect.width * 0.5f, metrics.tick_to_y(720)}, true,
             false, true, true, false, false, false, 8, std::nullopt, start.drag_state});
        if (!finish.note_to_add.has_value() || finish.note_to_add->lane != 2 ||
            finish.note_to_add->tick != 240 || finish.note_to_add->end_tick != 720) {
            std::cerr << "drag release should request a new note\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "editor_timeline_controller smoke test passed\n";
    return EXIT_SUCCESS;
}
