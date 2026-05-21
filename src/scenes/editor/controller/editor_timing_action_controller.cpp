#include "editor/controller/editor_timing_action_controller.h"

#include "editor/service/editor_timing_selection_service.h"
#include "editor/service/editor_timing_edit_service.h"
#include "editor/service/editor_transport_service.h"

namespace {

editor_scene_sync_context sync_context(const editor_timing_action_controller::context& context) {
    return {
        context.state,
        context.meter_map,
        context.timing_panel,
        context.metadata_panel,
        context.selected_note_indices,
    };
}

void scroll_to_tick(const editor_timing_action_controller::context& context, int tick) {
    context.viewport = editor_timeline_viewport::scroll_to_tick(context.viewport_model, tick);
}

void select_timing_event(const editor_timing_action_controller::context& context,
                         std::optional<size_t> index,
                         bool scroll_into_view) {
    if (scroll_into_view && index.has_value() && *index < context.state.data().timing_events.size()) {
        scroll_to_tick(context, context.state.data().timing_events[*index].tick);
    }
    context.timing_panel.selected_scroll_event_index.reset();
    editor_scene_sync_context sync = sync_context(context);
    editor_timing_selection_service::select_event(
        sync,
        index,
        scroll_into_view,
        context.timing_panel.list_scroll_offset);
}

void select_scroll_event(const editor_timing_action_controller::context& context,
                         std::optional<size_t> index,
                         bool scroll_into_view) {
    context.timing_panel.selected_event_index.reset();
    context.timing_panel.selected_scroll_event_index = index;
    context.timing_panel.active_input_field = editor_timing_input_field::none;
    context.timing_panel.input_error.clear();
    context.timing_panel.bar_pick_mode = false;
    editor_scene_sync::load_scroll_event_inputs(sync_context(context));
    if (scroll_into_view && index.has_value() && *index < context.state.data().scroll_automation.size()) {
        scroll_to_tick(context, context.state.data().scroll_automation[*index].tick);
    }
}

}  // namespace

namespace editor_timing_action_controller {

int default_timing_event_tick(const context& context) {
    return editor_timeline_viewport::default_timing_event_tick(
        context.viewport_model,
        context.timing_panel.selected_event_index);
}

bool apply_selected_timing_event(const context& context) {
    editor_scene_sync::sync_timing_event_selection(sync_context(context));
    const editor_timing_edit_result result = editor_timing_edit_service::apply_selected({
        context.state,
        context.meter_map,
        context.timing_panel,
        default_timing_event_tick(context),
    });
    if (!result.success) {
        return false;
    }
    editor_scene_sync::sync_after_timing_change(sync_context(context));
    editor_transport_service::sync(
        context.transport,
        &context.state,
        context.hitsound_path,
        &context.hitsounds,
        true);
    if (result.scroll_to_tick.has_value()) {
        scroll_to_tick(context, *result.scroll_to_tick);
    }
    return true;
}

bool apply_selected_scroll_event(const context& context) {
    const editor_timing_edit_result result = editor_timing_edit_service::apply_selected_scroll({
        context.state,
        context.meter_map,
        context.timing_panel,
        default_timing_event_tick(context),
    });
    if (!result.success) {
        return false;
    }
    editor_scene_sync::load_scroll_event_inputs(sync_context(context));
    if (result.scroll_to_tick.has_value()) {
        scroll_to_tick(context, *result.scroll_to_tick);
    }
    if (result.selected_scroll_event_index.has_value()) {
        select_scroll_event(context, result.selected_scroll_event_index, false);
    }
    return true;
}

void cycle_selected_scroll_curve(const context& context) {
    const editor_timing_edit_result result = editor_timing_edit_service::cycle_selected_scroll_curve({
        context.state,
        context.meter_map,
        context.timing_panel,
        default_timing_event_tick(context),
    });
    if (result.success) {
        select_scroll_event(context, result.selected_scroll_event_index, false);
    }
}

void add_timing_event(const context& context, timing_event_type type) {
    const editor_timing_edit_result result = editor_timing_edit_service::add_event({
        context.state,
        context.meter_map,
        context.timing_panel,
        default_timing_event_tick(context),
    }, type);
    editor_scene_sync::sync_after_timing_change(sync_context(context));
    editor_transport_service::sync(
        context.transport,
        &context.state,
        context.hitsound_path,
        &context.hitsounds,
        true);
    if (result.selected_event_index.has_value()) {
        select_timing_event(context, result.selected_event_index, true);
    }
}

void add_scroll_event(const context& context) {
    const editor_timing_edit_result result = editor_timing_edit_service::add_scroll_event({
        context.state,
        context.meter_map,
        context.timing_panel,
        default_timing_event_tick(context),
    });
    if (result.selected_scroll_event_index.has_value()) {
        select_scroll_event(context, result.selected_scroll_event_index, true);
    }
}

void delete_selected_timing_event(const context& context) {
    editor_scene_sync::sync_timing_event_selection(sync_context(context));
    const editor_timing_edit_result result = editor_timing_edit_service::delete_selected({
        context.state,
        context.meter_map,
        context.timing_panel,
        default_timing_event_tick(context),
    });
    if (!result.success) {
        return;
    }
    editor_scene_sync::sync_after_timing_change(sync_context(context));
    editor_transport_service::sync(
        context.transport,
        &context.state,
        context.hitsound_path,
        &context.hitsounds,
        true);
}

void delete_selected_scroll_event(const context& context) {
    editor_scene_sync::sync_timing_event_selection(sync_context(context));
    const editor_timing_edit_result result = editor_timing_edit_service::delete_selected_scroll({
        context.state,
        context.meter_map,
        context.timing_panel,
        default_timing_event_tick(context),
    });
    if (!result.success) {
        return;
    }
    context.timing_panel.selected_scroll_event_index.reset();
    editor_scene_sync::load_scroll_event_inputs(sync_context(context));
    context.timing_panel.input_error.clear();
}

bool can_delete_selected_timing_event(const editor_state& state, const editor_timing_panel_state& timing_panel) {
    return editor_timing_edit_service::can_delete_selected({state, timing_panel});
}

bool can_delete_selected_scroll_event(const editor_state& state, const editor_timing_panel_state& timing_panel) {
    return editor_timing_edit_service::can_delete_selected_scroll({state, timing_panel});
}

}  // namespace editor_timing_action_controller
