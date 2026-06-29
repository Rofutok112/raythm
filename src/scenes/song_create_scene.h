#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "scene.h"
#include "shared/square_image_picker.h"
#include "song_create/song_create_timing_panel.h"
#include "ui_draw.h"
#include "ui_text_input.h"

// 新規楽曲作成 + 譜面作成フロー。
class song_create_scene final : public scene {
public:
    explicit song_create_scene(scene_manager& manager);
    song_create_scene(scene_manager& manager, song_data song_to_edit);

    void update(float dt) override;
    void draw() override;

private:
    enum class step { song_metadata, song_saved };

    void update_song_metadata();
    void update_song_saved();

    void draw_song_metadata();
    void draw_song_saved();

    bool create_song();
    bool save_song_edits();
    bool export_jacket_image(const std::filesystem::path& source_path,
                             const std::filesystem::path& song_dir,
                             std::string& jacket_filename,
                             std::string& error_message);
    void ensure_timing_events_initialized();
    void sync_selected_timing_inputs();
    void add_timing_event(timing_event_type type);
    void delete_selected_timing_event();
    bool apply_selected_timing_event();
    bool flush_selected_timing_event_inputs();
    void close_timing_modal();
    bool start_timing_preview();
    void stop_timing_preview();
    bool import_midi_timing(const std::string& midi_path);
    std::vector<timing_event> validated_timing_events(float base_bpm, bool& ok);
    void update_metronome(float dt);
    song_create::timing_panel::state_refs timing_panel_state_refs();
    song_create::timing_panel::callbacks timing_panel_callbacks();
    song_create::timing_panel::config timing_panel_config() const;
    void apply_timing_modal_result(const song_create::timing_panel::modal_result& result);
    void go_back_to_song_select(const std::string& preferred_song_id = "");
    bool is_edit_mode() const;

    step current_step_ = step::song_metadata;

    // Song metadata inputs
    ui::text_input_state title_input_;
    ui::text_input_state artist_input_;
    ui::text_input_state genre_search_input_;
    ui::text_input_state keyword_input_;
    ui::text_input_state bpm_input_;
    ui::text_input_state offset_input_;
    ui::text_input_state timing_bar_input_;
    ui::text_input_state timing_bpm_input_;
    ui::text_input_state timing_numerator_input_;
    ui::text_input_state timing_denominator_input_;
    ui::text_input_state audio_path_input_;
    ui::text_input_state jacket_path_input_;
    ui::text_input_state preview_ms_input_;
    std::vector<timing_event> timing_events_;
    std::optional<size_t> selected_timing_event_index_;
    bool metronome_enabled_ = false;
    bool timing_modal_open_ = false;
    double metronome_elapsed_ms_ = 0.0;
    int metronome_next_tick_ = 0;
    float timing_event_scroll_offset_ = 0.0f;
    bool timing_event_scrollbar_dragging_ = false;
    float timing_event_scrollbar_drag_offset_ = 0.0f;
    std::string timing_import_status_;
    std::vector<std::string> selected_genres_;
    std::vector<std::string> selected_keywords_;
    square_image_picker::state jacket_picker_;
    std::string jacket_crop_source_;

    // Created song data (stored after song creation)
    song_data created_song_;
    std::optional<song_data> editing_song_;

    std::string error_;
};
