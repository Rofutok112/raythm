#pragma once

#include <filesystem>

namespace updater {

std::filesystem::path update_root();
std::filesystem::path version_file_path();
std::filesystem::path downloads_root();
std::filesystem::path staging_root();
std::filesystem::path backup_root();
void ensure_update_directories();

}  // namespace updater
