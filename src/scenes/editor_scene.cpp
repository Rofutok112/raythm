#include "editor_scene.h"

#include <memory>

#include "audio_manager.h"
#include "editor/controller/editor_runtime_controller.h"
#include "editor/view/editor_cursor_hud_view.h"
#include "editor/editor_flow_controller.h"
#include "editor/service/editor_chart_identity_service.h"
#include "editor/service/editor_metadata_service.h"
#include "editor/service/editor_transport_service.h"
#include "editor/service/editor_timing_edit_service.h"
#include "editor/service/editor_timing_selection_service.h"
#include "editor/view/editor_header_view.h"
#include "editor/view/editor_layout.h"
#include "editor/view/editor_left_panel_view.h"
#include "editor/view/editor_modal_view.h"
#include "editor/view/editor_right_panel_view.h"
#include "editor/view/editor_timeline_presenter.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "editor/editor_session_loader.h"
#include "play_scene.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {
namespace layout = editor::layout;

Rectangle snap_dropdown_menu_rect() {
    return layout::snap_dropdown_menu_rect(static_cast<int>(editor_timeline_viewport::snap_labels().size()));
}

}

editor_scene::editor_scene(scene_manager& manager, song_data song, std::string chart_path)
    : scene(manager), song_(std::move(song)), chart_path_(std::move(chart_path)), state_(std::make_shared<editor_state>()) {
}

editor_scene::editor_scene(scene_manager& manager, song_data song, int key_count)
    : scene(manager), song_(std::move(song)), chart_path_(std::nullopt),
      new_chart_key_count_(key_count), state_(std::make_shared<editor_state>()) {
}

editor_scene::editor_scene(scene_manager& manager, song_data song, chart_meta initial_meta)
    : scene(manager), song_(std::move(song)), chart_path_(std::nullopt),
      initial_meta_(std::move(initial_meta)),
      new_chart_key_count_(initial_meta_.has_value() ? initial_meta_->key_count : 4),
      state_(std::make_shared<editor_state>()) {
}

editor_scene::editor_scene(scene_manager& manager, song_data song, editor_resume_state resume)
    : scene(manager), song_(std::move(song)), chart_path_(std::nullopt),
      new_chart_key_count_(resume.state ? resume.state->data().meta.key_count : 4),
      state_(resume.state ? resume.state : std::make_shared<editor_state>()),
      resume_state_(std::move(resume)) {
}

void editor_scene::on_enter() {
    const editor_session_load_result load_result = editor_session_loader::load({
        song_,
        chart_path_,
        initial_meta_,
        new_chart_key_count_,
        state_,
        resume_state_,
    });

    state_ = load_result.state;
    chart_path_ = load_result.chart_path;
    meter_map_ = load_result.meter_map;
    timing_panel_ = load_result.timing_panel;
    metadata_panel_ = load_result.metadata_panel;
    save_dialog_ = load_result.save_dialog;
    unsaved_changes_dialog_ = load_result.unsaved_changes_dialog;
    load_errors_ = load_result.load_errors;
    transport_.audio_length_tick = load_result.audio_length_tick;
    transport_.audio_loaded = load_result.audio_loaded;
    transport_.audio_playing = load_result.audio_playing;
    transport_.audio_time_seconds = load_result.audio_time_seconds;
    transport_.playback_tick = load_result.playback_tick;
    transport_.previous_playback_tick = load_result.previous_playback_tick;
    transport_.previous_audio_playing = load_result.previous_audio_playing;
    hitsound_path_ = load_result.hitsound_path;
    waveform_visible_ = load_result.waveform_visible;
    waveform_offset_ms_ = load_result.waveform_offset_ms;
    waveform_summary_ = load_result.waveform_summary;
    viewport_.bottom_tick = resume_state_.has_value() ? load_result.bottom_tick : editor_timeline_viewport::min_bottom_tick();
    viewport_.bottom_tick_target = resume_state_.has_value() ? load_result.bottom_tick_target : viewport_.bottom_tick;
    viewport_.ticks_per_pixel = load_result.ticks_per_pixel;
    viewport_.snap_index = load_result.snap_index;
    viewport_.scrollbar_dragging = false;
    viewport_.scrollbar_drag_offset = 0.0f;
    snap_dropdown_open_ = false;
    selected_note_index_ = load_result.selected_note_index;
    timeline_drag_ = {};
    resume_state_.reset();
}

void editor_scene::on_exit() {
    audio_manager::instance().stop_bgm();
    audio_manager::instance().stop_all_se();
}

void editor_scene::update(float dt) {
    rebuild_hit_regions();
    editor_transport_service::sync(transport_, state_.get(), hitsound_path_);

    const chart_data chart_for_save = make_chart_data_for_save();
    const bool save_dialog_submit = save_dialog_.submit_requested ||
        (save_dialog_.open && ui::is_clicked(layout::save_submit_button_rect(), ui::draw_layer::modal));
    save_dialog_.submit_requested = false;

    const editor_flow_result flow_result = editor_flow_controller::update({
        &song_,
        &chart_for_save,
        state_,
        &metadata_panel_,
        &save_dialog_,
        &unsaved_changes_dialog_,
        IsKeyPressed(KEY_ESCAPE),
        ui::is_clicked(layout::kBackButtonRect),
        (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S),
        IsKeyPressed(KEY_F5),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
        has_active_metadata_input(),
        timing_panel_.active_input_field != editor_timing_input_field::none,
        timing_panel_.bar_pick_mode,
        timeline_drag_.active,
        save_dialog_submit,
        save_dialog_.open && ui::is_clicked(layout::save_cancel_button_rect(), ui::draw_layer::modal),
        unsaved_changes_dialog_.open && ui::is_clicked(layout::unsaved_save_button_rect(), ui::draw_layer::modal),
        unsaved_changes_dialog_.open && ui::is_clicked(layout::unsaved_discard_button_rect(), ui::draw_layer::modal),
        unsaved_changes_dialog_.open && ui::is_clicked(layout::unsaved_cancel_button_rect(), ui::draw_layer::modal),
        metadata_panel_.key_count_confirm_open && ui::is_clicked(layout::key_count_confirm_button_rect(), ui::draw_layer::modal),
        metadata_panel_.key_count_confirm_open && ui::is_clicked(layout::key_count_cancel_button_rect(), ui::draw_layer::modal),
        transport_.playback_tick,
    });
    apply_flow_result(flow_result);
    if (flow_result.consume_update) {
        return;
    }

    const editor_shortcut_result shortcut_result = editor_runtime_controller::handle_shortcuts({
        *state_,
        make_sync_context(),
        metadata_panel_,
        timing_panel_,
        timeline_drag_,
        selected_note_index_,
        transport_,
        space_playback_start_tick_,
        hitsound_path_,
        has_blocking_modal(),
        false,
        IsKeyPressed(KEY_SPACE),
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
        IsKeyPressed(KEY_Z),
        IsKeyPressed(KEY_Y),
        IsKeyPressed(KEY_DELETE),
    });
    if (shortcut_result.restore_scroll_tick.has_value()) {
        scroll_to_tick(*shortcut_result.restore_scroll_tick);
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const editor_timeline_metrics metrics = timeline_metrics();
    const Rectangle content = metrics.content_rect();
    const editor_runtime_timeline_result timeline_result = editor_runtime_controller::handle_timeline_interaction({
        *state_,
        meter_map_,
        timing_panel_,
        transport_,
        space_playback_start_tick_,
        selected_note_index_,
        timeline_drag_,
        hitsound_path_,
        metrics,
        mouse,
        ui::is_hovered(content, ui::draw_layer::base),
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
        IsKeyPressed(KEY_ESCAPE),
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT),
        editor_timeline_viewport::snap_division(viewport_),
    });
    if (timeline_result.request_apply_selected_timing) {
        apply_selected_timing_event();
    }
    if (timeline_result.scroll_to_tick.has_value()) {
        scroll_to_tick(*timeline_result.scroll_to_tick);
    }

    editor_transport_service::sync(transport_, state_.get(), hitsound_path_);
    apply_scroll_and_zoom(dt);
}

void editor_scene::rebuild_hit_regions() const {
    ui::begin_hit_regions();
    if (snap_dropdown_open_) {
        ui::register_hit_region(snap_dropdown_menu_rect(), ui::draw_layer::overlay);
    }
    if (save_dialog_.open) {
        ui::register_hit_region(layout::kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(layout::kSaveDialogRect, ui::draw_layer::modal);
    }
    if (unsaved_changes_dialog_.open) {
        ui::register_hit_region(layout::kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(layout::kUnsavedChangesRect, ui::draw_layer::modal);
    }
    if (metadata_panel_.key_count_confirm_open) {
        ui::register_hit_region(layout::kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(layout::kMetadataConfirmRect, ui::draw_layer::modal);
    }
}

void editor_scene::draw() {
    const auto& t = *g_theme;
    const double now = GetTime();
    virtual_screen::begin();
    rebuild_hit_regions();
    ui::begin_draw_queue();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);

    ui::draw_panel(layout::kLeftPanelRect);
    ui::draw_panel(layout::kTimelineRect);
    ui::draw_panel(layout::kRightPanelRect);
    ui::draw_panel(layout::kHeaderRect);

    ui::draw_button_colored(layout::kBackButtonRect, "BACK", 20, t.row, t.row_hover, t.text);

    const editor_left_panel_view_result left_panel = editor_left_panel_view::draw({
        song_.meta.title.c_str(),
        !state_->file_path().empty(),
        state_->is_dirty(),
        &metadata_panel_,
        state_->data().meta.key_count == 6 ? "6K" : "4K",
        state_->data().meta.offset,
        static_cast<int>(state_->data().notes.size()),
        load_errors_.empty() ? nullptr : &load_errors_.front(),
        now,
    });
    const editor_metadata_panel_result metadata_panel_result = editor_panel_controller::update_metadata_panel(
        metadata_panel_,
        timing_panel_,
        {
            left_panel.difficulty_result.activated || left_panel.author_result.activated,
            left_panel.difficulty_result.submitted || left_panel.author_result.submitted,
            left_panel.key_count_left_clicked || left_panel.key_count_right_clicked,
        });
    if (metadata_panel_result.request_apply_metadata) {
        apply_metadata_changes(false);
    }

    draw_timeline();

    editor_scene_sync::sync_timing_event_selection(make_sync_context());
    const editor_right_panel_view_result right_panel = editor_right_panel_view::draw({
        &state_->data().timing_events,
        &meter_map_,
        timing_panel_.selected_event_index,
        can_delete_selected_timing_event(),
        virtual_screen::get_virtual_mouse(),
    }, timing_panel_);
    const editor_timing_panel_update_result update_result = editor_panel_controller::update_timing_panel(
        metadata_panel_,
        timing_panel_,
        {
            right_panel.panel_result,
            right_panel.clicked_outside_editor,
        });
    if (update_result.select_timing_event_index.has_value()) {
        select_timing_event(update_result.select_timing_event_index, true);
    }
    if (update_result.request_add_bpm) {
        add_timing_event(timing_event_type::bpm);
    }
    if (update_result.request_add_meter) {
        add_timing_event(timing_event_type::meter);
    }
    if (update_result.request_delete_selected) {
        delete_selected_timing_event();
    }
    if (update_result.request_apply_selected) {
        apply_selected_timing_event();
    }

    const std::string playback_status = editor_transport_service::playback_status_text(transport_);
    const std::string offset_label =
        (state_->data().meta.offset > 0 ? "+" : "") + std::to_string(state_->data().meta.offset) + " ms";
    const editor_header_view_result header_result = editor_header_view::draw({
        playback_status.c_str(),
        transport_.audio_loaded,
        offset_label.c_str(),
        waveform_visible_,
        editor_timeline_viewport::snap_labels(),
        viewport_.snap_index,
        snap_dropdown_open_,
    }, snap_dropdown_menu_rect());
    if (header_result.offset_left_clicked) {
        apply_chart_offset(std::max(-10000, state_->data().meta.offset - 5));
    } else if (header_result.offset_right_clicked) {
        apply_chart_offset(std::min(10000, state_->data().meta.offset + 5));
    }
    if (header_result.waveform_toggled) {
        waveform_visible_ = !waveform_visible_;
    }
    if (header_result.snap_dropdown_toggled) {
        snap_dropdown_open_ = !snap_dropdown_open_;
    }
    if (header_result.snap_index_clicked >= 0) {
        viewport_.snap_index = header_result.snap_index_clicked;
        snap_dropdown_open_ = false;
    } else if (header_result.snap_dropdown_close_requested) {
        snap_dropdown_open_ = false;
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool hud_visible = CheckCollisionPointRec(mouse, layout::kTimelineRect);
    if (hud_visible) {
        const int tick = std::max(0, timeline_metrics().y_to_tick(mouse.y));
        const editor_meter_map::bar_beat_position position = meter_map_.bar_beat_at_tick(tick);
        editor_cursor_hud_view::draw({
            true,
            editor_timeline_viewport::snap_tick(viewport_model(), tick),
            meter_map_.beat_number_at_tick(tick),
            position.measure,
            position.beat,
        });
    }

    if (unsaved_changes_dialog_.open) {
        editor_modal_view::draw_unsaved_changes_dialog();
    }
    if (save_dialog_.open) {
        const editor_modal_view_result modal_result = editor_modal_view::draw_save_dialog(save_dialog_);
        save_dialog_.submit_requested = save_dialog_.submit_requested || modal_result.save_dialog_submit_requested;
    }
    if (metadata_panel_.key_count_confirm_open) {
        editor_modal_view::draw_key_count_confirmation(metadata_panel_.pending_key_count);
    }

    ui::flush_draw_queue();
    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

chart_data editor_scene::make_chart_data_for_save() const {
    chart_data data = state_->data();
    if (state_->file_path().empty()) {
        data.meta.chart_id = generated_chart_id(data.meta.difficulty);
    }
    data.meta.song_id = song_.meta.song_id;
    return data;
}

std::string editor_scene::generated_chart_id(const std::string& difficulty) const {
    return editor_chart_identity_service::generated_chart_id(song_, difficulty);
}

editor_resume_state editor_scene::build_resume_state() const {
    return {
        state_,
        transport_.playback_tick,
        viewport_.bottom_tick,
        viewport_.bottom_tick_target,
        viewport_.ticks_per_pixel,
        viewport_.snap_index,
        waveform_visible_,
        selected_note_index_
    };
}

editor_scene_sync_context editor_scene::make_sync_context() {
    return {
        *state_,
        meter_map_,
        timing_panel_,
        metadata_panel_,
        selected_note_index_,
    };
}

void editor_scene::apply_flow_result(const editor_flow_result& result) {
    if (result.saved_chart_path.has_value()) {
        chart_path_ = result.saved_chart_path;
    }

    if (result.request_apply_confirmed_key_count) {
        apply_metadata_changes(result.clear_notes_for_key_count_change);
    }

    switch (result.navigation.target) {
        case editor_navigation_target::none:
            return;
        case editor_navigation_target::song_select:
            manager_.change_scene(song_select::make_seamless_create_scene(
                manager_,
                song_.meta.song_id,
                state_->file_path().empty() ? "" : state_->data().meta.chart_id));
            return;
        case editor_navigation_target::playtest:
            manager_.change_scene(std::make_unique<play_scene>(
                manager_,
                song_,
                make_chart_data_for_save(),
                std::max(0, result.navigation.playtest_start_tick),
                build_resume_state()));
            return;
    }
}

bool editor_scene::has_blocking_modal() const {
    return metadata_panel_.key_count_confirm_open || save_dialog_.open || unsaved_changes_dialog_.open;
}

std::optional<note_data> editor_scene::dragged_note() const {
    if (!timeline_drag_.active) {
        return std::nullopt;
    }

    note_data note;
    note.lane = timeline_drag_.lane;
    note.tick = std::min(timeline_drag_.start_tick, timeline_drag_.current_tick);
    note.end_tick = std::max(timeline_drag_.start_tick, timeline_drag_.current_tick);
    note.type = (note.end_tick - note.tick) >= editor_timeline_viewport::snap_interval(viewport_model()) ? note_type::hold : note_type::tap;
    if (note.type == note_type::tap) {
        note.end_tick = note.tick;
    }

    return note;
}

std::vector<size_t> editor_scene::sorted_timing_event_indices() const {
    return editor_timing_selection_service::sorted_indices(state_->data());
}

editor_timeline_viewport_model editor_scene::viewport_model() const {
    return {state_.get(), transport_.audio_length_tick, viewport_};
}

editor_timeline_metrics editor_scene::timeline_metrics() const {
    return editor_timeline_viewport::metrics(viewport_model());
}

int editor_scene::default_timing_event_tick() const {
    return editor_timeline_viewport::default_timing_event_tick(viewport_model(), timing_panel_.selected_event_index);
}

void editor_scene::apply_scroll_and_zoom(float dt) {
    viewport_ = editor_timeline_viewport::apply_scroll_and_zoom(viewport_model(), {
        virtual_screen::get_virtual_mouse(),
        GetMouseWheelMove(),
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL),
        transport_.audio_playing,
        transport_.playback_tick,
        dt,
    });
}

void editor_scene::select_timing_event(std::optional<size_t> index, bool scroll_into_view) {
    if (scroll_into_view && index.has_value() && *index < state_->data().timing_events.size()) {
        scroll_to_tick(state_->data().timing_events[*index].tick);
    }
    editor_scene_sync_context sync_context = make_sync_context();
    editor_timing_selection_service::select_event(sync_context, index, scroll_into_view, timing_panel_.list_scroll_offset);
}

void editor_scene::scroll_to_tick(int tick) {
    viewport_ = editor_timeline_viewport::scroll_to_tick(viewport_model(), tick);
}

bool editor_scene::apply_selected_timing_event() {
    editor_scene_sync::sync_timing_event_selection(make_sync_context());
    const editor_timing_edit_result result = editor_timing_edit_service::apply_selected({
        *state_,
        meter_map_,
        timing_panel_,
        default_timing_event_tick(),
    });
    if (!result.success) {
        return false;
    }
    editor_scene_sync::sync_after_timing_change(make_sync_context());
    editor_transport_service::sync(transport_, state_.get(), hitsound_path_, true);
    if (result.scroll_to_tick.has_value()) {
        scroll_to_tick(*result.scroll_to_tick);
    }
    return true;
}

void editor_scene::add_timing_event(timing_event_type type) {
    const editor_timing_edit_result result = editor_timing_edit_service::add_event({
        *state_,
        meter_map_,
        timing_panel_,
        default_timing_event_tick(),
    }, type);
    editor_scene_sync::sync_after_timing_change(make_sync_context());
    editor_transport_service::sync(transport_, state_.get(), hitsound_path_, true);
    if (result.selected_event_index.has_value()) {
        select_timing_event(result.selected_event_index, true);
    }
}

void editor_scene::delete_selected_timing_event() {
    editor_scene_sync::sync_timing_event_selection(make_sync_context());
    const editor_timing_edit_result result = editor_timing_edit_service::delete_selected({
        *state_,
        meter_map_,
        timing_panel_,
        default_timing_event_tick(),
    });
    if (!result.success) {
        return;
    }
    editor_scene_sync::sync_after_timing_change(make_sync_context());
    editor_transport_service::sync(transport_, state_.get(), hitsound_path_, true);
}

bool editor_scene::can_delete_selected_timing_event() const {
    return editor_timing_edit_service::can_delete_selected({*state_, timing_panel_});
}

bool editor_scene::has_active_metadata_input() const {
    return metadata_panel_.difficulty_input.active || metadata_panel_.chart_author_input.active;
}

bool editor_scene::apply_metadata_changes(bool clear_notes_for_key_count_change) {
    const editor_metadata_apply_result result = editor_metadata_service::apply_changes({
        *state_,
        metadata_panel_,
        clear_notes_for_key_count_change,
        generated_chart_id(metadata_panel_.difficulty_input.value),
    });
    if (!result.success) {
        return false;
    }
    editor_scene_sync::sync_after_metadata_change(make_sync_context(), result.key_count_changed);
    editor_transport_service::sync(transport_, state_.get(), hitsound_path_, true);
    return true;
}

bool editor_scene::apply_chart_offset(int offset_ms) {
    if (!editor_metadata_service::apply_chart_offset(*state_, offset_ms)) {
        return false;
    }

    editor_scene_sync::sync_after_offset_change(make_sync_context());
    editor_transport_service::sync(transport_, state_.get(), hitsound_path_, true);
    return true;
}

void editor_scene::draw_timeline() const {
    const std::optional<note_data> preview_note = dragged_note();
    editor_timeline_presenter::draw({
        *state_,
        meter_map_,
        &waveform_summary_,
        waveform_visible_,
        waveform_offset_ms_,
        transport_.audio_loaded,
        transport_.playback_tick,
        selected_note_index_,
        preview_note,
        preview_note.has_value() && state_->has_note_overlap(*preview_note),
        viewport_model(),
    });
}
