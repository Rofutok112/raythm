#include "editor_scene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <memory>

#include "app_paths.h"
#include "audio_manager.h"
#include "editor/view/editor_cursor_hud_view.h"
#include "editor/editor_flow_controller.h"
#include "editor/view/editor_header_view.h"
#include "editor/view/editor_layout.h"
#include "editor/view/editor_left_panel_view.h"
#include "editor/view/editor_modal_view.h"
#include "editor/view/editor_right_panel_view.h"
#include "editor/viewport/editor_timeline_viewport.h"
#include "editor/editor_session_loader.h"
#include "path_utils.h"
#include "play_scene.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "theme.h"
#include "uuid_util.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {
namespace layout = editor::layout;

constexpr float kPlaybackRestartEpsilonSeconds = 0.01f;

Rectangle snap_dropdown_menu_rect() {
    return layout::snap_dropdown_menu_rect(static_cast<int>(editor_timeline_viewport::snap_labels().size()));
}

const char* key_count_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

editor_timeline_note make_timeline_note(const note_data& note) {
    return {
        note.type == note_type::hold ? editor_timeline_note_type::hold : editor_timeline_note_type::tap,
        note.tick,
        note.lane,
        note.end_tick
    };
}

bool timing_event_sort_less(const timing_event& left, size_t left_index,
                            const timing_event& right, size_t right_index) {
    if (left.tick != right.tick) {
        return left.tick < right.tick;
    }
    if (left.type != right.type) {
        return left.type == timing_event_type::bpm;
    }
    return left_index < right_index;
}

bool try_parse_int(const std::string& text, int& out_value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const int value = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        out_value = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_float(const std::string& text, float& out_value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const float value = std::stof(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        out_value = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_bar_beat(const std::string& text, editor_meter_map::bar_beat_position& out_value) {
    if (text.empty()) {
        return false;
    }

    const size_t colon = text.find(':');
    int measure = 0;
    int beat = 1;
    if (colon == std::string::npos) {
        if (!try_parse_int(text, measure)) {
            return false;
        }
    } else {
        if (!try_parse_int(text.substr(0, colon), measure) ||
            !try_parse_int(text.substr(colon + 1), beat)) {
            return false;
        }
    }

    if (measure <= 0 || beat <= 0) {
        return false;
    }

    out_value = {measure, beat};
    return true;
}

std::string format_playback_time(double seconds) {
    const int total_ms = std::max(0, static_cast<int>(std::lround(seconds * 1000.0)));
    const int minutes = total_ms / 60000;
    const int whole_seconds = (total_ms / 1000) % 60;
    const int centiseconds = (total_ms % 1000) / 10;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d.%02d", minutes, whole_seconds, centiseconds);
    return buffer;
}

std::string slugify(std::string text) {
    std::string slug;
    slug.reserve(text.size());
    bool previous_dash = false;

    for (unsigned char ch : text) {
        if (std::isalnum(ch) != 0) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
            previous_dash = false;
        } else if (!previous_dash && !slug.empty()) {
            slug.push_back('-');
            previous_dash = true;
        }
    }

    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }

    return slug;
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
    audio_length_tick_ = load_result.audio_length_tick;
    audio_loaded_ = load_result.audio_loaded;
    audio_playing_ = load_result.audio_playing;
    audio_time_seconds_ = load_result.audio_time_seconds;
    playback_tick_ = load_result.playback_tick;
    previous_playback_tick_ = load_result.previous_playback_tick;
    previous_audio_playing_ = load_result.previous_audio_playing;
    hitsound_path_ = load_result.hitsound_path;
    waveform_visible_ = load_result.waveform_visible;
    waveform_offset_ms_ = load_result.waveform_offset_ms;
    waveform_summary_ = load_result.waveform_summary;
    waveform_samples_ = load_result.waveform_samples;
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
    sync_transport_state();

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
        playback_tick_,
    });
    apply_flow_result(flow_result);
    if (flow_result.consume_update) {
        return;
    }

    handle_shortcuts();
    handle_text_input();
    handle_timeline_interaction();
    sync_transport_state();
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

    const std::string playback_status = playback_status_text();
    const std::string offset_label =
        (state_->data().meta.offset > 0 ? "+" : "") + std::to_string(state_->data().meta.offset) + " ms";
    const editor_header_view_result header_result = editor_header_view::draw({
        playback_status.c_str(),
        audio_loaded_,
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
    // For AppData-based songs, use UUID.
    const bool is_appdata_song = song_.directory.find(path_utils::to_utf8(app_paths::app_data_root())) != std::string::npos;
    if (is_appdata_song) {
        return generate_uuid();
    }

    // Legacy: slug-based ID.
    const std::string song_id = slugify(song_.meta.song_id);
    const std::string difficulty_slug = slugify(difficulty);
    if (!song_id.empty() && !difficulty_slug.empty()) {
        return song_id + "-" + difficulty_slug;
    }
    if (!song_id.empty()) {
        return song_id + "-chart";
    }
    if (!difficulty_slug.empty()) {
        return "chart-" + difficulty_slug;
    }
    return "new-chart";
}

editor_resume_state editor_scene::build_resume_state() const {
    return {
        state_,
        playback_tick_,
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
        waveform_summary_,
        waveform_samples_,
        waveform_offset_ms_,
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
            manager_.change_scene(std::make_unique<song_select_scene>(manager_, song_.meta.song_id));
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

void editor_scene::apply_transport_result(const editor_transport_result& result) {
    audio_loaded_ = result.audio_loaded;
    audio_playing_ = result.audio_playing;
    audio_time_seconds_ = result.audio_time_seconds;
    playback_tick_ = result.playback_tick;
    previous_playback_tick_ = result.previous_playback_tick;
    previous_audio_playing_ = result.previous_audio_playing;
    audio_length_tick_ = result.audio_length_tick;

    for (int i = 0; i < result.hitsound_count; ++i) {
        audio_manager::instance().play_se(hitsound_path_, 0.45f);
    }
}

void editor_scene::sync_transport_state(bool suppress_hitsounds) {
    editor_transport_context context;
    context.state = state_.get();
    context.audio_loaded = audio_loaded_;
    context.audio_playing = audio_playing_;
    context.audio_time_seconds = audio_time_seconds_;
    context.playback_tick = playback_tick_;
    context.previous_playback_tick = suppress_hitsounds ? playback_tick_ : previous_playback_tick_;
    context.previous_audio_playing = suppress_hitsounds ? false : previous_audio_playing_;
    context.hitsound_path = &hitsound_path_;
    if (audio_loaded_ && audio_manager::instance().is_bgm_loaded()) {
        context.bgm_clock = audio_manager::instance().get_bgm_clock();
        context.bgm_length_seconds = audio_manager::instance().get_bgm_length_seconds();
    }

    apply_transport_result(editor_transport_controller::sync(context));
}

void editor_scene::seek_audio_to_tick(int tick) {
    editor_transport_context context;
    context.state = state_.get();
    context.audio_loaded = audio_loaded_;
    const editor_transport_result result = editor_transport_controller::seek_to_tick(context, tick);
    if (result.seek_bgm_seconds.has_value()) {
        audio_manager::instance().seek_bgm(*result.seek_bgm_seconds);
        sync_transport_state(true);
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
    std::vector<size_t> indices(state_->data().timing_events.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    std::stable_sort(indices.begin(), indices.end(), [this](size_t left_index, size_t right_index) {
        const timing_event& left = state_->data().timing_events[left_index];
        const timing_event& right = state_->data().timing_events[right_index];
        return timing_event_sort_less(right, right_index, left, left_index);
    });

    return indices;
}

editor_timeline_viewport_model editor_scene::viewport_model() const {
    return {state_.get(), audio_length_tick_, viewport_};
}

editor_timeline_metrics editor_scene::timeline_metrics() const {
    return editor_timeline_viewport::metrics(viewport_model());
}

int editor_scene::default_timing_event_tick() const {
    return editor_timeline_viewport::default_timing_event_tick(viewport_model(), timing_panel_.selected_event_index);
}

std::string editor_scene::playback_status_text() const {
    if (!audio_loaded_) {
        return "No audio";
    }

    return std::string(audio_playing_ ? "Playing " : "Paused ") + format_playback_time(audio_time_seconds_);
}

void editor_scene::handle_shortcuts() {
    if (has_blocking_modal()) {
        return;
    }

    if (!has_active_metadata_input() &&
        timing_panel_.active_input_field == editor_timing_input_field::none &&
        !timing_panel_.bar_pick_mode &&
        !timeline_drag_.active &&
        IsKeyPressed(KEY_SPACE)) {
        editor_transport_context transport_context;
        transport_context.state = state_.get();
        transport_context.audio_loaded = audio_loaded_;
        transport_context.audio_playing = audio_playing_;
        transport_context.playback_tick = playback_tick_;
        transport_context.space_playback_start_tick = space_playback_start_tick_;
        const std::optional<int> restore_tick = transport_context.space_playback_start_tick;
        const editor_transport_result transport_result =
            editor_transport_controller::toggle_playback(transport_context);
        space_playback_start_tick_ = transport_result.next_space_playback_start_tick;
        if (transport_result.request_pause_bgm) {
            audio_manager::instance().pause_bgm();
            if (transport_result.seek_bgm_seconds.has_value()) {
                audio_manager::instance().seek_bgm(*transport_result.seek_bgm_seconds);
            }
        } else if (transport_result.request_play_bgm && audio_manager::instance().is_bgm_loaded()) {
            const double length_seconds = audio_manager::instance().get_bgm_length_seconds();
            const double position_seconds = audio_manager::instance().get_bgm_position_seconds();
            const bool restart = length_seconds > 0.0 &&
                                 position_seconds >= std::max(0.0, length_seconds - kPlaybackRestartEpsilonSeconds);
            audio_manager::instance().play_bgm(restart);
        }
        sync_transport_state(true);
        if (transport_result.request_pause_bgm && restore_tick.has_value()) {
            scroll_to_tick(*restore_tick);
        }
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Z)) {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            state_->redo();
        } else {
            state_->undo();
        }
        editor_scene_sync::sync_after_history_change(make_sync_context());
        sync_transport_state(true);
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Y)) {
        state_->redo();
        editor_scene_sync::sync_after_history_change(make_sync_context());
        sync_transport_state(true);
    }

    if (!has_active_metadata_input() &&
        timing_panel_.active_input_field == editor_timing_input_field::none &&
        IsKeyPressed(KEY_DELETE) && selected_note_index_.has_value()) {
        const size_t selected_index = *selected_note_index_;
        if (state_->remove_note(selected_index)) {
            selected_note_index_.reset();
        }
    }

    if (selected_note_index_.has_value() && *selected_note_index_ >= state_->data().notes.size()) {
        selected_note_index_.reset();
    }
}

void editor_scene::handle_text_input() {
    if (has_active_metadata_input() || has_blocking_modal()) {
        return;
    }
}

void editor_scene::handle_timeline_interaction() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const editor_timeline_metrics metrics = timeline_metrics();
    const Rectangle content = metrics.content_rect();
    const editor_timeline_result result = editor_timeline_controller::update(timing_panel_, {
        state_.get(),
        &meter_map_,
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
        selected_note_index_,
        timeline_drag_,
    });

    selected_note_index_ = result.selected_note_index;
    timeline_drag_ = result.drag_state;

    if (result.request_seek) {
        const bool was_playing = audio_playing_;
        if (was_playing) {
            audio_manager::instance().pause_bgm();
            space_playback_start_tick_.reset();
            sync_transport_state(true);
        }
        seek_audio_to_tick(result.seek_tick);
        if (was_playing || result.scroll_seek_if_paused) {
            scroll_to_tick(playback_tick_);
        }
    }
    if (result.request_apply_selected_timing) {
        apply_selected_timing_event();
    }
    if (result.note_to_delete_index.has_value()) {
        if (state_->remove_note(*result.note_to_delete_index)) {
            selected_note_index_.reset();
        }
    }
    if (result.note_to_add.has_value()) {
        state_->add_note(*result.note_to_add);
        selected_note_index_ = state_->data().notes.empty()
            ? std::nullopt
            : std::optional<size_t>(state_->data().notes.size() - 1);
    }
}

void editor_scene::apply_scroll_and_zoom(float dt) {
    viewport_ = editor_timeline_viewport::apply_scroll_and_zoom(viewport_model(), {
        virtual_screen::get_virtual_mouse(),
        GetMouseWheelMove(),
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL),
        audio_playing_,
        playback_tick_,
        dt,
    });
}

void editor_scene::select_timing_event(std::optional<size_t> index, bool scroll_into_view) {
    timing_panel_.selected_event_index = index;
    timing_panel_.active_input_field = editor_timing_input_field::none;
    timing_panel_.input_error.clear();
    timing_panel_.bar_pick_mode = false;
    editor_scene_sync::load_timing_event_inputs(make_sync_context());

    if (scroll_into_view && index.has_value() && *index < state_->data().timing_events.size()) {
        scroll_to_tick(state_->data().timing_events[*index].tick);
        const auto timing_indices = sorted_timing_event_indices();
        const auto it = std::find(timing_indices.begin(), timing_indices.end(), *index);
        if (it != timing_indices.end()) {
            constexpr float kTimingRowHeight = 30.0f;
            constexpr float kTimingRowGap = 4.0f;
            constexpr float kTimingListViewportHeight = 174.0f;
            const float row_top = static_cast<float>(std::distance(timing_indices.begin(), it)) * (kTimingRowHeight + kTimingRowGap);
            const float row_bottom = row_top + kTimingRowHeight;
            if (row_top < timing_panel_.list_scroll_offset) {
                timing_panel_.list_scroll_offset = row_top;
            } else if (row_bottom > timing_panel_.list_scroll_offset + kTimingListViewportHeight) {
                timing_panel_.list_scroll_offset = row_bottom - kTimingListViewportHeight;
            }
        }
    }
}

void editor_scene::scroll_to_tick(int tick) {
    viewport_ = editor_timeline_viewport::scroll_to_tick(viewport_model(), tick);
}

bool editor_scene::apply_selected_timing_event() {
    editor_scene_sync::sync_timing_event_selection(make_sync_context());
    if (!timing_panel_.selected_event_index.has_value()) {
        timing_panel_.input_error = "Select a timing event first.";
        return false;
    }

    const size_t index = *timing_panel_.selected_event_index;
    if (index >= state_->data().timing_events.size()) {
        timing_panel_.input_error = "Selected timing event is out of range.";
        return false;
    }

    timing_event updated = state_->data().timing_events[index];
    if (updated.type == timing_event_type::bpm) {
        editor_meter_map::bar_beat_position position;
        float bpm = 0.0f;
        if (!try_parse_bar_beat(timing_panel_.inputs.bpm_bar.value, position)) {
            timing_panel_.input_error = "Bar must be in M:B format.";
            return false;
        }
        if (!try_parse_float(timing_panel_.inputs.bpm_value.value, bpm) || bpm <= 0.0f) {
            timing_panel_.input_error = "BPM must be greater than zero.";
            return false;
        }
        const std::optional<int> tick = meter_map_.tick_from_bar_beat(position.measure, position.beat);
        if (!tick.has_value()) {
            timing_panel_.input_error = "Bar is outside the current meter layout.";
            return false;
        }
        updated.tick = *tick;
        updated.bpm = bpm;
    } else {
        editor_meter_map::bar_beat_position position;
        int numerator = 0;
        int denominator = 0;
        if (!try_parse_bar_beat(timing_panel_.inputs.meter_bar.value, position)) {
            timing_panel_.input_error = "Bar must be in M:B format.";
            return false;
        }
        if (!try_parse_int(timing_panel_.inputs.meter_numerator.value, numerator) || numerator <= 0) {
            timing_panel_.input_error = "Numerator must be 1 or greater.";
            return false;
        }
        if (!try_parse_int(timing_panel_.inputs.meter_denominator.value, denominator) || denominator <= 0) {
            timing_panel_.input_error = "Denominator must be 1 or greater.";
            return false;
        }
        const std::optional<int> tick = meter_map_.tick_from_bar_beat(position.measure, position.beat);
        if (!tick.has_value()) {
            timing_panel_.input_error = "Bar is outside the current meter layout.";
            return false;
        }
        updated.tick = *tick;
        updated.numerator = numerator;
        updated.denominator = denominator;
    }

    if (updated.type == timing_event_type::bpm && state_->data().timing_events[index].tick == 0 && updated.tick != 0) {
        timing_panel_.input_error = "The BPM event at tick 0 must stay at tick 0.";
        return false;
    }

    if (!state_->modify_timing_event(index, updated)) {
        timing_panel_.input_error = "Failed to update the timing event.";
        return false;
    }

    editor_scene_sync::sync_after_timing_change(make_sync_context());
    sync_transport_state(true);
    scroll_to_tick(updated.tick);
    return true;
}

void editor_scene::add_timing_event(timing_event_type type) {
    timing_event event;
    event.type = type;
    event.tick = default_timing_event_tick();
    if (type == timing_event_type::bpm) {
        event.bpm = state_->engine().get_bpm_at(event.tick);
        event.numerator = 4;
        event.denominator = 4;
    } else {
        const editor_meter_map::bar_beat_position position = meter_map_.bar_beat_at_tick(event.tick);
        const std::optional<int> snapped_tick = meter_map_.tick_from_bar_beat(position.measure, 1);
        event.tick = snapped_tick.value_or(event.tick);
        event.bpm = 0.0f;
        event.numerator = 4;
        event.denominator = 4;
    }

    state_->add_timing_event(event);
    editor_scene_sync::sync_after_timing_change(make_sync_context());
    sync_transport_state(true);
    select_timing_event(state_->data().timing_events.size() - 1, true);
}

void editor_scene::delete_selected_timing_event() {
    editor_scene_sync::sync_timing_event_selection(make_sync_context());
    if (!timing_panel_.selected_event_index.has_value()) {
        timing_panel_.input_error = "Select a timing event first.";
        return;
    }
    if (!can_delete_selected_timing_event()) {
        timing_panel_.input_error = "The BPM event at tick 0 cannot be deleted.";
        return;
    }

    const size_t index = *timing_panel_.selected_event_index;
    if (!state_->remove_timing_event(index)) {
        timing_panel_.input_error = "Failed to delete the timing event.";
        return;
    }

    editor_scene_sync::sync_after_timing_change(make_sync_context());
    sync_transport_state(true);
}

bool editor_scene::can_delete_selected_timing_event() const {
    if (!timing_panel_.selected_event_index.has_value() ||
        *timing_panel_.selected_event_index >= state_->data().timing_events.size()) {
        return false;
    }
    const timing_event& event = state_->data().timing_events[*timing_panel_.selected_event_index];
    return !(event.type == timing_event_type::bpm && event.tick == 0);
}

bool editor_scene::has_active_metadata_input() const {
    return metadata_panel_.difficulty_input.active || metadata_panel_.chart_author_input.active;
}

bool editor_scene::apply_metadata_changes(bool clear_notes_for_key_count_change) {
    chart_meta updated = state_->data().meta;
    updated.difficulty = metadata_panel_.difficulty_input.value;
    updated.chart_author = metadata_panel_.chart_author_input.value;
    updated.key_count = metadata_panel_.key_count;
    if (state_->file_path().empty()) {
        updated.chart_id = generated_chart_id(updated.difficulty);
    }

    const bool key_count_changed = updated.key_count != state_->data().meta.key_count;
    if (key_count_changed && !clear_notes_for_key_count_change && !state_->data().notes.empty()) {
        metadata_panel_.pending_key_count = updated.key_count;
        metadata_panel_.key_count_confirm_open = true;
        metadata_panel_.error = "Changing mode will clear all notes.";
        return false;
    }

    if (!state_->modify_metadata(updated, clear_notes_for_key_count_change)) {
        metadata_panel_.error = "Failed to update chart metadata.";
        metadata_panel_.key_count = state_->data().meta.key_count;
        return false;
    }

    editor_scene_sync::sync_after_metadata_change(make_sync_context(), key_count_changed);
    sync_transport_state(true);
    return true;
}

bool editor_scene::apply_chart_offset(int offset_ms) {
    chart_meta updated = state_->data().meta;
    updated.offset = offset_ms;
    if (!state_->modify_metadata(updated)) {
        return false;
    }

    editor_scene_sync::sync_after_offset_change(make_sync_context());
    sync_transport_state(true);
    return true;
}

void editor_scene::draw_timeline() const {
    const editor_timeline_viewport_model model = viewport_model();
    const editor_timeline_metrics metrics = timeline_metrics();
    const float visible_tick_span = editor_timeline_viewport::visible_tick_span(model);
    const int min_tick = static_cast<int>(std::floor(viewport_.bottom_tick - visible_tick_span * 0.1f));
    const int max_tick = static_cast<int>(std::ceil(viewport_.bottom_tick + visible_tick_span));
    std::vector<editor_timeline_note> notes;
    notes.reserve(state_->data().notes.size());
    for (const note_data& note : state_->data().notes) {
        notes.push_back(make_timeline_note(note));
    }
    std::optional<editor_timeline_note> preview_note;
    bool preview_has_overlap = false;
    if (const std::optional<note_data> preview_data = dragged_note(); preview_data.has_value()) {
        preview_note = make_timeline_note(*preview_data);
        preview_has_overlap = state_->has_note_overlap(*preview_data);
    }

    editor_timeline_view::draw({
        metrics,
        meter_map_.visible_grid_lines(min_tick, max_tick),
        std::move(notes),
        selected_note_index_,
        audio_loaded_ ? std::optional<int>(playback_tick_) : std::nullopt,
        &waveform_samples_,
        waveform_visible_,
        preview_note,
        preview_has_overlap,
        min_tick,
        max_tick,
        editor_timeline_viewport::snap_interval(model),
        editor_timeline_viewport::content_height_pixels(model),
        editor_timeline_viewport::scroll_offset_pixels(model)
    });
}
