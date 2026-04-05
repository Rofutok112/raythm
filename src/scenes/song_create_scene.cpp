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
#include "song_select_scene.h"
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
constexpr Rectangle kCardFrameRect = ui::place(kScreenRect, 750.0f, 660.0f,
                                               ui::anchor::top_left, ui::anchor::top_left,
                                               {265.0f, 30.0f});
constexpr Rectangle kHeaderRect = ui::place(kCardFrameRect, 620.0f, 60.0f,
                                            ui::anchor::top_left, ui::anchor::top_left,
                                            {34.0f, 28.0f});

constexpr float kFormWidth = 700.0f;
constexpr float kRowHeight = 42.0f;
constexpr float kRowGap = 6.0f;
constexpr float kFormStartY = 142.0f;
constexpr float kFormX = (static_cast<float>(kScreenW) - kFormWidth) * 0.5f;
constexpr Rectangle kFormCardRect = {kFormX - 26.0f, 118.0f, kFormWidth + 52.0f, 520.0f};
constexpr Rectangle kDecisionCardRect = {350.0f, 210.0f, 580.0f, 220.0f};
constexpr Rectangle kChartCardRect = {kFormX - 26.0f, 166.0f, kFormWidth + 52.0f, 420.0f};

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
        case step::chart_metadata: update_chart_metadata(); break;
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
        case step::chart_metadata:
            content_title = "New Chart";
            content_subtitle = "Enter chart metadata";
            break;
    }

    virtual_screen::begin();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);
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
        case step::chart_metadata:
            ui::draw_section(kChartCardRect);
            draw_chart_metadata();
            break;
    }

    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void song_create_scene::update_song_metadata() {
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

void song_create_scene::update_chart_metadata() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        current_step_ = step::song_saved;
        error_.clear();
    }
}

void song_create_scene::draw_song_metadata() {
    int row = 0;

    ui::draw_text_input(make_row(row++), title_input_, "Title", "Song title",
                        nullptr, kLayer, 16, 128, wide_text_filter, 120.0f);

    ui::draw_text_input(make_row(row++), artist_input_, "Artist", "Artist name",
                        nullptr, kLayer, 16, 128, wide_text_filter, 120.0f);

    ui::draw_text_input(make_row(row++), bpm_input_, "BPM", "120.0",
                        nullptr, kLayer, 16, 16, numeric_filter, 120.0f);

    {
        constexpr float kBrowseWidth = 92.0f;
        const Rectangle audio_row = make_row(row++);
        const Rectangle input_rect = {audio_row.x, audio_row.y, audio_row.width - kBrowseWidth - 8.0f, audio_row.height};
        const Rectangle browse_rect = {audio_row.x + audio_row.width - kBrowseWidth, audio_row.y, kBrowseWidth, audio_row.height};

        ui::draw_text_input(input_rect, audio_path_input_, "Audio", "Select audio file...",
                            nullptr, kLayer, 16, 512, nullptr, 120.0f);

        if (ui::draw_button(browse_rect, "BROWSE", 14).clicked) {
            const std::string path = file_dialog::open_audio_file();
            if (!path.empty()) {
                audio_path_input_.value = path;
            }
        }
    }

    {
        constexpr float kBrowseWidth = 92.0f;
        const Rectangle jacket_row = make_row(row++);
        const Rectangle input_rect = {jacket_row.x, jacket_row.y, jacket_row.width - kBrowseWidth - 8.0f, jacket_row.height};
        const Rectangle browse_rect = {jacket_row.x + jacket_row.width - kBrowseWidth, jacket_row.y, kBrowseWidth, jacket_row.height};

        ui::draw_text_input(input_rect, jacket_path_input_, "Jacket", "Select image file... (optional)",
                            nullptr, kLayer, 16, 512, nullptr, 120.0f);

        if (ui::draw_button(browse_rect, "BROWSE", 14).clicked) {
            const std::string path = file_dialog::open_image_file();
            if (!path.empty()) {
                jacket_path_input_.value = path;
            }
        }
    }

    ui::draw_text_input(make_row(row++), preview_ms_input_, "Preview (ms)", "0",
                        "0", kLayer, 16, 10, int_filter, 120.0f);

    ui::draw_text_input(make_row(row++), sns_youtube_input_, "YouTube", "URL (optional)",
                        nullptr, kLayer, 16, 256, nullptr, 120.0f);
    ui::draw_text_input(make_row(row++), sns_niconico_input_, "Niconico", "URL (optional)",
                        nullptr, kLayer, 16, 256, nullptr, 120.0f);
    ui::draw_text_input(make_row(row++), sns_x_input_, "X", "URL (optional)",
                        nullptr, kLayer, 16, 256, nullptr, 120.0f);

    const float button_y = kFormStartY + static_cast<float>(row) * (kRowHeight + kRowGap) + 16.0f;
    constexpr float kButtonWidth = 180.0f;
    constexpr float kButtonHeight = 44.0f;
    const Rectangle create_rect = {kFormX + kFormWidth - kButtonWidth, button_y, kButtonWidth, kButtonHeight};
    const Rectangle cancel_rect = {kFormX + kFormWidth - kButtonWidth * 2.0f - 12.0f, button_y, kButtonWidth, kButtonHeight};
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
        const Rectangle error_rect = {kFormX, button_y + kButtonHeight + 12.0f, kFormWidth, 24.0f};
        ui::draw_text_in_rect(error_.c_str(), 14, error_rect, g_theme->error, ui::text_align::left);
    }
}

void song_create_scene::draw_song_saved() {
    constexpr float kCenterY = 310.0f;
    constexpr float kButtonWidth = 220.0f;
    constexpr float kButtonHeight = 48.0f;
    constexpr float kGap = 16.0f;
    const float total_width = kButtonWidth * 2.0f + kGap;
    const float start_x = (static_cast<float>(kScreenW) - total_width) * 0.5f;

    const Rectangle title_rect = {kDecisionCardRect.x + 36.0f, kDecisionCardRect.y + 28.0f, kDecisionCardRect.width - 72.0f, 34.0f};
    const Rectangle song_rect = {kDecisionCardRect.x + 36.0f, kDecisionCardRect.y + 72.0f, kDecisionCardRect.width - 72.0f, 30.0f};
    const Rectangle msg_rect = {kDecisionCardRect.x + 36.0f, kDecisionCardRect.y + 118.0f, kDecisionCardRect.width - 72.0f, 30.0f};
    ui::draw_text_in_rect("Song has been created.", 24, title_rect, g_theme->text, ui::text_align::center);
    ui::draw_text_in_rect(created_song_.meta.title.c_str(), 22, song_rect, g_theme->text_secondary, ui::text_align::center);
    ui::draw_text_in_rect("What would you like to do next?", 20, msg_rect, g_theme->text_muted, ui::text_align::center);

    const Rectangle add_chart_rect = {start_x, kCenterY + 45, kButtonWidth, kButtonHeight};
    const Rectangle add_later_rect = {start_x + kButtonWidth + kGap, kCenterY + 45, kButtonWidth, kButtonHeight};

    if (ui::draw_button(add_chart_rect, "ADD CHART", 16).clicked) {
        manager_.change_scene(std::make_unique<editor_scene>(manager_, created_song_, 4));
        return;
    }

    if (ui::draw_button(add_later_rect, "ADD LATER", 16).clicked) {
        go_back_to_song_select(created_song_.meta.song_id);
    }
}

void song_create_scene::draw_chart_metadata() {
    int row = 0;

    ui::draw_text_input(make_row(row++), difficulty_input_, "Difficulty", "Normal",
                        "Normal", kLayer, 16, 32, wide_text_filter, 120.0f);

    {
        const Rectangle key_row = make_row(row++);
        const ui::selector_state sel = ui::draw_value_selector(
            key_row, "Key Mode", key_count_label(chart_key_count_).c_str(),
            16, 26.0f, 120.0f, 12.0f);

        if (sel.left.clicked || sel.right.clicked) {
            chart_key_count_ = (chart_key_count_ == 4) ? 6 : 4;
        }
    }

    ui::draw_text_input(make_row(row++), level_input_, "Level", "0.0",
                        "0.0", kLayer, 16, 4, numeric_filter, 120.0f);

    ui::draw_text_input(make_row(row++), chart_author_input_, "Author", "Your name",
                        nullptr, kLayer, 16, 64, wide_text_filter, 120.0f);

    const float button_y = kFormStartY + static_cast<float>(row) * (kRowHeight + kRowGap) + 16.0f;
    constexpr float kButtonWidth = 220.0f;
    constexpr float kButtonHeight = 44.0f;
    const Rectangle create_rect = {kFormX + kFormWidth - kButtonWidth, button_y, kButtonWidth, kButtonHeight};
    const Rectangle back_rect = {kFormX + kFormWidth - kButtonWidth * 2.0f - 12.0f, button_y, kButtonWidth, kButtonHeight};

    if (ui::draw_button(create_rect, "CREATE & OPEN EDITOR", 14).clicked) {
        if (create_chart_and_open_editor()) {
            return;
        }
    }

    if (ui::draw_button(back_rect, "BACK", 16).clicked) {
        current_step_ = step::song_saved;
        error_.clear();
    }

    if (!error_.empty()) {
        const Rectangle error_rect = {kFormX, button_y + kButtonHeight + 12.0f, kFormWidth, 24.0f};
        ui::draw_text_in_rect(error_.c_str(), 14, error_rect, g_theme->error, ui::text_align::left);
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
        if (fs::exists(jacket_source)) {
            jacket_filename = path_utils::to_utf8(jacket_source.filename());
            const fs::path jacket_dest = song_dir / jacket_filename;
            try {
                fs::copy_file(jacket_source, jacket_dest, fs::copy_options::overwrite_existing);
            } catch (...) {
            }
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
    created_song_.source = content_source::app_data;
    created_song_.can_edit = true;
    created_song_.can_delete = true;

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
        if (!editing_song_->meta.jacket_file.empty() && paths_match(jacket_source, current_jacket_path)) {
            jacket_filename = editing_song_->meta.jacket_file;
        } else {
            jacket_filename = path_utils::to_utf8(jacket_source.filename());
            const fs::path jacket_dest = song_dir / jacket_filename;
            try {
                fs::copy_file(jacket_source, jacket_dest, fs::copy_options::overwrite_existing);
            } catch (const std::exception& e) {
                error_ = std::string("Failed to copy jacket file: ") + e.what();
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

bool song_create_scene::create_chart_and_open_editor() {
    if (difficulty_input_.value.empty()) {
        error_ = "Difficulty is required.";
        return false;
    }

    float level = 0.0f;
    if (!level_input_.value.empty()) {
        try {
            level = std::stof(level_input_.value);
        } catch (...) {
            error_ = "Invalid level value.";
            return false;
        }
    }

    chart_meta meta;
    meta.chart_id = generate_uuid();
    meta.song_id = created_song_.meta.song_id;
    meta.key_count = chart_key_count_;
    meta.difficulty = difficulty_input_.value;
    meta.level = level;
    meta.chart_author = chart_author_input_.value;
    meta.is_public = false;
    meta.format_version = 3;
    meta.resolution = 1920;
    meta.offset = 0;

    manager_.change_scene(std::make_unique<editor_scene>(manager_, created_song_, meta));
    return true;
}

void song_create_scene::go_back_to_song_select(const std::string& preferred_song_id) {
    manager_.change_scene(std::make_unique<song_select_scene>(manager_, preferred_song_id));
}

bool song_create_scene::is_edit_mode() const {
    return editing_song_.has_value();
}
