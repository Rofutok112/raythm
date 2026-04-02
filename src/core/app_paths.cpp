#include "app_paths.h"

#include <cstdlib>

namespace {

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

}

namespace app_paths {

std::filesystem::path app_data_root() {
    const char* local_app_data = std::getenv("LOCALAPPDATA");
    if (local_app_data != nullptr) {
        return std::filesystem::path(local_app_data) / "raythm";
    }
    // Fallback to repo root if LOCALAPPDATA is not set.
    return repo_root();
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
    return repo_root() / "assets" / "songs";
}

void ensure_directories() {
    std::filesystem::create_directories(songs_root());
    std::filesystem::create_directories(charts_root());
}

}
