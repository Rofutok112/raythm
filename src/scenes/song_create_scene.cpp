#include "song_create_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app_paths.h"

#include "audio_manager.h"
#include "path_utils.h"
#include "editor/editor_meter_map.h"
#include "editor_scene.h"
#include "file_dialog.h"
#include "game_settings.h"
#include "gameplay/timing_engine.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_create/song_create_form_panel.h"
#include "song_create/song_create_saved_view.h"
#include "song_create/song_create_service.h"
#include "song_create/song_create_tag_editor.h"
#include "song_create/song_create_timing_controller.h"
#include "song_create/song_create_timing_panel.h"
#include "song_create/song_create_timing_service.h"
#include "song_select/song_select_navigation.h"
#include "theme.h"
#include "ui_frame.h"
#include "ui_text.h"
#include "ui_text_input.h"
#include "virtual_screen.h"

namespace {

namespace fs = std::filesystem;

constexpr ui::draw_layer kLayer = ui::draw_layer::base;
constexpr int kScreenW = kScreenWidth;
constexpr int kScreenH = kScreenHeight;
constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenW), static_cast<float>(kScreenH)};
constexpr Rectangle kCardFrameRect = ui::place(kScreenRect, 1125.0f, 990.0f,
                                               ui::anchor::top_left, ui::anchor::top_left,
                                               Vector2{397.5f, 45.0f});
constexpr Rectangle kHeaderRect = ui::place(kCardFrameRect, 930.0f, 90.0f,
                                            ui::anchor::top_left, ui::anchor::top_left,
                                            Vector2{51.0f, 42.0f});

constexpr float kFormWidth = 1050.0f;
constexpr float kRowHeight = 63.0f;
constexpr float kRowGap = 9.0f;
constexpr float kFormStartY = 213.0f;
constexpr float kFormX = (static_cast<float>(kScreenW) - kFormWidth) * 0.5f;
constexpr float kFormCardPaddingX = 39.0f;
constexpr Rectangle kFormCardRect = {kFormX - kFormCardPaddingX, 177.0f, kFormWidth + kFormCardPaddingX * 2.0f, 875.0f};
constexpr Rectangle kDecisionCardRect = {525.0f, 315.0f, 870.0f, 330.0f};
constexpr Rectangle kTimingModalRect = ui::place(kScreenRect, 1160.0f, 720.0f,
                                                 ui::anchor::center, ui::anchor::center);
constexpr float kTextInputLabelWidth = 180.0f;
constexpr ui::draw_layer kModalLayer = ui::draw_layer::modal;

bool parse_int_text(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_float_text(const std::string& text, float& value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const float parsed = std::stof(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

song_create::timing_controller::context timing_context(ui::text_input_state& bpm_input,
                                                       ui::text_input_state& bar_input,
                                                       ui::text_input_state& event_bpm_input,
                                                       ui::text_input_state& numerator_input,
                                                       ui::text_input_state& denominator_input,
                                                       std::vector<timing_event>& events,
                                                       std::optional<size_t>& selected_event_index,
                                                       float& event_scroll_offset,
                                                       std::string& import_status,
                                                       std::string& error) {
    return {
        bpm_input,
        bar_input,
        event_bpm_input,
        numerator_input,
        denominator_input,
        events,
        selected_event_index,
        event_scroll_offset,
        import_status,
        error,
    };
}

}  // namespace

song_create_scene::song_create_scene(scene_manager& manager)
    : scene(manager) {
    ensure_timing_events_initialized();
}

song_create_scene::song_create_scene(scene_manager& manager, song_data song_to_edit)
    : scene(manager), created_song_(song_to_edit), editing_song_(std::move(song_to_edit)) {
    const song_meta& meta = editing_song_->meta;
    title_input_.value = meta.title;
    artist_input_.value = meta.artist;
    selected_genres_ = song_create::tag_editor::normalize_genres_for_editor(meta);
    selected_keywords_ = song_create::tag_editor::normalize_keywords_for_editor(meta);
    bpm_input_.value = meta.base_bpm > 0.0f ? TextFormat("%.6g", meta.base_bpm) : "";
    offset_input_.value = std::to_string(meta.has_offset ? meta.offset : 0);
    preview_ms_input_.value = std::to_string(meta.preview_start_ms);
    timing_events_ = meta.timing_events;
    ensure_timing_events_initialized();

    if (!meta.audio_file.empty()) {
        audio_path_input_.value = path_utils::to_utf8(path_utils::join_utf8(editing_song_->directory, meta.audio_file));
    }
    if (!meta.jacket_file.empty()) {
        jacket_path_input_.value = path_utils::to_utf8(path_utils::join_utf8(editing_song_->directory, meta.jacket_file));
    }
}

void song_create_scene::ensure_timing_events_initialized() {
    song_create::timing_controller::ensure_initialized(timing_context(
        bpm_input_, timing_bar_input_, timing_bpm_input_, timing_numerator_input_,
        timing_denominator_input_, timing_events_, selected_timing_event_index_,
        timing_event_scroll_offset_, timing_import_status_, error_));
}

void song_create_scene::sync_selected_timing_inputs() {
    song_create::timing_controller::sync_selected_inputs(timing_context(
        bpm_input_, timing_bar_input_, timing_bpm_input_, timing_numerator_input_,
        timing_denominator_input_, timing_events_, selected_timing_event_index_,
        timing_event_scroll_offset_, timing_import_status_, error_));
}

void song_create_scene::add_timing_event(timing_event_type type) {
    song_create::timing_controller::add_event(
        timing_context(bpm_input_, timing_bar_input_, timing_bpm_input_, timing_numerator_input_,
                       timing_denominator_input_, timing_events_, selected_timing_event_index_,
                       timing_event_scroll_offset_, timing_import_status_, error_),
        type);
}

void song_create_scene::delete_selected_timing_event() {
    song_create::timing_controller::delete_selected_event(timing_context(
        bpm_input_, timing_bar_input_, timing_bpm_input_, timing_numerator_input_,
        timing_denominator_input_, timing_events_, selected_timing_event_index_,
        timing_event_scroll_offset_, timing_import_status_, error_));
}

bool song_create_scene::apply_selected_timing_event() {
    return song_create::timing_controller::apply_selected_event(timing_context(
        bpm_input_, timing_bar_input_, timing_bpm_input_, timing_numerator_input_,
        timing_denominator_input_, timing_events_, selected_timing_event_index_,
        timing_event_scroll_offset_, timing_import_status_, error_));
}

bool song_create_scene::flush_selected_timing_event_inputs() {
    return song_create::timing_controller::flush_selected_inputs(timing_context(
        bpm_input_, timing_bar_input_, timing_bpm_input_, timing_numerator_input_,
        timing_denominator_input_, timing_events_, selected_timing_event_index_,
        timing_event_scroll_offset_, timing_import_status_, error_));
}

void song_create_scene::close_timing_modal() {
    if (!flush_selected_timing_event_inputs()) {
        return;
    }
    timing_modal_open_ = false;
    stop_timing_preview();
}

bool song_create_scene::start_timing_preview() {
    if (audio_path_input_.value.empty()) {
        error_ = "Audio file is required.";
        return false;
    }
    const fs::path audio_source = path_utils::from_utf8(audio_path_input_.value);
    if (!fs::exists(audio_source)) {
        error_ = "Audio file not found: " + audio_path_input_.value;
        return false;
    }

    audio_manager& audio = audio_manager::instance();
    if (!audio.load_preview(audio_path_input_.value)) {
        error_ = "Failed to load audio preview.";
        return false;
    }
    audio.set_preview_volume(g_settings.bgm_volume);
    audio.set_preview_fade_gain(0.65f);
    audio.seek_preview(0.0);
    audio.play_preview(true);

    const std::filesystem::path tap = app_paths::audio_root() / "HitSound_RayTap.mp3";
    audio.play_se(path_utils::to_utf8(tap), 0.6f);
    metronome_elapsed_ms_ = 0.0;
    metronome_next_tick_ = 480;
    error_.clear();
    return true;
}

void song_create_scene::stop_timing_preview() {
    metronome_enabled_ = false;
    metronome_elapsed_ms_ = 0.0;
    metronome_next_tick_ = 0;
    audio_manager::instance().stop_preview();
}

bool song_create_scene::import_midi_timing(const std::string& midi_path) {
    return song_create::timing_controller::import_midi_timing(
        timing_context(bpm_input_, timing_bar_input_, timing_bpm_input_, timing_numerator_input_,
                       timing_denominator_input_, timing_events_, selected_timing_event_index_,
                       timing_event_scroll_offset_, timing_import_status_, error_),
        midi_path);
}

std::vector<timing_event> song_create_scene::validated_timing_events(float base_bpm, bool& ok) {
    return song_create::timing_controller::validated_events(
        timing_context(bpm_input_, timing_bar_input_, timing_bpm_input_, timing_numerator_input_,
                       timing_denominator_input_, timing_events_, selected_timing_event_index_,
                       timing_event_scroll_offset_, timing_import_status_, error_),
        base_bpm,
        ok);
}

void song_create_scene::update_metronome(float dt) {
    if (!metronome_enabled_ || !selected_timing_event_index_.has_value() ||
        *selected_timing_event_index_ >= timing_events_.size()) {
        metronome_elapsed_ms_ = 0.0;
        return;
    }
    if (!audio_manager::instance().is_preview_playing()) {
        stop_timing_preview();
        return;
    }

    int offset_ms = 0;
    if (!offset_input_.value.empty()) {
        parse_int_text(offset_input_.value, offset_ms);
    }

    timing_engine engine;
    try {
        engine.init(timing_events_, 480, offset_ms);
    } catch (...) {
        stop_timing_preview();
        error_ = "Song timing contains an invalid BPM or time signature.";
        return;
    }

    const double current_ms = audio_manager::instance().get_preview_position_seconds() * 1000.0;
    const int current_tick = std::max(0, engine.ms_to_tick(current_ms));
    if (metronome_next_tick_ <= 0 || metronome_next_tick_ < current_tick - 480) {
        metronome_next_tick_ = current_tick;
    }

    const editor_meter_map meter_map = song_create::timing_service::build_meter_map(timing_events_, 480);
    int safety = 0;
    while (current_tick >= metronome_next_tick_ && safety++ < 16) {
        const editor_meter_map::bar_beat_position position = meter_map.bar_beat_at_tick(metronome_next_tick_);
        const bool downbeat = position.beat == 1;
        const std::filesystem::path tap = app_paths::audio_root() /
            (downbeat ? "HitSound_RayTap.mp3" : "HitSound_Tap.mp3");
        audio_manager::instance().play_se(path_utils::to_utf8(tap), downbeat ? 0.6f : 0.55f);
        metronome_next_tick_ += song_create::timing_service::beat_step_ticks_at(engine, metronome_next_tick_, 480);
    }
}

void song_create_scene::update(float dt) {
    ui::begin_input_frame();
    update_metronome(dt);

    switch (current_step_) {
        case step::song_metadata: update_song_metadata(); break;
        case step::song_saved: update_song_saved(); break;
    }
}

void song_create_scene::draw() {
    const auto& t = *g_theme;
    const char* content_title = is_edit_mode() ? "Edit Song" : "New Song";
    const char* content_subtitle = is_edit_mode() ? "Update song metadata" : "Enter song metadata";
    switch (current_step_) {
        case step::song_metadata:
            break;
        case step::song_saved:
            content_title = "Song Created";
            content_subtitle = "Choose the next action";
            break;
    }

    virtual_screen::begin_ui();
    if (timing_modal_open_) {
        ui::register_hit_region(kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(kTimingModalRect, kModalLayer);
    }
    draw_scene_background(t);
    ui::draw_header_block(kHeaderRect, content_title, content_subtitle);

    switch (current_step_) {
        case step::song_metadata:
            ui::section(kFormCardRect);
            draw_song_metadata();
            break;
        case step::song_saved:
            ui::section(kDecisionCardRect);
            draw_song_saved();
            break;
    }

    if (timing_modal_open_) {
        const song_create::timing_panel::modal_result timing_modal =
            song_create::timing_panel::draw_modal(
            {
                bpm_input_, offset_input_, timing_bar_input_, timing_bpm_input_,
                timing_numerator_input_, timing_denominator_input_, timing_events_,
                selected_timing_event_index_, metronome_enabled_,
                timing_event_scroll_offset_, timing_event_scrollbar_dragging_,
                timing_event_scrollbar_drag_offset_, timing_import_status_, error_,
            },
            {
                [this] { ensure_timing_events_initialized(); },
            },
            {kScreenRect, kTimingModalRect, kTextInputLabelWidth, kLayer, kModalLayer});
        const song_create::timing_panel::editor_result& timing_editor = timing_modal.editor;
        if (timing_editor.event_scroll_changed) {
            timing_event_scroll_offset_ = timing_editor.event_scroll_offset;
        }
        if (timing_editor.event_scrollbar_drag_state_changed) {
            timing_event_scrollbar_dragging_ = timing_editor.event_scrollbar_dragging;
            timing_event_scrollbar_drag_offset_ = timing_editor.event_scrollbar_drag_offset;
        }
        if (timing_editor.selected_event_index.has_value() && flush_selected_timing_event_inputs()) {
            selected_timing_event_index_ = *timing_editor.selected_event_index;
            sync_selected_timing_inputs();
            error_.clear();
        }
        if (timing_editor.add_event_type.has_value()) {
            add_timing_event(*timing_editor.add_event_type);
        }
        if (timing_editor.delete_selected_event_requested) {
            delete_selected_timing_event();
        }
        if (timing_editor.stop_metronome_requested) {
            stop_timing_preview();
        }
        if (timing_editor.start_metronome_requested && start_timing_preview()) {
            metronome_enabled_ = true;
        }
        if (timing_modal.import_midi_requested) {
            const std::string path = file_dialog::open_midi_file();
            if (!path.empty()) {
                import_midi_timing(path);
            }
        }
        if (timing_modal.close_requested) {
            close_timing_modal();
        }
    }

    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void song_create_scene::update_song_metadata() {
    if (timing_modal_open_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            close_timing_modal();
        }
        return;
    }

    if (jacket_picker_.is_open()) {
        jacket_picker_.update();
        if (jacket_picker_.consume_accept()) {
            jacket_path_input_.value = jacket_picker_.source_path();
            jacket_crop_source_ = jacket_picker_.source_path();
            error_.clear();
        } else if (jacket_picker_.consume_cancel()) {
            jacket_crop_source_.clear();
            error_.clear();
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        go_back_to_song_select(editing_song_.has_value() ? editing_song_->meta.song_id : "");
        return;
    }
}

void song_create_scene::update_song_saved() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        go_back_to_song_select(created_song_.meta.song_id);
        return;
    }
}

void song_create_scene::draw_song_metadata() {
    const song_create::form_panel::result result = song_create::form_panel::draw(
        {
            title_input_, artist_input_, genre_search_input_, keyword_input_,
            audio_path_input_, jacket_path_input_, preview_ms_input_,
            selected_genres_, selected_keywords_, jacket_picker_, error_,
        },
        {
            [this](Rectangle rect) {
                const song_create::timing_panel::summary_result timing_summary =
                    song_create::timing_panel::draw_summary(
                    rect,
                    {
                        bpm_input_, offset_input_, timing_bar_input_, timing_bpm_input_,
                        timing_numerator_input_, timing_denominator_input_, timing_events_,
                        selected_timing_event_index_, metronome_enabled_,
                        timing_event_scroll_offset_, timing_event_scrollbar_dragging_,
                        timing_event_scrollbar_drag_offset_, timing_import_status_, error_,
                    },
                    {
                        [this] { ensure_timing_events_initialized(); },
                    },
                    {kScreenRect, kTimingModalRect, kTextInputLabelWidth, kLayer, kModalLayer});
                return timing_summary.open_requested;
            },
        },
        {kFormX, kFormWidth, kFormStartY, kRowHeight, kRowGap, kTextInputLabelWidth, kLayer, is_edit_mode()});
    song_create::tag_editor::apply_genre_selector_result(
        selected_genres_, genre_search_input_, result.genre_selector);
    song_create::tag_editor::apply_keyword_editor_result(
        selected_keywords_, keyword_input_, result.keyword_editor);
    if (result.timing_summary_open_requested) {
        timing_modal_open_ = true;
        timing_import_status_.clear();
        ensure_timing_events_initialized();
    }
    if (result.browse_audio_requested) {
        const std::string path = file_dialog::open_audio_file();
        if (!path.empty()) {
            audio_path_input_.value = path;
        }
    }
    if (result.browse_jacket_requested) {
        const std::string path = file_dialog::open_image_file();
        if (!path.empty() && !jacket_picker_.open(path, error_)) {
            jacket_path_input_.value.clear();
            jacket_crop_source_.clear();
        }
    }
    if (result.submit_requested) {
        const bool editing = is_edit_mode();
        const bool success = editing ? save_song_edits() : create_song();
        if (success && !editing) {
            current_step_ = step::song_saved;
            error_.clear();
        }
    }
    if (result.cancel_requested) {
        go_back_to_song_select(editing_song_.has_value() ? editing_song_->meta.song_id : "");
    }
}

void song_create_scene::draw_song_saved() {
    const song_create::saved_view::action action =
        song_create::saved_view::draw(created_song_, kDecisionCardRect, kScreenW);
    if (action == song_create::saved_view::action::add_chart) {
        manager_.change_scene(std::make_unique<editor_scene>(manager_, created_song_, 4));
        return;
    }
    if (action == song_create::saved_view::action::add_later) {
        go_back_to_song_select(created_song_.meta.song_id);
    }
}


bool song_create_scene::create_song() {
    float base_bpm = 0.0f;
    if (!bpm_input_.value.empty()) {
        parse_float_text(bpm_input_.value, base_bpm);
    }

    const bool timing_input_active = timing_bar_input_.active || timing_bpm_input_.active ||
                                     timing_numerator_input_.active || timing_denominator_input_.active;
    if (timing_input_active && selected_timing_event_index_.has_value() && !apply_selected_timing_event()) {
        return false;
    }
    bool timing_ok = false;
    const std::vector<timing_event> timing_events = validated_timing_events(base_bpm, timing_ok);
    if (!timing_ok) {
        return false;
    }

    song_create::song_form_data form;
    form.title = title_input_.value;
    form.artist = artist_input_.value;
    form.audio_path = audio_path_input_.value;
    form.jacket_path = jacket_path_input_.value;
    form.bpm_text = bpm_input_.value;
    form.preview_ms_text = preview_ms_input_.value;
    form.offset_ms_text = offset_input_.value;
    form.genres = selected_genres_;
    form.keywords = selected_keywords_;
    form.timing_events = timing_events;

    const song_create::song_save_result result = song_create::create_song(
        form,
        [this](const fs::path& source_path, const fs::path& song_dir) {
            song_create::jacket_export_result exported;
            std::string error_message;
            exported.success = export_jacket_image(source_path, song_dir, exported.filename, error_message);
            exported.error = error_message;
            return exported;
        });
    if (!result.success) {
        error_ = result.error;
        return false;
    }

    created_song_ = result.song;
    return true;
}

bool song_create_scene::save_song_edits() {
    if (!editing_song_.has_value()) {
        error_ = "No song selected for editing.";
        return false;
    }
    float base_bpm = 0.0f;
    if (!bpm_input_.value.empty()) {
        parse_float_text(bpm_input_.value, base_bpm);
    }

    const bool timing_input_active = timing_bar_input_.active || timing_bpm_input_.active ||
                                     timing_numerator_input_.active || timing_denominator_input_.active;
    if (timing_input_active && selected_timing_event_index_.has_value() && !apply_selected_timing_event()) {
        return false;
    }
    bool timing_ok = false;
    const std::vector<timing_event> timing_events = validated_timing_events(base_bpm, timing_ok);
    if (!timing_ok) {
        return false;
    }

    song_create::song_form_data form;
    form.title = title_input_.value;
    form.artist = artist_input_.value;
    form.audio_path = audio_path_input_.value;
    form.jacket_path = jacket_path_input_.value;
    form.bpm_text = bpm_input_.value;
    form.preview_ms_text = preview_ms_input_.value;
    form.offset_ms_text = offset_input_.value;
    form.genres = selected_genres_;
    form.keywords = selected_keywords_;
    form.timing_events = timing_events;
    form.reuse_existing_jacket_when_source_matches = jacket_crop_source_ != jacket_path_input_.value;

    const song_create::song_save_result result = song_create::save_song_edits(
        *editing_song_,
        form,
        [this](const fs::path& source_path, const fs::path& song_dir) {
            song_create::jacket_export_result exported;
            std::string error_message;
            exported.success = export_jacket_image(source_path, song_dir, exported.filename, error_message);
            exported.error = error_message;
            return exported;
        });
    if (!result.success) {
        error_ = result.error;
        return false;
    }

    editing_song_ = result.song;
    created_song_ = result.song;
    error_.clear();
    go_back_to_song_select(result.song.meta.song_id);
    return true;
}

bool song_create_scene::export_jacket_image(const fs::path& source_path,
                                            const fs::path& song_dir,
                                            std::string& jacket_filename,
                                            std::string& error_message) {
    jacket_filename = "jacket.png";
    const fs::path jacket_dest = song_dir / jacket_filename;
    const std::string source_utf8 = path_utils::to_utf8(source_path);
    const std::string dest_utf8 = path_utils::to_utf8(jacket_dest);

    square_image_picker::export_result result;
    if (jacket_crop_source_ == source_utf8 && jacket_picker_.source_path() == source_utf8) {
        result = jacket_picker_.export_png(dest_utf8, {.output_size = 512});
    } else {
        result = square_image_picker::export_center_square_png(source_utf8, dest_utf8, {.output_size = 512});
    }

    if (!result.success) {
        error_message = result.message.empty() ? "Failed to export jacket image." : result.message;
        jacket_filename.clear();
        return false;
    }
    return true;
}

void song_create_scene::go_back_to_song_select(const std::string& preferred_song_id) {
    manager_.change_scene(song_select::make_seamless_create_scene(manager_, preferred_song_id));
}

bool song_create_scene::is_edit_mode() const {
    return editing_song_.has_value();
}
