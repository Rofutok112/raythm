#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "data_models.h"
#include "scene.h"
#include "shared/square_image_picker.h"
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
                             std::string& jacket_filename);
    void go_back_to_song_select(const std::string& preferred_song_id = "");
    bool is_edit_mode() const;

    step current_step_ = step::song_metadata;

    // Song metadata inputs
    ui::text_input_state title_input_;
    ui::text_input_state artist_input_;
    ui::text_input_state bpm_input_;
    ui::text_input_state audio_path_input_;
    ui::text_input_state jacket_path_input_;
    ui::text_input_state preview_ms_input_;
    ui::text_input_state sns_youtube_input_;
    ui::text_input_state sns_niconico_input_;
    ui::text_input_state sns_x_input_;
    square_image_picker::state jacket_picker_;
    std::string jacket_crop_source_;

    // Created song data (stored after song creation)
    song_data created_song_;
    std::optional<song_data> editing_song_;

    std::string error_;
};
