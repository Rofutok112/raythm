#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace updater {

std::string file_name_from_url(const std::string& url, const std::string& fallback_name);
bool download_url_to_file(const std::string& url, const std::filesystem::path& destination_path);

}  // namespace updater
