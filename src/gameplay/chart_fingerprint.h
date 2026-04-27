#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace chart_fingerprint {

std::string build(std::string_view content);
std::optional<std::string> compute_sha256_hex(const std::filesystem::path& chart_path);

}  // namespace chart_fingerprint
