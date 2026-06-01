#include "editor_scene.h"

#include <algorithm>
#include <cstdlib>
#include <memory>

#include "audio_manager.h"
#include "editor/controller/editor_runtime_controller.h"
#include "editor/controller/editor_screen_controller.h"
#include "editor/controller/editor_timeline_screen_controller.h"
#include "editor/controller/editor_timing_action_controller.h"
#include "editor/editor_flow_controller.h"
#include "editor/service/editor_chart_identity_service.h"
#include "editor/service/editor_metadata_service.h"
#include "editor/service/editor_transport_service.h"
#include "editor/service/editor_timing_selection_service.h"
#include "editor/view/editor_layout.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "editor/editor_session_loader.h"
#include "chart_level_cache.h"
#include "play_scene.h"
#include "platform/window_chrome.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "theme.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {
namespace layout = editor::layout;
constexpr double kLevelRefreshDebounceSeconds = 0.10;

Rectangle snap_dropdown_menu_rect() {
    return layout::snap_dropdown_menu_rect(static_cast<int>(editor_timeline_viewport::snap_labels().size()));
}

}

editor_scene::editor_scene(scene_manager& manager, song_data song, std::string chart_path)
    : scene(manager), song_(std::move(song)), chart_path_(std::move(chart_path)), state_(std::make_shared<editor_state>()),
      settings_overlay_(g_settings) {
}

editor_scene::editor_scene(scene_manager& manager, song_data song, int key_count)
    : scene(manager), song_(std::move(song)), chart_path_(std::nullopt),
      new_chart_key_count_(key_count), state_(std::make_shared<editor_state>()),
      settings_overlay_(g_settings) {
}

editor_scene::editor_scene(scene_manager& manager, song_data song, chart_meta initial_meta)
    : scene(manager), song_(std::move(song)), chart_path_(std::nullopt),
      initial_meta_(std::move(initial_meta)),
      new_chart_key_count_(initial_meta_.has_value() ? initial_meta_->key_count : 4),
      state_(std::make_shared<editor_state>()),
      settings_overlay_(g_settings) {
}

editor_scene::editor_scene(scene_manager& manager, song_data song, editor_resume_state resume)
    : scene(manager), song_(std::move(song)), chart_path_(std::nullopt),
      new_chart_key_count_(resume.state ? resume.state->data().meta.key_count : 4),
      state_(resume.state ? resume.state : std::make_shared<editor_state>()),
      resume_state_(std::move(resume)),
      settings_overlay_(g_settings) {
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
    transport_.pre_audio_playing = load_result.pre_audio_playing;
    transport_.audio_time_seconds = load_result.audio_time_seconds;
    transport_.playback_tick = load_result.playback_tick;
    transport_.previous_playback_tick = load_result.previous_playback_tick;
    transport_.previous_audio_playing = load_result.previous_audio_playing;
    hitsound_path_ = load_result.hitsound_path;
    hitsounds_ = load_result.hitsounds;
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
    selected_note_indices_ = load_result.selected_note_indices;
    timeline_drag_ = {};
    resume_state_.reset();
}

void editor_scene::on_exit() {
    if (settings_overlay_active_) {
        settings_overlay_.save();
    }
    window_chrome::set_content_cursor(MOUSE_CURSOR_DEFAULT);
    audio_manager::instance().stop_bgm();
    audio_manager::instance().stop_all_se();
}

void editor_scene::update(float dt) {
    rebuild_hit_regions();
    editor_transport_service::sync(transport_, state_.get(), hitsound_path_, &hitsounds_, false, dt);

    if (settings_overlay_active_) {
        settings_overlay_.update_animation(true, dt);
        if (settings_overlay_.closing()) {
            if (settings_overlay_.closed()) {
                settings_overlay_active_ = false;
            }
            window_chrome::set_content_cursor(MOUSE_CURSOR_DEFAULT);
            return;
        }
        settings_overlay_.update(dt);
        window_chrome::set_content_cursor(MOUSE_CURSOR_DEFAULT);
        return;
    }

    if ((metadata_modal_open_ || timing_modal_open_) &&
        !metadata_panel_.key_count_confirm_open &&
        IsKeyPressed(KEY_ESCAPE)) {
        metadata_modal_open_ = false;
        timing_modal_open_ = false;
        metadata_panel_.difficulty_input.active = false;
        metadata_panel_.chart_author_input.active = false;
        timing_panel_.active_input_field = editor_timing_input_field::none;
        timing_panel_.bar_pick_mode = false;
        window_chrome::set_content_cursor(MOUSE_CURSOR_DEFAULT);
        return;
    }

    const bool save_dialog_submit = save_dialog_.submit_requested ||
        (save_dialog_.open && ui::is_clicked(layout::save_submit_button_rect(), ui::draw_layer::modal));
    save_dialog_.submit_requested = false;
    const bool playtest_requested = IsKeyPressed(KEY_F5) || playtest_button_requested_;
    playtest_button_requested_ = false;

    const editor_flow_result flow_result = editor_flow_controller::update({
        &song_,
        [this]() { return make_chart_data_for_save(); },
        state_,
        &metadata_panel_,
        &save_dialog_,
        &unsaved_changes_dialog_,
        IsKeyPressed(KEY_ESCAPE),
        ui::is_clicked(layout::kBackButtonRect),
        (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S),
        playtest_requested,
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
        window_chrome::set_content_cursor(MOUSE_CURSOR_DEFAULT);
        return;
    }

    if (!has_blocking_modal() && ui::is_clicked(layout::kSettingsButtonRect)) {
        window_chrome::set_content_cursor(MOUSE_CURSOR_DEFAULT);
        editor_transport_service::pause_for_seek(transport_, state_.get(), space_playback_start_tick_, hitsound_path_, &hitsounds_);
        settings_overlay_.open();
        settings_overlay_active_ = true;
        return;
    }

    const editor_shortcut_result shortcut_result = editor_runtime_controller::handle_shortcuts({
        *state_,
        make_sync_context(),
        metadata_panel_,
        timing_panel_,
        timeline_drag_,
        selected_note_indices_,
        clipboard_notes_,
        transport_,
        space_playback_start_tick_,
        hitsound_path_,
        &hitsounds_,
        has_blocking_modal(),
        false,
        IsKeyPressed(KEY_SPACE),
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
        IsKeyPressed(KEY_C),
        IsKeyPressed(KEY_V),
        IsKeyPressed(KEY_D),
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
        selected_note_indices_,
        timeline_drag_,
        hitsound_path_,
        &hitsounds_,
        metrics,
        mouse,
        ui::is_hovered(content, ui::draw_layer::base),
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
        IsKeyPressed(KEY_ESCAPE),
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL),
        editor_timeline_viewport::snap_division(viewport_),
        note_palette_,
        IsMouseButtonDown(MOUSE_BUTTON_RIGHT),
        IsMouseButtonReleased(MOUSE_BUTTON_RIGHT),
    });
    if (timeline_result.request_apply_selected_timing) {
        editor_timing_action_controller::apply_selected_timing_event(timing_action_context());
    }
    if (timeline_result.request_apply_selected_scroll) {
        editor_timing_action_controller::apply_selected_scroll_event(timing_action_context());
    }
    if (timeline_result.selected_scroll_event_index.has_value()) {
        select_scroll_event(timeline_result.selected_scroll_event_index, false);
    }
    if (timeline_result.scroll_to_tick.has_value()) {
        scroll_to_tick(*timeline_result.scroll_to_tick);
    }

    editor_transport_service::sync(transport_, state_.get(), hitsound_path_, &hitsounds_);
    apply_scroll_and_zoom(dt);
    refresh_chart_level_when_idle();
    window_chrome::set_content_cursor(editor_timeline_screen_controller::mouse_cursor({
        *state_,
        timeline_drag_,
        selected_note_indices_,
        timeline_metrics(),
        virtual_screen::get_virtual_mouse(),
        has_blocking_modal(),
    }));
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
    if (metadata_modal_open_) {
        ui::register_hit_region(layout::kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(layout::kEditorMetadataModalRect, ui::draw_layer::modal);
    }
    if (timing_modal_open_) {
        ui::register_hit_region(layout::kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(layout::kEditorTimingModalRect, ui::draw_layer::modal);
    }
}

void editor_scene::draw() {
    if (settings_overlay_active_) {
        settings_overlay_.prepare_current_page();
        virtual_screen::begin_ui();
        draw_scene_background(*g_theme);
        settings_overlay_.draw();
        virtual_screen::end();
        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    editor_screen_controller::draw_and_update({
        song_,
        *state_,
        meter_map_,
        timing_panel_,
        metadata_panel_,
        transport_,
        space_playback_start_tick_,
        hitsound_path_,
        hitsounds_,
        waveform_visible_,
        viewport_,
        snap_dropdown_open_,
        selected_note_indices_,
        timeline_drag_,
        note_palette_,
        load_errors_,
        save_dialog_,
        unsaved_changes_dialog_,
        metadata_modal_open_,
        timing_modal_open_,
        playtest_button_requested_,
        [this]() { rebuild_hit_regions(); },
        [this]() {
            return editor_timeline_screen_controller::draw({
                *state_,
                meter_map_,
                waveform_summary_,
                waveform_visible_,
                waveform_offset_ms_,
                transport_,
                viewport_,
                snap_dropdown_open_,
                selected_note_indices_,
                timing_panel_,
                timeline_drag_,
                note_palette_,
            });
        },
        [this]() { return make_sync_context(); },
        [this]() { return timing_action_context(); },
        [this](std::optional<size_t> index, bool scroll_into_view) { select_timing_event(index, scroll_into_view); },
        [this](std::optional<size_t> index, bool scroll_into_view) { select_scroll_event(index, scroll_into_view); },
        [this](int tick) { scroll_to_tick(tick); },
        [this](bool clear_notes) { return apply_metadata_changes(clear_notes); },
        [this](int offset_ms) { return apply_chart_offset(offset_ms); },
    });
}

chart_data editor_scene::make_chart_data_for_save() {
    state_->refresh_auto_level();
    pending_level_refresh_generation_ = state_->level_refresh_generation();
    level_refresh_after_time_ = 0.0;
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
        selected_note_indices_
    };
}

editor_scene_sync_context editor_scene::make_sync_context() {
    return {
        *state_,
        meter_map_,
        timing_panel_,
        metadata_panel_,
        selected_note_indices_,
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
    return metadata_panel_.key_count_confirm_open || save_dialog_.open || unsaved_changes_dialog_.open ||
        metadata_modal_open_ || timing_modal_open_;
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

editor_timing_action_controller::context editor_scene::timing_action_context() {
    return {
        *state_,
        meter_map_,
        timing_panel_,
        metadata_panel_,
        selected_note_indices_,
        transport_,
        viewport_,
        viewport_model(),
        hitsound_path_,
        hitsounds_,
    };
}

void editor_scene::apply_scroll_and_zoom(float dt) {
    Vector2 mouse = virtual_screen::get_virtual_mouse();
    const editor_timeline_metrics metrics = timeline_metrics();
    const Rectangle content = metrics.content_rect();
    const Rectangle automation = {content.x + content.width + 8.0f, content.y, 380.0f, content.height};
    if (CheckCollisionPointRec(mouse, automation)) {
        mouse = {content.x + content.width * 0.5f, std::clamp(mouse.y, content.y, content.y + content.height)};
    }
    viewport_ = editor_timeline_viewport::apply_scroll_and_zoom(viewport_model(), {
        mouse,
        GetMouseWheelMove(),
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL),
        transport_.audio_playing || transport_.pre_audio_playing,
        transport_.playback_tick,
        dt,
    });
}

void editor_scene::select_timing_event(std::optional<size_t> index, bool scroll_into_view) {
    if (scroll_into_view && index.has_value() && *index < state_->data().timing_events.size()) {
        scroll_to_tick(state_->data().timing_events[*index].tick);
    }
    timing_panel_.selected_scroll_event_index.reset();
    editor_scene_sync_context sync_context = make_sync_context();
    editor_timing_selection_service::select_event(sync_context, index, scroll_into_view, timing_panel_.list_scroll_offset);
}

void editor_scene::select_scroll_event(std::optional<size_t> index, bool scroll_into_view) {
    timing_panel_.selected_event_index.reset();
    timing_panel_.selected_scroll_event_index = index;
    timing_panel_.active_input_field = editor_timing_input_field::none;
    timing_panel_.input_error.clear();
    timing_panel_.bar_pick_mode = false;
    editor_scene_sync::load_scroll_event_inputs(make_sync_context());
    if (scroll_into_view && index.has_value() && *index < state_->data().scroll_automation.size()) {
        scroll_to_tick(state_->data().scroll_automation[*index].tick);
    }
}

void editor_scene::scroll_to_tick(int tick) {
    viewport_ = editor_timeline_viewport::scroll_to_tick(viewport_model(), tick);
}

void editor_scene::refresh_chart_level_when_idle() {
    if (!state_->level_needs_refresh()) {
        return;
    }

    const size_t generation = state_->level_refresh_generation();
    const double now = GetTime();
    if (generation != pending_level_refresh_generation_) {
        pending_level_refresh_generation_ = generation;
        level_refresh_after_time_ = now + kLevelRefreshDebounceSeconds;
    }
    if (timeline_drag_.active) {
        level_refresh_after_time_ = now + kLevelRefreshDebounceSeconds;
        return;
    }
    if (now >= level_refresh_after_time_) {
        state_->refresh_auto_level();
        level_refresh_after_time_ = 0.0;
    }
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
    editor_transport_service::sync(transport_, state_.get(), hitsound_path_, &hitsounds_, true);
    return true;
}

bool editor_scene::apply_chart_offset(int offset_ms) {
    if (!editor_metadata_service::apply_chart_offset(*state_, offset_ms)) {
        return false;
    }

    editor_scene_sync::sync_after_offset_change(make_sync_context());
    editor_transport_service::sync(transport_, state_.get(), hitsound_path_, &hitsounds_, true);
    chart_level_cache::clear();
    return true;
}
