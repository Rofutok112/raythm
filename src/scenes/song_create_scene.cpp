#include "song_create_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "app_paths.h"

#include "path_utils.h"
#include "editor_scene.h"
#include "file_dialog.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "song_writer.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_text.h"
#include "ui_text_input.h"
#include "uuid_util.h"
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
constexpr Rectangle kFormCardRect = {kFormX - kFormCardPaddingX, 177.0f, kFormWidth + kFormCardPaddingX * 2.0f, 780.0f};
constexpr Rectangle kDecisionCardRect = {525.0f, 315.0f, 870.0f, 330.0f};
constexpr float kTextInputLabelWidth = 180.0f;
constexpr float kBrowseWidth = 138.0f;
constexpr float kBrowseGap = 12.0f;
constexpr float kButtonTopGap = 24.0f;
constexpr float kButtonWidth = 270.0f;
constexpr float kButtonHeight = 66.0f;
constexpr float kButtonGap = 18.0f;
constexpr float kErrorTopGap = 18.0f;
constexpr float kErrorHeight = 36.0f;

constexpr Rectangle make_row(int index) {
    return {kFormX, kFormStartY + static_cast<float>(index) * (kRowHeight + kRowGap), kFormWidth, kRowHeight};
}

bool numeric_filter(int codepoint, const std::string&) {
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '.';
}

bool int_filter(int codepoint, const std::string&) {
    return codepoint >= '0' && codepoint <= '9';
}

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

std::string key_count_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

bool paths_match(const fs::path& left, const fs::path& right) {
    std::error_code ec;
    if (fs::exists(left, ec) && fs::exists(right, ec)) {
        if (fs::equivalent(left, right, ec) && !ec) {
            return true;
        }
    }

    return left.lexically_normal() == right.lexically_normal();
}

}  // namespace

song_create_scene::song_create_scene(scene_manager& manager)
    : scene(manager) {
}

song_create_scene::song_create_scene(scene_manager& manager, song_data song_to_edit)
    : scene(manager), created_song_(song_to_edit), editing_song_(std::move(song_to_edit)) {
    const song_meta& meta = editing_song_->meta;
    title_input_.value = meta.title;
    artist_input_.value = meta.artist;
    bpm_input_.value = meta.base_bpm > 0.0f ? TextFormat("%.6g", meta.base_bpm) : "";
    preview_ms_input_.value = std::to_string(meta.preview_start_ms);
    sns_youtube_input_.value = meta.sns_youtube;
    sns_niconico_input_.value = meta.sns_niconico;
    sns_x_input_.value = meta.sns_x;

    if (!meta.audio_file.empty()) {
        audio_path_input_.value = path_utils::to_utf8(path_utils::join_utf8(editing_song_->directory, meta.audio_file));
    }
    if (!meta.jacket_file.empty()) {
        jacket_path_input_.value = path_utils::to_utf8(path_utils::join_utf8(editing_song_->directory, meta.jacket_file));
    }
}

void song_create_scene::update(float dt) {
    ui::begin_hit_regions();

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
    draw_scene_background(t);
    ui::draw_header_block(kHeaderRect, content_title, content_subtitle);

    switch (current_step_) {
        case step::song_metadata:
            ui::draw_section(kFormCardRect);
            draw_song_metadata();
            break;
        case step::song_saved:
            ui::draw_section(kDecisionCardRect);
            draw_song_saved();
            break;
    }

    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void song_create_scene::update_song_metadata() {
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
    int row = 0;

    ui::draw_text_input(make_row(row++), title_input_, "Title", "Song title",
                        nullptr, kLayer, 16, 128, wide_text_filter, kTextInputLabelWidth);

    ui::draw_text_input(make_row(row++), artist_input_, "Artist", "Artist name",
                        nullptr, kLayer, 16, 128, wide_text_filter, kTextInputLabelWidth);

    ui::draw_text_input(make_row(row++), bpm_input_, "BPM", "120.0",
                        nullptr, kLayer, 16, 16, numeric_filter, kTextInputLabelWidth);

    {
        const Rectangle audio_row = make_row(row++);
        const Rectangle input_rect = {audio_row.x, audio_row.y, audio_row.width - kBrowseWidth - kBrowseGap, audio_row.height};
        const Rectangle browse_rect = {audio_row.x + audio_row.width - kBrowseWidth, audio_row.y, kBrowseWidth, audio_row.height};

        ui::draw_text_input(input_rect, audio_path_input_, "Audio", "Select audio file...",
                            nullptr, kLayer, 16, 512, nullptr, kTextInputLabelWidth);

        if (ui::draw_button(browse_rect, "BROWSE", 14).clicked) {
            const std::string path = file_dialog::open_audio_file();
            if (!path.empty()) {
                audio_path_input_.value = path;
            }
        }
    }

    {
        const Rectangle jacket_row = make_row(row++);
        const Rectangle input_rect = {jacket_row.x, jacket_row.y, jacket_row.width - kBrowseWidth - kBrowseGap, jacket_row.height};
        const Rectangle browse_rect = {jacket_row.x + jacket_row.width - kBrowseWidth, jacket_row.y, kBrowseWidth, jacket_row.height};

        ui::draw_text_input(input_rect, jacket_path_input_, "Jacket", "Select image file... (optional)",
                            nullptr, kLayer, 16, 512, nullptr, kTextInputLabelWidth);

        if (ui::draw_button(browse_rect, "BROWSE", 14).clicked) {
            const std::string path = file_dialog::open_image_file();
            if (!path.empty()) {
                if (!jacket_picker_.open(path, error_)) {
                    jacket_path_input_.value.clear();
                    jacket_crop_source_.clear();
                }
            }
        }
    }

    ui::draw_text_input(make_row(row++), preview_ms_input_, "Preview (ms)", "0",
                        "0", kLayer, 16, 10, int_filter, kTextInputLabelWidth);

    ui::draw_text_input(make_row(row++), sns_youtube_input_, "YouTube", "URL (optional)",
                        nullptr, kLayer, 16, 256, nullptr, kTextInputLabelWidth);
    ui::draw_text_input(make_row(row++), sns_niconico_input_, "Niconico", "URL (optional)",
                        nullptr, kLayer, 16, 256, nullptr, kTextInputLabelWidth);
    ui::draw_text_input(make_row(row++), sns_x_input_, "X", "URL (optional)",
                        nullptr, kLayer, 16, 256, nullptr, kTextInputLabelWidth);

    const float button_y = kFormStartY + static_cast<float>(row) * (kRowHeight + kRowGap) + kButtonTopGap;
    const Rectangle create_rect = {kFormX + kFormWidth - kButtonWidth, button_y, kButtonWidth, kButtonHeight};
    const Rectangle cancel_rect = {kFormX + kFormWidth - kButtonWidth * 2.0f - kButtonGap, button_y, kButtonWidth, kButtonHeight};
    const char* submit_label = is_edit_mode() ? "SAVE" : "CREATE";
    const char* cancel_label = is_edit_mode() ? "BACK" : "CANCEL";

    if (ui::draw_button(create_rect, submit_label, 16).clicked) {
        const bool success = is_edit_mode() ? save_song_edits() : create_song();
        if (success && !is_edit_mode()) {
            current_step_ = step::song_saved;
            error_.clear();
        }
    }

    if (ui::draw_button(cancel_rect, cancel_label, 16).clicked) {
        go_back_to_song_select(editing_song_.has_value() ? editing_song_->meta.song_id : "");
        return;
    }

    if (!error_.empty()) {
        const Rectangle error_rect = {kFormX, button_y + kButtonHeight + kErrorTopGap, kFormWidth, kErrorHeight};
        ui::draw_text_in_rect(error_.c_str(), 14, error_rect, g_theme->error, ui::text_align::left);
    }

    if (jacket_picker_.is_open()) {
        jacket_picker_.draw();
    }
}

void song_create_scene::draw_song_saved() {
    constexpr float kCenterY = 465.0f;
    constexpr float kSavedButtonWidth = 330.0f;
    constexpr float kSavedButtonHeight = 72.0f;
    constexpr float kSavedButtonGap = 24.0f;
    const float total_width = kSavedButtonWidth * 2.0f + kSavedButtonGap;
    const float start_x = (static_cast<float>(kScreenW) - total_width) * 0.5f;

    const Rectangle title_rect = {kDecisionCardRect.x + 54.0f, kDecisionCardRect.y + 42.0f, kDecisionCardRect.width - 108.0f, 51.0f};
    const Rectangle song_rect = {kDecisionCardRect.x + 54.0f, kDecisionCardRect.y + 108.0f, kDecisionCardRect.width - 108.0f, 45.0f};
    const Rectangle msg_rect = {kDecisionCardRect.x + 54.0f, kDecisionCardRect.y + 177.0f, kDecisionCardRect.width - 108.0f, 45.0f};
    ui::draw_text_in_rect("Song has been created.", 24, title_rect, g_theme->text, ui::text_align::center);
    ui::draw_text_in_rect(created_song_.meta.title.c_str(), 22, song_rect, g_theme->text_secondary, ui::text_align::center);
    ui::draw_text_in_rect("What would you like to do next?", 20, msg_rect, g_theme->text_muted, ui::text_align::center);

    const Rectangle add_chart_rect = {start_x, kCenterY + 67.5f, kSavedButtonWidth, kSavedButtonHeight};
    const Rectangle add_later_rect = {start_x + kSavedButtonWidth + kSavedButtonGap, kCenterY + 67.5f,
                                      kSavedButtonWidth, kSavedButtonHeight};

    if (ui::draw_button(add_chart_rect, "ADD CHART", 16).clicked) {
        manager_.change_scene(std::make_unique<editor_scene>(manager_, created_song_, 4));
        return;
    }

    if (ui::draw_button(add_later_rect, "ADD LATER", 16).clicked) {
        go_back_to_song_select(created_song_.meta.song_id);
    }
}


bool song_create_scene::create_song() {
    if (title_input_.value.empty()) {
        error_ = "Title is required.";
        return false;
    }
    if (artist_input_.value.empty()) {
        error_ = "Artist is required.";
        return false;
    }
    if (audio_path_input_.value.empty()) {
        error_ = "Audio file is required.";
        return false;
    }

    const fs::path audio_source = path_utils::from_utf8(audio_path_input_.value);
    if (!fs::exists(audio_source)) {
        error_ = "Audio file not found: " + audio_path_input_.value;
        return false;
    }

    float base_bpm = 0.0f;
    if (!bpm_input_.value.empty()) {
        try {
            base_bpm = std::stof(bpm_input_.value);
        } catch (...) {
            error_ = "Invalid BPM value.";
            return false;
        }
    }

    int preview_ms = 0;
    if (!preview_ms_input_.value.empty()) {
        try {
            preview_ms = std::stoi(preview_ms_input_.value);
        } catch (...) {
            error_ = "Invalid preview start value.";
            return false;
        }
    }

    const std::string song_id = generate_uuid();
    app_paths::ensure_directories();
    const fs::path song_dir = app_paths::song_dir(song_id);
    fs::create_directories(song_dir);

    const std::string audio_filename = path_utils::to_utf8(audio_source.filename());
    const fs::path audio_dest = song_dir / audio_filename;
    try {
        fs::copy_file(audio_source, audio_dest, fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        error_ = std::string("Failed to copy audio file: ") + e.what();
        return false;
    }

    std::string jacket_filename;
    if (!jacket_path_input_.value.empty()) {
        const fs::path jacket_source = path_utils::from_utf8(jacket_path_input_.value);
        if (!fs::exists(jacket_source)) {
            error_ = "Jacket file not found: " + jacket_path_input_.value;
            return false;
        }
        if (!export_jacket_image(jacket_source, song_dir, jacket_filename)) {
            return false;
        }
    }

    song_meta meta;
    meta.song_id = song_id;
    meta.title = title_input_.value;
    meta.artist = artist_input_.value;
    meta.base_bpm = base_bpm;
    meta.audio_file = audio_filename;
    meta.jacket_file = jacket_filename;
    meta.preview_start_ms = preview_ms;
    meta.preview_start_seconds = static_cast<float>(preview_ms) / 1000.0f;
    meta.song_version = 1;
    meta.sns_youtube = sns_youtube_input_.value;
    meta.sns_niconico = sns_niconico_input_.value;
    meta.sns_x = sns_x_input_.value;

    if (!song_writer::write_song_json(meta, path_utils::to_utf8(song_dir))) {
        error_ = "Failed to write song.json.";
        return false;
    }

    created_song_.meta = meta;
    created_song_.directory = path_utils::to_utf8(song_dir);
    created_song_.chart_paths.clear();

    return true;
}

bool song_create_scene::save_song_edits() {
    if (!editing_song_.has_value()) {
        error_ = "No song selected for editing.";
        return false;
    }
    if (title_input_.value.empty()) {
        error_ = "Title is required.";
        return false;
    }
    if (artist_input_.value.empty()) {
        error_ = "Artist is required.";
        return false;
    }
    if (audio_path_input_.value.empty()) {
        error_ = "Audio file is required.";
        return false;
    }

    const fs::path song_dir = path_utils::from_utf8(editing_song_->directory);
    const fs::path audio_source = path_utils::from_utf8(audio_path_input_.value);
    if (!fs::exists(audio_source)) {
        error_ = "Audio file not found: " + audio_path_input_.value;
        return false;
    }

    float base_bpm = 0.0f;
    if (!bpm_input_.value.empty()) {
        try {
            base_bpm = std::stof(bpm_input_.value);
        } catch (...) {
            error_ = "Invalid BPM value.";
            return false;
        }
    }

    int preview_ms = 0;
    if (!preview_ms_input_.value.empty()) {
        try {
            preview_ms = std::stoi(preview_ms_input_.value);
        } catch (...) {
            error_ = "Invalid preview start value.";
            return false;
        }
    }

    std::string audio_filename = editing_song_->meta.audio_file;
    const fs::path current_audio_path = path_utils::join_utf8(editing_song_->directory, editing_song_->meta.audio_file);
    if (!paths_match(audio_source, current_audio_path)) {
        audio_filename = path_utils::to_utf8(audio_source.filename());
        const fs::path audio_dest = song_dir / audio_filename;
        try {
            fs::copy_file(audio_source, audio_dest, fs::copy_options::overwrite_existing);
        } catch (const std::exception& e) {
            error_ = std::string("Failed to copy audio file: ") + e.what();
            return false;
        }
    }

    std::string jacket_filename = editing_song_->meta.jacket_file;
    if (jacket_path_input_.value.empty()) {
        jacket_filename.clear();
    } else {
        const fs::path jacket_source = path_utils::from_utf8(jacket_path_input_.value);
        if (!fs::exists(jacket_source)) {
            error_ = "Jacket file not found: " + jacket_path_input_.value;
            return false;
        }

        const fs::path current_jacket_path = editing_song_->meta.jacket_file.empty()
            ? fs::path()
            : path_utils::join_utf8(editing_song_->directory, editing_song_->meta.jacket_file);
        if (!editing_song_->meta.jacket_file.empty() &&
            paths_match(jacket_source, current_jacket_path) &&
            jacket_crop_source_ != path_utils::to_utf8(jacket_source)) {
            jacket_filename = editing_song_->meta.jacket_file;
        } else {
            if (!export_jacket_image(jacket_source, song_dir, jacket_filename)) {
                return false;
            }
        }
    }

    song_meta meta = editing_song_->meta;
    meta.title = title_input_.value;
    meta.artist = artist_input_.value;
    meta.base_bpm = base_bpm;
    meta.audio_file = audio_filename;
    meta.jacket_file = jacket_filename;
    meta.preview_start_ms = preview_ms;
    meta.preview_start_seconds = static_cast<float>(preview_ms) / 1000.0f;
    meta.sns_youtube = sns_youtube_input_.value;
    meta.sns_niconico = sns_niconico_input_.value;
    meta.sns_x = sns_x_input_.value;
    if (meta.song_version <= 0) {
        meta.song_version = 1;
    }

    if (!song_writer::write_song_json(meta, path_utils::to_utf8(song_dir))) {
        error_ = "Failed to write song.json.";
        return false;
    }

    editing_song_->meta = meta;
    created_song_ = *editing_song_;
    error_.clear();
    go_back_to_song_select(meta.song_id);
    return true;
}

bool song_create_scene::export_jacket_image(const fs::path& source_path,
                                            const fs::path& song_dir,
                                            std::string& jacket_filename) {
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
        error_ = result.message.empty() ? "Failed to export jacket image." : result.message;
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
