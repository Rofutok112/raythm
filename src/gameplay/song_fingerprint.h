#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace song_fingerprint {

std::string build(std::string_view content);
std::optional<std::string> compute_sha256_hex(const std::filesystem::path& song_json_path);

}  // namespace song_fingerprint
