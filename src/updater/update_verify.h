#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace updater {

std::optional<std::string> compute_sha256_hex(const std::filesystem::path& file_path);
std::optional<std::string> parse_sha256sums_for_file(const std::string& checksums_content, const std::string& file_name);
bool verify_sha256_checksum(const std::filesystem::path& file_path, const std::filesystem::path& checksums_path);

}  // namespace updater
