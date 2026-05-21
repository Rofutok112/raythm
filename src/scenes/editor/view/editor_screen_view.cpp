#include "editor/view/editor_screen_view.h"

#include "editor/view/editor_layout.h"
#include "editor/view/editor_modal_view.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "scene_common.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

Rectangle snap_dropdown_menu_rect() {
    return editor::layout::snap_dropdown_menu_rect(
        static_cast<int>(editor_timeline_viewport::snap_labels().size()));
}

std::string selected_note_summary(const std::vector<size_t>& selected_note_indices) {
    if (selected_note_indices.empty()) {
        return "No notes selected";
    }
    return selected_note_indices.size() == 1
        ? "1 note selected"
        : TextFormat("%d notes selected", static_cast<int>(selected_note_indices.size()));
}

}  // namespace

namespace editor_screen_view {

void begin_frame(const frame_context& context) {
    virtual_screen::begin_ui();
    context.rebuild_hit_regions();
    ui::begin_draw_queue();
    draw_scene_background(*g_theme);
}

editor_left_panel_view_result draw_left_panel(const left_panel_context& context) {
    return editor::daw::draw_left_panel({
        context.song.meta.title.c_str(),
        context.state.data().meta.level,
        !context.state.file_path().empty(),
        context.state.is_dirty(),
        &context.metadata_panel,
        context.note_palette,
        context.load_errors.empty() ? nullptr : &context.load_errors.front(),
        context.now,
    });
}

editor_right_panel_view_result draw_timeline(
    const std::function<editor_right_panel_view_result()>& draw_timeline) {
    return draw_timeline();
}

editor_header_view_result draw_header(const header_context& context) {
    return editor::daw::draw_header({
        "",
        context.transport.audio_loaded,
        context.transport.audio_playing || context.transport.pre_audio_playing,
        context.offset_label,
        context.waveform_visible,
        editor_timeline_viewport::snap_labels(),
        context.viewport.snap_index,
        context.snap_dropdown_open,
    }, snap_dropdown_menu_rect());
}

bool draw_save_dialog(save_dialog_state& save_dialog) {
    const editor_modal_view_result modal_result = editor_modal_view::draw_save_dialog(save_dialog);
    return modal_result.save_dialog_submit_requested;
}

void draw_unsaved_changes_dialog() {
    editor_modal_view::draw_unsaved_changes_dialog();
}

editor::daw::metadata_modal_result draw_metadata_modal(const metadata_modal_context& context) {
    return editor::daw::draw_metadata_modal({
        context.song.meta.title.c_str(),
        context.state.data().meta.level,
        !context.state.file_path().empty(),
        context.state.is_dirty(),
        &context.metadata_panel,
        context.note_palette,
        context.load_errors.empty() ? nullptr : &context.load_errors.front(),
        context.now,
    });
}

editor::daw::timing_modal_result draw_timing_modal(const timing_modal_context& context) {
    return editor::daw::draw_timing_modal({
        &context.state.data().timing_events,
        &context.state.data().scroll_automation,
        &context.state.data().scroll_guides,
        &context.meter_map,
        context.timing_panel.selected_event_index,
        context.timing_panel.selected_scroll_event_index,
        context.selected_note_indices.size(),
        selected_note_summary(context.selected_note_indices),
        context.can_delete_selected_timing_event,
        context.can_delete_selected_scroll_event,
        virtual_screen::get_virtual_mouse(),
    }, context.timing_panel, context.offset_label);
}

void draw_key_count_confirmation(int pending_key_count) {
    editor_modal_view::draw_key_count_confirmation(pending_key_count);
}

void end_frame() {
    ui::flush_draw_queue();
    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

}  // namespace editor_screen_view
