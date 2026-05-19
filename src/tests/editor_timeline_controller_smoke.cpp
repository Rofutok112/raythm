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
    data.scroll_events = {
        {scroll_event_type::speed, 360, 480, 1.5f},
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
        const Rectangle content = metrics.content_rect();
        const float y = metrics.tick_to_y(700);
        const editor_timeline_result result = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {content.x + 12.0f, y}, true,
             true, false, false, false, false, false, 8, std::nullopt, {}});
        if (!result.selected_scroll_event_index.has_value() || *result.selected_scroll_event_index != 0 ||
            result.selected_note_index.has_value()) {
            std::cerr << "left click on a scroll event band should select the scroll event\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_timing_panel_state timing_panel;
        timing_panel.selected_scroll_event_index = 0;
        const editor_timeline_metrics metrics = make_metrics();
        const Rectangle content = metrics.content_rect();
        const float start_y = metrics.tick_to_y(360);
        editor_timeline_note_drag_state drag_state;
        editor_timeline_result start = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {content.x + 12.0f, start_y}, true,
             true, false, false, false, false, false, 8, std::nullopt, drag_state});
        editor_timeline_result finish = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {content.x + 12.0f, metrics.tick_to_y(480)}, true,
             false, true, true, false, false, false, 8, std::nullopt, start.drag_state});
        if (!finish.scroll_event_to_modify_index.has_value() || *finish.scroll_event_to_modify_index != 0 ||
            !finish.scroll_event_to_modify.has_value() ||
            finish.scroll_event_to_modify->tick != 480 ||
            finish.scroll_event_to_modify->duration != 360) {
            std::cerr << "scroll event start handle should resize the event\n";
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
        const editor_timeline_metrics metrics = make_metrics();
        const Rectangle lane_rect = metrics.lane_rect(1);
        const float y = metrics.tick_to_y(480);
        const editor_timeline_result result = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane_rect.x + lane_rect.width * 0.5f, y}, true,
             true, false, false, false, false, false, 4, std::nullopt, {}});
        if (!result.selected_note_index.has_value() || *result.selected_note_index != 0 ||
            result.note_to_add.has_value()) {
            std::cerr << "left click should select the note under the cursor\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_timing_panel_state timing_panel;
        const editor_timeline_metrics metrics = make_metrics();
        const editor_timeline_note selected_note{editor_timeline_note_type::tap, 480, 1, 480, false, 1};
        const editor_timeline_note_draw_info info = metrics.note_rects(selected_note);
        const Rectangle lane2_rect = metrics.lane_rect(2);
        editor_timeline_note_drag_state drag_state;
        editor_timeline_result start = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics,
             {info.right_resize_rect.x + info.right_resize_rect.width * 0.5f,
              info.right_resize_rect.y + info.right_resize_rect.height * 0.5f},
             true, true, false, false, false, false, false, 4, std::optional<size_t>(0), drag_state});
        editor_timeline_result finish = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics,
             {lane2_rect.x + lane2_rect.width * 0.5f, metrics.tick_to_y(480)},
             true, false, true, true, false, false, false, 4, start.selected_note_index, start.drag_state});
        if (!finish.note_to_modify_index.has_value() || *finish.note_to_modify_index != 0 ||
            !finish.note_to_modify.has_value() || finish.note_to_modify->lane != 1 ||
            finish.note_to_modify->lane_width != 2) {
            std::cerr << "right resize handle should expand the selected note\n";
            return EXIT_FAILURE;
        }
    }

    {
        chart_data hold_chart = make_chart();
        hold_chart.notes = {
            {note_type::hold, 480, 1, 720},
        };
        const auto hold_state = std::make_shared<editor_state>(hold_chart, "");
        editor_meter_map hold_meter_map;
        hold_meter_map.rebuild(hold_state->data());
        editor_timing_panel_state timing_panel;
        const editor_timeline_metrics metrics = make_metrics();
        const editor_timeline_note selected_note{editor_timeline_note_type::hold, 480, 1, 720, false, 1};
        const editor_timeline_note_draw_info info = metrics.note_rects(selected_note);
        editor_timeline_note_drag_state drag_state;
        editor_timeline_result start = editor_timeline_controller::update(
            timing_panel,
            {hold_state.get(), &hold_meter_map, metrics,
             {info.end_resize_rect.x + info.end_resize_rect.width * 0.5f,
              info.end_resize_rect.y + info.end_resize_rect.height * 0.5f},
             true, true, false, false, false, false, false, 8, std::optional<size_t>(0), drag_state});
        editor_timeline_result finish = editor_timeline_controller::update(
            timing_panel,
            {hold_state.get(), &hold_meter_map, metrics,
             {info.end_resize_rect.x + info.end_resize_rect.width * 0.5f, metrics.tick_to_y(960)},
             true, false, true, true, false, false, false, 8, start.selected_note_index, start.drag_state});
        if (!finish.note_to_modify_index.has_value() || *finish.note_to_modify_index != 0 ||
            !finish.note_to_modify.has_value() || finish.note_to_modify->tick != 480 ||
            finish.note_to_modify->end_tick != 960 || finish.note_to_modify->type != note_type::hold) {
            std::cerr << "hold top resize handle should adjust the end tick\n";
            return EXIT_FAILURE;
        }
    }

    {
        chart_data hold_chart = make_chart();
        hold_chart.notes = {
            {note_type::hold, 480, 1, 720},
        };
        const auto hold_state = std::make_shared<editor_state>(hold_chart, "");
        editor_meter_map hold_meter_map;
        hold_meter_map.rebuild(hold_state->data());
        editor_timing_panel_state timing_panel;
        editor_timeline_metrics metrics = make_metrics();
        metrics.ticks_per_pixel = 10.0f;
        const editor_timeline_note selected_note{editor_timeline_note_type::hold, 480, 1, 720, false, 1};
        const editor_timeline_note_draw_info info = metrics.note_rects(selected_note);
        editor_timeline_result start = editor_timeline_controller::update(
            timing_panel,
            {hold_state.get(), &hold_meter_map, metrics,
             {info.start_resize_rect.x + info.start_resize_rect.width * 0.5f,
              info.start_resize_rect.y + info.start_resize_rect.height * 0.5f},
             true, true, false, false, false, false, false, 16, std::optional<size_t>(0), {}});
        editor_timeline_result finish = editor_timeline_controller::update(
            timing_panel,
            {hold_state.get(), &hold_meter_map, metrics,
             {info.start_resize_rect.x + info.start_resize_rect.width * 0.5f, metrics.tick_to_y(360)},
             true, false, true, true, false, false, false, 16, start.selected_note_index, start.drag_state});
        if (!finish.note_to_modify_index.has_value() || *finish.note_to_modify_index != 0 ||
            !finish.note_to_modify.has_value() || finish.note_to_modify->tick != 360 ||
            finish.note_to_modify->end_tick != 720 || finish.note_to_modify->type != note_type::hold) {
            std::cerr << "hold bottom resize handle should adjust only the start tick\n";
            return EXIT_FAILURE;
        }
    }

    {
        chart_data hold_chart = make_chart();
        hold_chart.notes = {
            {note_type::hold, 480, 1, 720},
        };
        const auto hold_state = std::make_shared<editor_state>(hold_chart, "");
        editor_meter_map hold_meter_map;
        hold_meter_map.rebuild(hold_state->data());
        editor_timing_panel_state timing_panel;
        editor_timeline_metrics metrics = make_metrics();
        metrics.ticks_per_pixel = 10.0f;
        const editor_timeline_note selected_note{editor_timeline_note_type::hold, 480, 1, 720, false, 1};
        const editor_timeline_note_draw_info info = metrics.note_rects(selected_note);
        const Vector2 body_center = {
            info.body_rect.x + info.body_rect.width * 0.5f,
            info.body_rect.y + info.body_rect.height * 0.5f,
        };
        editor_timeline_result start = editor_timeline_controller::update(
            timing_panel,
            {hold_state.get(), &hold_meter_map, metrics, body_center,
             true, true, false, false, false, false, false, 16, std::optional<size_t>(0), {}});
        if (start.drag_state.original_note.type != note_type::hold ||
            start.drag_state.original_note.tick != 480 ||
            start.drag_state.original_note.end_tick != 720) {
            std::cerr << "hold move preview should preserve the original hold note\n";
            return EXIT_FAILURE;
        }
        editor_timeline_result finish = editor_timeline_controller::update(
            timing_panel,
            {hold_state.get(), &hold_meter_map, metrics,
             {body_center.x, metrics.tick_to_y(600)},
             true, false, true, true, false, false, false, 16, start.selected_note_index, start.drag_state});
        if (finish.notes_to_modify.empty() || finish.notes_to_modify.front().first != 0 ||
            finish.notes_to_modify.front().second.tick != 600 ||
            finish.notes_to_modify.front().second.end_tick != 840) {
            std::cerr << "hold body drag should move the whole note instead of resizing it\n";
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
        const Rectangle lane2_rect = metrics.lane_rect(2);
        const Rectangle lane3_rect = metrics.lane_rect(3);
        const float start_y = metrics.tick_to_y(240);
        editor_timeline_note_drag_state drag_state;
        editor_timeline_result start = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane2_rect.x + lane2_rect.width * 0.5f, start_y}, true,
             true, false, false, false, false, false, 8, std::nullopt, drag_state,
             {note_type::hold, false}});
        editor_timeline_result finish = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane3_rect.x + lane3_rect.width * 0.5f, metrics.tick_to_y(720)}, true,
             false, true, true, false, false, false, 8, std::nullopt, start.drag_state,
             {note_type::hold, false}});
        if (!finish.note_to_add.has_value() || finish.note_to_add->lane != 2 ||
            finish.note_to_add->lane_width != 2 ||
            finish.note_to_add->tick != 240 || finish.note_to_add->end_tick != 720) {
            std::cerr << "drag release should request a new wide note\n";
            return EXIT_FAILURE;
        }
    }

    {
        editor_timing_panel_state timing_panel;
        const editor_timeline_metrics metrics = make_metrics();
        const Rectangle lane_rect = metrics.lane_rect(3);
        const float y = metrics.tick_to_y(840);
        editor_timeline_note_drag_state drag_state;
        editor_timeline_result start = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane_rect.x + lane_rect.width * 0.5f, y}, true,
             true, false, false, false, false, false, 8, std::nullopt, drag_state,
             {note_type::release, true}});
        editor_timeline_result finish = editor_timeline_controller::update(
            timing_panel,
            {state.get(), &meter_map, metrics, {lane_rect.x + lane_rect.width * 0.5f, y}, true,
             false, true, true, false, false, false, 8, std::nullopt, start.drag_state,
             {note_type::release, true}});
        if (!finish.note_to_add.has_value() || finish.note_to_add->type != note_type::release ||
            !finish.note_to_add->is_ray || finish.note_to_add->tick != finish.note_to_add->end_tick) {
            std::cerr << "palette should create a ray release note without converting it to tap\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "editor_timeline_controller smoke test passed\n";
    return EXIT_SUCCESS;
}
