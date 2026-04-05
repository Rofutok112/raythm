#include "updater/update_paths.h"

#include <system_error>

#include "app_paths.h"

namespace updater {

std::filesystem::path update_root() {
    return app_paths::app_data_root() / "updater";
}

std::filesystem::path version_file_path() {
    return update_root() / "version.json";
}

std::filesystem::path downloads_root() {
    return update_root() / "downloads";
}

std::filesystem::path staging_root() {
    return update_root() / "staged";
}

std::filesystem::path backup_root() {
    return update_root() / "backup";
}

void ensure_update_directories() {
    app_paths::ensure_directories();

    std::error_code ec;
    std::filesystem::create_directories(update_root(), ec);
    std::filesystem::create_directories(downloads_root(), ec);
    std::filesystem::create_directories(staging_root(), ec);
    std::filesystem::create_directories(backup_root(), ec);
}

}  // namespace updater
