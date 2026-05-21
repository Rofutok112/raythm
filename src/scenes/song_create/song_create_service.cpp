#include "song_create/song_create_service.h"

#include <filesystem>
#include <stdexcept>

#include "app_paths.h"
#include "path_utils.h"
#include "song_writer.h"
#include "uuid_util.h"

namespace song_create {
namespace {

namespace fs = std::filesystem;

bool parse_float_field(const std::string& text, float& value, std::string& error, const char* label) {
    if (text.empty()) {
        value = 0.0f;
        return true;
    }
    try {
        value = std::stof(text);
    } catch (...) {
        error = std::string("Invalid ") + label + " value.";
        return false;
    }
    return true;
}

bool parse_int_field(const std::string& text, int& value, std::string& error, const char* label) {
    if (text.empty()) {
        value = 0;
        return true;
    }
    try {
        value = std::stoi(text);
    } catch (...) {
        error = std::string("Invalid ") + label + " value.";
        return false;
    }
    return true;
}

bool paths_match(const fs::path& left, const fs::path& right) {
    std::error_code ec;
    if (left.empty() || right.empty()) {
        return left.empty() && right.empty();
    }
    if (fs::equivalent(left, right, ec)) {
        return true;
    }
    ec.clear();
    return fs::weakly_canonical(left, ec) == fs::weakly_canonical(right, ec);
}

bool validate_required_fields(const song_form_data& form, song_save_result& result) {
    if (form.title.empty()) {
        result.error = "Title is required.";
        return false;
    }
    if (form.artist.empty()) {
        result.error = "Artist is required.";
        return false;
    }
    if (form.audio_path.empty()) {
        result.error = "Audio file is required.";
        return false;
    }
    return true;
}

bool parse_numeric_fields(const song_form_data& form,
                          float& base_bpm,
                          int& preview_ms,
                          int& offset_ms,
                          song_save_result& result) {
    if (!parse_float_field(form.bpm_text, base_bpm, result.error, "BPM")) {
        return false;
    }
    if (!parse_int_field(form.preview_ms_text, preview_ms, result.error, "preview start")) {
        return false;
    }
    if (!parse_int_field(form.offset_ms_text, offset_ms, result.error, "song offset")) {
        return false;
    }
    return true;
}

void apply_form_metadata(song_meta& meta,
                         const song_form_data& form,
                         float base_bpm,
                         int preview_ms,
                         int offset_ms,
                         const std::string& audio_filename,
                         const std::string& jacket_filename) {
    meta.title = form.title;
    meta.artist = form.artist;
    meta.genres = form.genres;
    meta.genre = meta.genres.empty() ? "" : meta.genres.front();
    meta.keywords = form.keywords;
    meta.base_bpm = base_bpm;
    meta.offset = offset_ms;
    meta.has_offset = true;
    meta.timing_events = form.timing_events;
    meta.audio_file = audio_filename;
    meta.jacket_file = jacket_filename;
    meta.preview_start_ms = preview_ms;
    meta.preview_start_seconds = static_cast<float>(preview_ms) / 1000.0f;
    if (meta.song_version <= 0) {
        meta.song_version = 1;
    }
}

bool copy_audio_if_needed(const fs::path& source,
                          const fs::path& current_path,
                          const fs::path& song_dir,
                          std::string& audio_filename,
                          song_save_result& result) {
    if (!fs::exists(source)) {
        result.error = "Audio file not found: " + path_utils::to_utf8(source);
        return false;
    }
    if (!current_path.empty() && paths_match(source, current_path)) {
        return true;
    }

    audio_filename = path_utils::to_utf8(source.filename());
    const fs::path destination = song_dir / audio_filename;
    try {
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        result.error = std::string("Failed to copy audio file: ") + e.what();
        return false;
    }
    return true;
}

bool export_jacket_if_present(const song_form_data& form,
                              const fs::path& song_dir,
                              const fs::path& current_jacket_path,
                              const std::string& existing_jacket_filename,
                              const jacket_exporter& export_jacket,
                              std::string& jacket_filename,
                              song_save_result& result) {
    if (form.jacket_path.empty()) {
        jacket_filename.clear();
        return true;
    }

    const fs::path source = path_utils::from_utf8(form.jacket_path);
    if (!fs::exists(source)) {
        result.error = "Jacket file not found: " + form.jacket_path;
        return false;
    }

    if (!existing_jacket_filename.empty() &&
        paths_match(source, current_jacket_path) &&
        form.reuse_existing_jacket_when_source_matches) {
        jacket_filename = existing_jacket_filename;
        return true;
    }

    if (!export_jacket) {
        result.error = "Failed to export jacket image.";
        return false;
    }
    const jacket_export_result exported = export_jacket(source, song_dir);
    if (!exported.success) {
        result.error = exported.error.empty() ? "Failed to export jacket image." : exported.error;
        return false;
    }
    jacket_filename = exported.filename;
    return true;
}

}  // namespace

song_save_result create_song(const song_form_data& form, const jacket_exporter& export_jacket) {
    song_save_result result;
    if (!validate_required_fields(form, result)) {
        return result;
    }

    float base_bpm = 0.0f;
    int preview_ms = 0;
    int offset_ms = 0;
    if (!parse_numeric_fields(form, base_bpm, preview_ms, offset_ms, result)) {
        return result;
    }

    const fs::path audio_source = path_utils::from_utf8(form.audio_path);
    const std::string song_id = generate_uuid();
    app_paths::ensure_directories();
    const fs::path song_dir = app_paths::song_dir(song_id);
    fs::create_directories(song_dir);

    std::string audio_filename = path_utils::to_utf8(audio_source.filename());
    if (!copy_audio_if_needed(audio_source, {}, song_dir, audio_filename, result)) {
        return result;
    }

    std::string jacket_filename;
    if (!export_jacket_if_present(form, song_dir, {}, "", export_jacket, jacket_filename, result)) {
        return result;
    }

    song_meta meta;
    meta.song_id = song_id;
    apply_form_metadata(meta, form, base_bpm, preview_ms, offset_ms, audio_filename, jacket_filename);

    if (!song_writer::write_song_json(meta, path_utils::to_utf8(song_dir))) {
        result.error = "Failed to write song.json.";
        return result;
    }

    result.song.meta = meta;
    result.song.directory = path_utils::to_utf8(song_dir);
    result.success = true;
    return result;
}

song_save_result save_song_edits(const song_data& editing_song,
                                 const song_form_data& form,
                                 const jacket_exporter& export_jacket) {
    song_save_result result;
    if (!validate_required_fields(form, result)) {
        return result;
    }

    float base_bpm = 0.0f;
    int preview_ms = 0;
    int offset_ms = 0;
    if (!parse_numeric_fields(form, base_bpm, preview_ms, offset_ms, result)) {
        return result;
    }

    const fs::path song_dir = path_utils::from_utf8(editing_song.directory);
    const fs::path audio_source = path_utils::from_utf8(form.audio_path);
    std::string audio_filename = editing_song.meta.audio_file;
    const fs::path current_audio_path = path_utils::join_utf8(editing_song.directory, editing_song.meta.audio_file);
    if (!copy_audio_if_needed(audio_source, current_audio_path, song_dir, audio_filename, result)) {
        return result;
    }

    std::string jacket_filename = editing_song.meta.jacket_file;
    const fs::path current_jacket_path = editing_song.meta.jacket_file.empty()
        ? fs::path()
        : path_utils::join_utf8(editing_song.directory, editing_song.meta.jacket_file);
    if (!export_jacket_if_present(form,
                                  song_dir,
                                  current_jacket_path,
                                  editing_song.meta.jacket_file,
                                  export_jacket,
                                  jacket_filename,
                                  result)) {
        return result;
    }

    song_meta meta = editing_song.meta;
    apply_form_metadata(meta, form, base_bpm, preview_ms, offset_ms, audio_filename, jacket_filename);

    if (!song_writer::write_song_json(meta, path_utils::to_utf8(song_dir))) {
        result.error = "Failed to write song.json.";
        return result;
    }

    result.song = editing_song;
    result.song.meta = meta;
    result.success = true;
    return result;
}

}  // namespace song_create
