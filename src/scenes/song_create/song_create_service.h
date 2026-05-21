#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "data_models.h"

namespace song_create {

struct song_form_data {
    std::string title;
    std::string artist;
    std::string audio_path;
    std::string jacket_path;
    std::string bpm_text;
    std::string preview_ms_text;
    std::string offset_ms_text;
    std::vector<std::string> genres;
    std::vector<std::string> keywords;
    std::vector<timing_event> timing_events;
    bool reuse_existing_jacket_when_source_matches = false;
};

struct jacket_export_result {
    bool success = false;
    std::string filename;
    std::string error;
};

using jacket_exporter = std::function<jacket_export_result(const std::filesystem::path& source_path,
                                                           const std::filesystem::path& song_dir)>;

struct song_save_result {
    bool success = false;
    song_data song;
    std::string error;
};

song_save_result create_song(const song_form_data& form, const jacket_exporter& export_jacket);
song_save_result save_song_edits(const song_data& editing_song,
                                 const song_form_data& form,
                                 const jacket_exporter& export_jacket);

}  // namespace song_create
