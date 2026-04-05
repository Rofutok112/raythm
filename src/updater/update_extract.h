#pragma once

#include <filesystem>

namespace updater {

bool reset_directory(const std::filesystem::path& directory_path);
bool extract_zip_to_directory(const std::filesystem::path& zip_path, const std::filesystem::path& destination_directory);

}  // namespace updater
