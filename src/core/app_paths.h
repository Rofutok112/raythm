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

// AppData/Local/raythm/rankings/
std::filesystem::path rankings_root();

// AppData/Local/raythm/songs/{song_id}/
std::filesystem::path song_dir(const std::string& song_id);

// AppData/Local/raythm/charts/{chart_id}.rchart
std::filesystem::path chart_path(const std::string& chart_id);

// AppData/Local/raythm/rankings/{chart_id}.bin
std::filesystem::path local_ranking_path(const std::string& chart_id);

// AppData/Local/raythm/settings.json
std::filesystem::path settings_path();

// AppData/Local/raythm/chart_offsets.txt
std::filesystem::path chart_offsets_path();

// AppData/Local/raythm/auth_session.json
std::filesystem::path auth_session_path();

// AppData/Local/raythm/upload_mappings.txt
std::filesystem::path upload_mapping_path();

// AppData/Local/raythm/scoring_ruleset_cache.txt
std::filesystem::path scoring_ruleset_cache_path();

// AppData/Local/raythm/source_verification_cache.txt
std::filesystem::path source_verification_cache_path();

// AppData/Local/raythm/mvs/
std::filesystem::path mvs_root();

// AppData/Local/raythm/mvs/{mv_id}/
std::filesystem::path mv_dir(const std::string& mv_id);

// Create songs/ and charts/ directories if they don't exist.
void ensure_directories();

}
