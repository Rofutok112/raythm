#pragma once

#include <filesystem>
#include <string>

namespace app_paths {

// Directory containing the running executable.
std::filesystem::path executable_dir();

// Asset root. Prefer executable-relative assets for distribution, fall back to source tree in dev.
std::filesystem::path assets_root();

// assets/audio/
std::filesystem::path audio_root();

// AppData/Local/raythm/
std::filesystem::path app_data_root();

// AppData/Local/raythm/songs/
std::filesystem::path songs_root();

// AppData/Local/raythm/charts/
std::filesystem::path charts_root();

// AppData/Local/raythm/songs/{song_id}/
std::filesystem::path song_dir(const std::string& song_id);

// AppData/Local/raythm/charts/{chart_id}.chart
std::filesystem::path chart_path(const std::string& chart_id);

// Legacy: repo_root/assets/songs/
std::filesystem::path legacy_songs_root();

// AppData/Local/raythm/settings.json
std::filesystem::path settings_path();

// AppData/Local/raythm/song_offsets.txt
std::filesystem::path song_offsets_path();

// Create songs/ and charts/ directories if they don't exist.
void ensure_directories();

}
