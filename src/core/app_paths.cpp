#include "app_paths.h"

#include <cstdlib>

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

std::filesystem::path song_dir(const std::string& song_id) {
    return songs_root() / song_id;
}

std::filesystem::path chart_path(const std::string& chart_id) {
    return charts_root() / (chart_id + ".chart");
}

std::filesystem::path legacy_songs_root() {
    return assets_root() / "songs";
}

std::filesystem::path settings_path() {
    return app_data_root() / "settings.json";
}

std::filesystem::path song_offsets_path() {
    return app_data_root() / "song_offsets.txt";
}

void ensure_directories() {
    std::filesystem::create_directories(app_data_root());
    std::filesystem::create_directories(songs_root());
    std::filesystem::create_directories(charts_root());
}

}
