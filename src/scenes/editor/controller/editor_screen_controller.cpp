#include "editor/controller/editor_screen_controller.h"

#include <algorithm>

#include "editor/editor_panel_controller.h"
#include "editor/service/editor_timing_edit_service.h"
#include "editor/service/editor_transport_service.h"
#include "editor/view/editor_screen_view.h"

namespace {

void apply_timeline_panel_result(const editor_screen_controller::context& context,
                                 const editor_right_panel_view_result& timeline_panel) {
    editor_scene_sync::sync_timing_event_selection(context.make_sync_context());
    if (timeline_panel.scroll_automation_point_to_add.has_value()) {
        const editor_timing_edit_result result = editor_timing_edit_service::add_scroll_event(
            {context.state,
             context.meter_map,
             context.timing_panel,
             editor_timing_action_controller::default_timing_event_tick(context.timing_action_context())},
            *timeline_panel.scroll_automation_point_to_add);
        if (result.success) {
            context.select_scroll_event(result.selected_scroll_event_index, false);
        }
    }
    if (timeline_panel.scroll_automation_point_to_modify.has_value()) {
        const editor_timing_edit_result result = editor_timing_edit_service::modify_scroll_event(
            {context.state,
             context.meter_map,
             context.timing_panel,
             editor_timing_action_controller::default_timing_event_tick(context.timing_action_context())},
            timeline_panel.scroll_automation_point_to_modify->first,
            timeline_panel.scroll_automation_point_to_modify->second);
        if (result.success) {
            context.select_scroll_event(result.selected_scroll_event_index, false);
        }
    }
    if (timeline_panel.scroll_automation_guides_to_modify.has_value()) {
        editor_timing_edit_service::modify_scroll_guides(
            {context.state,
             context.meter_map,
             context.timing_panel,
             editor_timing_action_controller::default_timing_event_tick(context.timing_action_context())},
            *timeline_panel.scroll_automation_guides_to_modify);
    }
}

void apply_timing_panel_result(const editor_screen_controller::context& context,
                               const editor_timing_panel_update_result& update_result) {
    if (update_result.select_timing_event_index.has_value()) {
        context.select_timing_event(update_result.select_timing_event_index, true);
    }
    if (update_result.select_scroll_event_index.has_value()) {
        context.select_scroll_event(update_result.select_scroll_event_index, false);
    }
    if (update_result.request_add_bpm) {
        editor_timing_action_controller::add_timing_event(context.timing_action_context(), timing_event_type::bpm);
    }
    if (update_result.request_add_meter) {
        editor_timing_action_controller::add_timing_event(context.timing_action_context(), timing_event_type::meter);
    }
    if (update_result.request_add_speed) {
        editor_timing_action_controller::add_scroll_event(context.timing_action_context());
    }
    if (update_result.request_add_stop) {
        editor_timing_action_controller::cycle_selected_scroll_curve(context.timing_action_context());
    }
    if (update_result.request_delete_selected) {
        editor_timing_action_controller::delete_selected_timing_event(context.timing_action_context());
    }
    if (update_result.request_delete_selected_scroll) {
        editor_timing_action_controller::delete_selected_scroll_event(context.timing_action_context());
    }
    if (update_result.request_apply_selected) {
        editor_timing_action_controller::apply_selected_timing_event(context.timing_action_context());
    }
    if (update_result.request_apply_selected_scroll) {
        editor_timing_action_controller::apply_selected_scroll_event(context.timing_action_context());
    }
    if (update_result.request_cycle_selected_scroll_curve) {
        editor_timing_action_controller::cycle_selected_scroll_curve(context.timing_action_context());
    }
}

void draw_metadata_modal(const editor_screen_controller::context& context, double now) {
    const editor::daw::metadata_modal_result modal_result = editor_screen_view::draw_metadata_modal({
        context.song,
        context.state,
        context.metadata_panel,
        context.note_palette,
        context.load_errors,
        now,
    });
    const editor_metadata_panel_result metadata_panel_result = editor_panel_controller::update_metadata_panel(
        context.metadata_panel,
        context.timing_panel,
        {
            modal_result.metadata_result.difficulty_result.activated ||
                modal_result.metadata_result.author_result.activated,
            modal_result.metadata_result.difficulty_result.submitted ||
                modal_result.metadata_result.author_result.submitted ||
                modal_result.apply_requested,
            modal_result.metadata_result.key_count_left_clicked ||
                modal_result.metadata_result.key_count_right_clicked,
        });
    if (metadata_panel_result.request_apply_metadata) {
        context.apply_metadata_changes(false);
    }
    if (modal_result.close_requested) {
        context.metadata_modal_open = false;
        context.metadata_panel.difficulty_input.active = false;
        context.metadata_panel.chart_author_input.active = false;
    }
}

void draw_timing_modal(const editor_screen_controller::context& context, const char* offset_label) {
    const editor::daw::timing_modal_result modal_result = editor_screen_view::draw_timing_modal({
        context.state,
        context.meter_map,
        context.timing_panel,
        context.selected_note_indices,
        editor_timing_action_controller::can_delete_selected_timing_event(context.state, context.timing_panel),
        editor_timing_action_controller::can_delete_selected_scroll_event(context.state, context.timing_panel),
        offset_label,
    });
    apply_timing_panel_result(
        context,
        editor_panel_controller::update_timing_panel(
            context.metadata_panel,
            context.timing_panel,
            {modal_result.panel_result, false}));
    if (modal_result.offset_left_clicked) {
        context.apply_chart_offset(std::max(-10000, context.state.data().meta.offset - 5));
    } else if (modal_result.offset_right_clicked) {
        context.apply_chart_offset(std::min(10000, context.state.data().meta.offset + 5));
    }
    if (modal_result.close_requested) {
        context.timing_modal_open = false;
        context.timing_panel.active_input_field = editor_timing_input_field::none;
        context.timing_panel.bar_pick_mode = false;
    }
}

}  // namespace

namespace editor_screen_controller {

void draw_and_update(const context& context) {
    const double now = GetTime();
    editor_screen_view::begin_frame({context.rebuild_hit_regions});

    const editor_left_panel_view_result left_panel = editor_screen_view::draw_left_panel({
        context.song,
        context.state,
        context.metadata_panel,
        context.note_palette,
        context.load_errors,
        now,
    });
    if (left_panel.selected_note_type.has_value()) {
        context.note_palette.type = *left_panel.selected_note_type;
    }
    if (left_panel.ray_toggled) {
        context.note_palette.is_ray = !context.note_palette.is_ray;
    }
    const editor_metadata_panel_result metadata_panel_result = editor_panel_controller::update_metadata_panel(
        context.metadata_panel,
        context.timing_panel,
        {
            left_panel.difficulty_result.activated || left_panel.author_result.activated,
            left_panel.difficulty_result.submitted || left_panel.author_result.submitted,
            left_panel.key_count_left_clicked || left_panel.key_count_right_clicked,
        });
    if (metadata_panel_result.request_apply_metadata) {
        context.apply_metadata_changes(false);
    }

    const editor_right_panel_view_result timeline_panel = editor_screen_view::draw_timeline(context.draw_timeline);
    apply_timeline_panel_result(context, timeline_panel);
    apply_timing_panel_result(
        context,
        editor_panel_controller::update_timing_panel(
            context.metadata_panel,
            context.timing_panel,
            {timeline_panel.panel_result, timeline_panel.clicked_outside_editor}));

    const std::string offset_label =
        (context.state.data().meta.offset > 0 ? "+" : "") + std::to_string(context.state.data().meta.offset) + " ms";
    const editor_header_view_result header_result = editor_screen_view::draw_header({
        context.transport,
        context.viewport,
        context.snap_dropdown_open,
    });
    if (header_result.metadata_modal_requested) {
        context.metadata_modal_open = true;
    }
    if (header_result.timing_modal_requested) {
        context.timing_modal_open = true;
    }
    if (header_result.snap_dropdown_toggled) {
        context.snap_dropdown_open = !context.snap_dropdown_open;
    }
    if (header_result.snap_index_clicked >= 0) {
        context.viewport.snap_index = header_result.snap_index_clicked;
        context.snap_dropdown_open = false;
    } else if (header_result.snap_dropdown_close_requested) {
        context.snap_dropdown_open = false;
    }

    if (context.unsaved_changes_dialog.open) {
        editor_screen_view::draw_unsaved_changes_dialog();
    }
    if (context.save_dialog.open) {
        context.save_dialog.submit_requested =
            context.save_dialog.submit_requested || editor_screen_view::draw_save_dialog(context.save_dialog);
    }
    if (context.metadata_modal_open) {
        draw_metadata_modal(context, now);
    }
    if (context.timing_modal_open) {
        draw_timing_modal(context, offset_label.c_str());
    }
    if (context.metadata_panel.key_count_confirm_open) {
        editor_screen_view::draw_key_count_confirmation(context.metadata_panel.pending_key_count);
    }

    editor_screen_view::end_frame();
}

}  // namespace editor_screen_controller
