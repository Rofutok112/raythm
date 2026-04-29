#include "app_paths.h"

#include <cstdlib>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::filesystem::path source_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

#ifdef _WIN32
std::filesystem::path executable_path() {
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::filesystem::current_path() / "raythm.exe";
    }
    return std::filesystem::path(buffer);
}
#else
std::filesystem::path executable_path() {
    return std::filesystem::current_path() / "raythm";
}
#endif

}

namespace app_paths {

std::filesystem::path executable_dir() {
    return executable_path().parent_path();
}

std::filesystem::path assets_root() {
    const std::filesystem::path bundled_assets = executable_dir() / "assets";
    if (std::filesystem::exists(bundled_assets) && std::filesystem::is_directory(bundled_assets)) {
        return bundled_assets;
    }

    const std::filesystem::path source_assets = source_root() / "assets";
    if (std::filesystem::exists(source_assets) && std::filesystem::is_directory(source_assets)) {
        return source_assets;
    }

    return bundled_assets;
}

std::filesystem::path audio_root() {
    return assets_root() / "audio";
}

std::filesystem::path app_data_root() {
    const char* local_app_data = std::getenv("LOCALAPPDATA");
    if (local_app_data != nullptr) {
        return std::filesystem::path(local_app_data) / "raythm";
    }
    return executable_dir() / "userdata";
}

std::filesystem::path songs_root() {
    return app_data_root() / "songs";
}

std::filesystem::path charts_root() {
    return app_data_root() / "charts";
}

std::filesystem::path rankings_root() {
    return app_data_root() / "rankings";
}

std::filesystem::path song_dir(const std::string& song_id) {
    return songs_root() / song_id;
}

std::filesystem::path chart_path(const std::string& chart_id) {
    return charts_root() / (chart_id + ".rchart");
}

std::filesystem::path local_ranking_path(const std::string& chart_id) {
    return rankings_root() / (chart_id + ".bin");
}

std::filesystem::path settings_path() {
    return app_data_root() / "settings.json";
}

std::filesystem::path chart_offsets_path() {
    return app_data_root() / "chart_offsets.txt";
}

std::filesystem::path auth_session_path() {
    return app_data_root() / "auth_session.json";
}

std::filesystem::path auth_device_path() {
    return app_data_root() / "auth_device.json";
}

std::filesystem::path upload_mapping_path() {
    return app_data_root() / "upload_mappings.txt";
}

std::filesystem::path local_content_db_path() {
    return app_data_root() / "local_content.db";
}

std::filesystem::path scoring_ruleset_cache_path() {
    return app_data_root() / "scoring_ruleset_cache.txt";
}

std::filesystem::path source_verification_cache_path() {
    return app_data_root() / "source_verification_cache.txt";
}

std::filesystem::path chart_identity_index_path() {
    return app_data_root() / "chart_identity_index.txt";
}

std::filesystem::path mvs_root() {
    return app_data_root() / "mvs";
}

std::filesystem::path mv_dir(const std::string& mv_id) {
    return mvs_root() / mv_id;
}

void ensure_directories() {
    std::error_code ec;
    std::filesystem::create_directories(app_data_root(), ec);
    std::filesystem::create_directories(songs_root(), ec);
    std::filesystem::create_directories(charts_root(), ec);
    std::filesystem::create_directories(rankings_root(), ec);
    std::filesystem::create_directories(mvs_root(), ec);
}

}
